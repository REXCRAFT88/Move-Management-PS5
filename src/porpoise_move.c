#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "porpoise_move.h"
#include "host.h"

#include <arpa/inet.h>
#include <math.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#define MOVE_PACKET_MAGIC UINT32_C(0x3142564d)
#define MOVE_PACKET_VERSION 2u
#define MOVE_PACKET_BYTES 184u
#define MOVE_PACKET_PAD_OFFSET 32u
#define MOVE_PACKET_PORT_BASE 9976u
#define MOVE_CAL_BYTES 143u

typedef struct {
    int fd;
    uint64_t sequence;
    struct sockaddr_in destination;
    float accel_scale[3];
    float accel_offset[3];
    float gyro_bias[3];
    float gyro_scale[3];
    int calibration_status;
    unsigned calibration_samples;
    uint64_t calibration_start,calibration_last;
    float calibration_sum[3],calibration_first_accel[3];
} movebridge_slot;

static movebridge_slot g_slots[HOST_MAX_PADS] = {
    { .fd = -1 }, { .fd = -1 }, { .fd = -1 }, { .fd = -1 },
    { .fd = -1 }, { .fd = -1 }, { .fd = -1 }, { .fd = -1 }
};

static unsigned optical_colors[HOST_MAX_PADS]={4,5,3,2,4,5,3,2};
static int optical_fd=-1;
static uint8_t optical_wire[224];
static uint64_t optical_time;
static uint32_t optical_u32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t optical_u64(const uint8_t*p){return optical_u32(p)|((uint64_t)optical_u32(p+4)<<32);}
static float optical_float(const uint8_t*p){uint32_t bits=optical_u32(p);float value;memcpy(&value,&bits,4);return value;}
static void optical_open(void){
    if(optical_fd>=0)return;
    int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return;
    struct sockaddr_in address={0};address.sin_family=AF_INET;
#if defined(__ORBIS__) || defined(__FreeBSD__) || defined(__APPLE__)
    address.sin_len=sizeof address;
#endif
    address.sin_port=htons(9990);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(fd,(const struct sockaddr*)&address,sizeof address)<0||fcntl(fd,F_SETFL,O_NONBLOCK)<0){close(fd);return;}
    optical_fd=fd;
}
static void optical_poll(uint64_t now){
    if(optical_fd<0)return;
    for(int attempt=0;attempt<8;attempt++){
        uint8_t wire[225];ssize_t size=recvfrom(optical_fd,wire,sizeof wire,0,NULL,NULL);if(size<0)break;
        if(!((size==176&&wire[4]==1)||(size==224&&wire[4]==2))||memcmp(wire,"EYE1",4)||wire[5]||wire[6]!=(unsigned)size||wire[7]||optical_u32(wire+24)!=320||optical_u32(wire+28)!=240)continue;
        uint64_t timestamp=optical_u64(wire+16);if(timestamp>now||now-timestamp>200000||timestamp<=optical_time)continue;
        int valid=1;
        for(int color=0;color<(size==224?8:6);color++){
            const uint8_t*d=wire+32+24*color;uint32_t present=optical_u32(d);if(present>1){valid=0;break;}
            if(!present)continue;
            float x=optical_float(d+8),y=optical_float(d+12),w=optical_float(d+16),h=optical_float(d+20);
            if(!__builtin_isfinite(x)||!__builtin_isfinite(y)||!__builtin_isfinite(w)||!__builtin_isfinite(h)||x<0||x>319||y<0||y>239||w<1||w>100||h<1||h>100||optical_u32(d+4)<4){valid=0;break;}
        }
        if(valid){memset(optical_wire,0,sizeof optical_wire);memcpy(optical_wire,wire,(size_t)size);optical_time=timestamp;}
    }
}

static uint16_t get_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int16_t calibration_value(const uint8_t *p)
{
    return (int16_t)((int)get_le16(p) - 0x8000);
}

static void put_le32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void put_le64(uint8_t *p, uint64_t value)
{
    put_le32(p, (uint32_t)value);
    put_le32(p + 4, (uint32_t)(value >> 32));
}

static void put_le_float(uint8_t *p, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    put_le32(p, bits);
}

static int read_calibration(movebridge_slot *slot, const uint8_t blob[MOVE_CAL_BYTES])
{
    /* ZCM1 six-pose accelerometer calibration, one LE offset-binary vector
     * per pose. Values below are the component for each axis. */
    static const uint8_t accel_low_offset[3] = { 0x0a, 0x24, 0x14 };
    static const uint8_t accel_high_offset[3] = { 0x16, 0x1e, 0x08 };
    static const uint8_t gyro_bias_offset[3] = { 0x2a, 0x2c, 0x2e };
    static const uint8_t gyro_80rpm_offset[3] = { 0x46, 0x50, 0x5a };
    unsigned axis;

    for (axis = 0; axis < 3; ++axis) {
        int low = calibration_value(blob + accel_low_offset[axis]);
        int high = calibration_value(blob + accel_high_offset[axis]);
        int bias = calibration_value(blob + gyro_bias_offset[axis]);
        int at_80rpm = calibration_value(blob + gyro_80rpm_offset[axis]);
        int span = at_80rpm - bias;
        if (high <= low || span == 0) return 0;
        slot->accel_scale[axis] = 2.0f / (float)(high - low);
        slot->accel_offset[axis] = -1.0f - slot->accel_scale[axis] * (float)low;
        /* Factory zero-rate values determine the 80RPM scale only.
         * ZCM1 runtime reports are already centred near zero; subtracting
         * the stored manufacturing bias again creates continuous rotation.
         * psmoveapi likewise uses dx=dy=dz=0 for ZCM1 runtime mapping. */
        slot->gyro_bias[axis] = 0.0f;
        slot->gyro_scale[axis] = (float)(80.0 * (2.0 * 3.14159265358979323846 / 60.0) /
                                                   (double)span);
        if (!__builtin_isfinite(slot->accel_scale[axis]) ||
            !__builtin_isfinite(slot->accel_offset[axis]) ||
            !__builtin_isfinite(slot->gyro_scale[axis])) return 0;
    }
    return 1;
}

static int publish(int index, const pad_state *state, uint64_t monotonic_us)
{
    movebridge_slot *slot = &g_slots[index];
    uint8_t packet[MOVE_PACKET_BYTES] = { 0 };
    uint8_t *pad = packet + MOVE_PACKET_PAD_OFFSET;
    uint32_t buttons;
    unsigned axis;
    ssize_t sent;

    if (slot->fd < 0) return 0;
    if (!monotonic_us) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        monotonic_us=(uint64_t)ts.tv_sec*1000000u+(uint64_t)ts.tv_nsec/1000u;
    }

    put_le32(packet, MOVE_PACKET_MAGIC);
    packet[4] = MOVE_PACKET_VERSION;
    packet[6] = (uint8_t)MOVE_PACKET_BYTES;
    put_le64(packet + 8, slot->sequence++);
    put_le64(packet + 16, monotonic_us);
    put_le32(packet + 24, state ? 1u : 0u);
    optical_poll(monotonic_us);
    if(state && optical_time && monotonic_us>=optical_time && monotonic_us-optical_time<=200000){

        const uint8_t*d=optical_wire+32+24*optical_colors[index];
        if(optical_u32(d)==1){memcpy(packet+152,d+8,16);put_le64(packet+168,optical_time);put_le32(packet+176,1);}
    }

    if (state) {
        /* In the native Move button map the Move button is 0x20000. AnyPad's
         * generic PAD_TOUCHPAD bit therefore maps there; Create/Select and
         * L2/T remain their ordinary button bits. */
        buttons = state->buttons & ~PAD_TOUCHPAD;
        if (state->buttons & PAD_TOUCHPAD) buttons |= UINT32_C(0x00020000);
        put_le32(pad, buttons);
        pad[4] = state->lx; pad[5] = state->ly;
        pad[6] = state->rx; pad[7] = state->ry;
        pad[8] = state->l2; pad[9] = state->r2;
        put_le_float(pad + 24, 1.0f); /* no orientation fusion */
        if (state->has_motion) {
            for (axis = 0; axis < 3; ++axis) {
                float accel_g = slot->accel_scale[axis] * (float)state->accel[axis] +
                                slot->accel_offset[axis];
                float gyro_rad_s = ((float)state->gyro[axis] - slot->gyro_bias[axis]) *
                                   slot->gyro_scale[axis];
                /* Move native Y follows the handle toward the sphere and
                 * Z faces out through the buttons. Pad coordinates expect
                 * Y through the face and Z back toward the player.
                 * Apply the same proper rotation to accel AND gyro. */
                static const unsigned destination_axis[3] = {0, 2, 1};
                float sign = axis == 1 ? -1.0f : 1.0f;
                put_le_float(pad + 28 + destination_axis[axis] * 4, sign * accel_g);
                put_le_float(pad + 40 + destination_axis[axis] * 4, sign * gyro_rad_s);
            }
        }
        put_le32(pad + 76, 1u);
        put_le64(pad + 80, monotonic_us);
    }
    sent = sendto(slot->fd, packet, sizeof packet, 0,
                  (const struct sockaddr *)&slot->destination,
                  sizeof slot->destination);
    return sent == (ssize_t)sizeof packet;
}

int movebridge_motion(int index,const pad_state *raw,pad_state *out){
    if(index<0||index>=HOST_MAX_PADS||!raw||!out||!raw->has_motion||g_slots[index].fd<0)return 0;
    movebridge_slot *slot=&g_slots[index];
    static const unsigned destination[3]={0,2,1};
    for(unsigned axis=0;axis<3;axis++){
        float sign=axis==1?-1.0f:1.0f;
        out->acceleration_g[destination[axis]]=sign*(slot->accel_scale[axis]*raw->accel[axis]+slot->accel_offset[axis]);
        out->angular_velocity[destination[axis]]=sign*((float)raw->gyro[axis]-slot->gyro_bias[axis])*slot->gyro_scale[axis];
    }
    out->calibrated_motion=1;return 1;
}

int movebridge_calibrate(int index,int reset,uint64_t now) {
    if(index<0||index>=HOST_MAX_PADS||g_slots[index].fd<0)return 0;
    movebridge_slot*s=&g_slots[index];
    s->calibration_samples=0;s->calibration_start=now;s->calibration_last=now;
    memset(s->calibration_sum,0,sizeof s->calibration_sum);
    s->calibration_status=reset?0:1;
    if(reset)memset(s->gyro_bias,0,sizeof s->gyro_bias);
    return 1;
}
void movebridge_calibration_sample(int index,const pad_state*raw,uint64_t now) {
    if(index<0||index>=HOST_MAX_PADS||!raw)return;
    movebridge_slot*s=&g_slots[index];
    if(s->fd<0||s->calibration_status!=1)return;
    if(!raw->has_motion||now<s->calibration_start||now-s->calibration_start>5000000||
       now<s->calibration_last||now-s->calibration_last>1000000){s->calibration_status=3;return;}
    if(s->calibration_samples&&now==s->calibration_last)return;
    s->calibration_last=now;
    for(unsigned a=0;a<3;a++) {
        float acceleration=raw->accel[a]*s->accel_scale[a]+s->accel_offset[a];
        if(!s->calibration_samples)s->calibration_first_accel[a]=acceleration;
        if(fabsf(raw->gyro[a]*s->gyro_scale[a])>0.15f||
           fabsf(acceleration-s->calibration_first_accel[a])>0.08f){s->calibration_status=3;return;}
    }
    for(unsigned a=0;a<3;a++)s->calibration_sum[a]+=raw->gyro[a];
    s->calibration_samples++;
    if(now-s->calibration_start>=3000000&&s->calibration_samples>=100){
        for(unsigned a=0;a<3;a++)s->gyro_bias[a]=s->calibration_sum[a]/s->calibration_samples;
        s->calibration_status=2;
    }
}
int movebridge_calibration_status(int index,uint64_t now,unsigned*percent) {
    if(percent)*percent=0;
    if(index<0||index>=HOST_MAX_PADS||g_slots[index].fd<0)return -1;
    movebridge_slot*s=&g_slots[index];
    if(s->calibration_status==1&&(now<s->calibration_start||now-s->calibration_start>5000000||now<s->calibration_last||now-s->calibration_last>1000000))s->calibration_status=3;
    if(percent&&s->calibration_status==1){uint64_t elapsed=now-s->calibration_start;*percent=(unsigned)(elapsed>=3000000?99:elapsed/30000);}
    if(percent&&s->calibration_status==2)*percent=100;
    return s->calibration_status;
}

int movebridge_start(int index, const uint8_t calibration[MOVE_CAL_BYTES])
{
    movebridge_slot *slot;
    if (index < 0 || index >= HOST_MAX_PADS || !calibration) return 0;
    slot = &g_slots[index];
    optical_open();
    if (slot->fd >= 0) return 1;
    slot->calibration_status=0;slot->calibration_samples=0;
    if (!read_calibration(slot, calibration)) return 0;
    slot->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (slot->fd < 0) return 0;
    memset(&slot->destination, 0, sizeof slot->destination);
    slot->destination.sin_family = AF_INET;
#if defined(__ORBIS__) || defined(__FreeBSD__) || defined(__APPLE__)
    slot->destination.sin_len = sizeof slot->destination;
#endif
    slot->destination.sin_port = htons((uint16_t)(index<4?MOVE_PACKET_PORT_BASE+index:0));
    slot->destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return 1;
}

int movebridge_update(int index, const pad_state *state, uint64_t monotonic_us)
{
    if (index < 0 || index >= HOST_MAX_PADS || !state) return 0;
    return publish(index, state, monotonic_us);
}

void movebridge_disconnect(int index)
{
    if (index >= 0 && index < HOST_MAX_PADS && g_slots[index].fd >= 0) {
        (void)publish(index, NULL, 0);
        close(g_slots[index].fd);
        g_slots[index].fd = -1;
    }
}

void movebridge_close(void)
{
    if(optical_fd>=0){close(optical_fd);optical_fd=-1;}
    optical_time=0;
    int index;
    for (index = 0; index < HOST_MAX_PADS; ++index) {
        if (g_slots[index].fd >= 0) {
            (void)publish(index, NULL, 0);
            close(g_slots[index].fd);
            g_slots[index].fd = -1;
        }
    }
}

int movebridge_optical_status(int slot,uint64_t now,float bounds[4]){

    if(slot<0||slot>=HOST_MAX_PADS||g_slots[slot].fd<0||!bounds||!optical_time||now<optical_time||now-optical_time>200000)return 0;
    const uint8_t*d=optical_wire+32+24*optical_colors[slot];if(optical_u32(d)!=1)return 0;
    for(int i=0;i<4;i++)bounds[i]=optical_float(d+8+4*i);
    return 1;
}

int movebridge_route(int index,int player,int secondary){if(index<0||index>=HOST_MAX_PADS||player<0||player>=4||g_slots[index].fd<0)return 0;(void)publish(index,NULL,0);g_slots[index].destination.sin_port=htons((secondary?10000:9976)+player);return 1;}

void movebridge_invalidate(int slot){if(slot>=0&&slot<HOST_MAX_PADS)(void)publish(slot,NULL,0);}

void movebridge_set_color(int slot,unsigned color){if(slot>=0&&slot<HOST_MAX_PADS&&color<8)optical_colors[slot]=color;}
