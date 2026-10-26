#include "porpoise_move.h"

#include <arpa/inet.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define PACKET_SIZE 184u

static uint32_t get_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t get_le64(const uint8_t *p)
{
    return (uint64_t)get_le32(p) | ((uint64_t)get_le32(p + 4) << 32);
}

static float get_le_float(const uint8_t *p)
{
    uint32_t bits = get_le32(p);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static void set_cal_value(uint8_t *cal, unsigned offset, int signed_value)
{
    unsigned value = (unsigned)(signed_value + 0x8000);
    cal[offset] = (uint8_t)value;
    cal[offset + 1] = (uint8_t)(value >> 8);
}

static void make_calibration(uint8_t cal[143])
{
    static const unsigned low_offset[3] = { 4+6*1, 4+6*5+2, 4+6*2+4 };
    static const unsigned high_offset[3] = { 4+6*3, 4+6*4+2, 4+6*0+4 };
    static const unsigned bias_offset[3] = { 0x2a, 0x2c, 0x2e };
    static const unsigned rate_offset[3] = { 0x46+8*0, 0x46+8*1+2, 0x46+8*2+4 };
    static const int low[3] = { -1000, -2000, -3000 };
    static const int high[3] = { 1000, 2000, 3000 };
    unsigned axis;
    memset(cal, 0, 143);
    for (axis = 0; axis < 3; ++axis) {
        set_cal_value(cal, low_offset[axis], low[axis]);
        set_cal_value(cal, high_offset[axis], high[axis]);
        set_cal_value(cal, bias_offset[axis], 100 + (int)axis * 10);
        set_cal_value(cal, rate_offset[axis], 1100 + (int)axis * 10);
    }
}

static int receive_packet(int fd, uint8_t packet[PACKET_SIZE])
{
    ssize_t n = recv(fd, packet, PACKET_SIZE, 0);
    return n == (ssize_t)PACKET_SIZE;
}


int main(void)
{
    uint8_t cal[143], packet[PACKET_SIZE];
    int receiver;
    struct sockaddr_in address;
    struct timeval timeout = { 1, 0 };
    socklen_t address_size = sizeof address;
    pad_state state;

    receiver = socket(AF_INET, SOCK_DGRAM, 0);
    assert(receiver >= 0);
    assert(setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO,
                      &timeout, sizeof timeout) == 0);
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(9978); /* slot 2 has its own channel */
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(receiver, (struct sockaddr *)&address, sizeof address) == 0);
    assert(getsockname(receiver, (struct sockaddr *)&address, &address_size) == 0);

    make_calibration(cal);
    assert(!movebridge_start(-1, cal));
    assert(!movebridge_start(0, NULL));
    {
        uint8_t bad[143];
        memset(bad, 0, sizeof bad); /* all calibration spans degenerate */
        assert(!movebridge_start(0, bad));
    }
    assert(movebridge_start(2, cal));
    int camera=socket(AF_INET,SOCK_DGRAM,0);assert(camera>=0);
    uint8_t optical[176]={0};memcpy(optical,"EYE1",4);optical[4]=1;optical[6]=176;
    uint32_t width=320,height=240;uint64_t stamp=987654320;
    memcpy(optical+16,&stamp,8);memcpy(optical+24,&width,4);memcpy(optical+28,&height,4);
    uint8_t*d=optical+32+24*3;uint32_t valid=1,area=100;float position[4]={100,120,10,10};
    memcpy(d,&valid,4);memcpy(d+4,&area,4);memcpy(d+8,position,16);
    struct sockaddr_in eye_addr={0};eye_addr.sin_family=AF_INET;eye_addr.sin_port=htons(9990);eye_addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(sendto(camera,optical,sizeof optical,0,(struct sockaddr*)&eye_addr,sizeof eye_addr)==176);

    pad_state_reset(&state);
    state.buttons = PAD_L2 | PAD_CREATE | PAD_OPTIONS | PAD_TOUCHPAD;
    state.l2 = 255;
    state.lx = 9; state.ly = 250; state.rx = 70; state.ry = 180;
    state.has_motion = 1;
    state.accel[0] = 0; state.accel[1] = 1000; state.accel[2] = -3000;
    state.gyro[0] = 0; state.gyro[1] = 500; state.gyro[2] = 1000;
    assert(movebridge_update(2, &state, UINT64_C(987654321)));
    assert(receive_packet(receiver, packet));
    assert(get_le32(packet) == UINT32_C(0x3142564d));
    assert(packet[4] == 2 && packet[6] == 184);
    assert(get_le64(packet + 8) == 0);
    assert(get_le64(packet + 16) == UINT64_C(987654321));
    assert(get_le32(packet + 24) == 1);
    assert(get_le32(packet+176)==1 && get_le64(packet+168)==987654320);
    assert(get_le_float(packet+152)==100 && get_le_float(packet+156)==120);
    {float bounds[4];assert(movebridge_optical_status(2,987654321,bounds));assert(bounds[0]==100);
     assert(!movebridge_optical_status(0,987654321,bounds));
     assert(!movebridge_optical_status(2,987854321,bounds));
     assert(!movebridge_optical_status(2,987654319,bounds));}


    assert(get_le32(packet + 32) ==
           ((PAD_L2 | PAD_CREATE | PAD_OPTIONS) | UINT32_C(0x00020000)));
    assert(!(get_le32(packet + 32) & PAD_TOUCHPAD));
    assert(packet[36] == 9 && packet[37] == 250 &&
           packet[38] == 70 && packet[39] == 180);
    assert(packet[40] == 255 && packet[41] == 0);
    assert(get_le32(packet + 56) == UINT32_C(0x3f800000));
    assert(fabsf(get_le_float(packet + 60) - 0.0f) < 0.0001f);
    assert(fabsf(get_le_float(packet + 64) - -1.0f) < 0.0001f);
    assert(fabsf(get_le_float(packet + 68) - -0.5f) < 0.0001f);
    assert(fabsf(get_le_float(packet + 72) - 0.0f) < 0.0001f);
    assert(fabsf(get_le_float(packet + 76) - 8.377580f) < 0.001f);
    assert(fabsf(get_le_float(packet + 80) - -4.188790f) < 0.001f);
    assert(get_le32(packet + 108) == 1);
    assert(get_le64(packet + 112) == UINT64_C(987654321));

    pad_state calibrated;pad_state_reset(&calibrated);
    assert(movebridge_motion(2,&state,&calibrated));assert(calibrated.calibrated_motion);
    for(int axis=0;axis<3;axis++){
        assert(fabsf(calibrated.acceleration_g[axis]-get_le_float(packet+60+axis*4))<0.0001f);
        assert(fabsf(calibrated.angular_velocity[axis]-get_le_float(packet+72+axis*4))<0.0001f);
    }
    assert(!movebridge_motion(0,&state,&calibrated));

    /* T release sends both digital-up and the analog zero through the packet. */
    state.buttons &= ~PAD_L2;
    state.l2 = 0;
    assert(movebridge_update(2, &state, UINT64_C(987654999)));
    assert(receive_packet(receiver, packet));
    assert(get_le64(packet + 8) == 1);
    assert(get_le32(packet + 32) ==
           (PAD_CREATE | PAD_OPTIONS | UINT32_C(0x00020000)));
    assert(packet[40] == 0);

    movebridge_disconnect(2);
    assert(receive_packet(receiver, packet));
    assert(get_le64(packet + 8) == 2 && get_le32(packet + 24) == 0);
    assert(get_le32(packet + 32) == 0 && get_le32(packet + 108) == 0);
    assert(!movebridge_update(2,&state,987655000));
    assert(movebridge_start(2,cal)); /* synthetic manufacturing offsets */
    state.accel[0]=-1000;state.accel[1]=2000;state.accel[2]=3000;
    state.gyro[0]=0;state.gyro[1]=0;state.gyro[2]=0;
    assert(movebridge_update(2,&state,987655000));
    assert(receive_packet(receiver,packet));
    assert(fabsf(get_le_float(packet+60)+1.0f)<0.0001f);
    assert(fabsf(get_le_float(packet+64)-1.0f)<0.0001f);
    assert(fabsf(get_le_float(packet+68)+1.0f)<0.0001f);
    assert(fabsf(get_le_float(packet+72))<0.0001f);
    assert(fabsf(get_le_float(packet+76))<0.0001f);
    assert(fabsf(get_le_float(packet+80))<0.0001f);
    state.gyro[0]=1000;state.gyro[1]=1000;state.gyro[2]=1000;
    assert(movebridge_update(2,&state,987655001));
    assert(receive_packet(receiver,packet));
    for(int axis=0;axis<3;++axis)assert(fabsf(get_le_float(packet+72+4*axis)-(axis==2?-8.377580f:8.377580f))<0.001f);
    /* Highest physical slot routes to an existing four-player output. */
    assert(movebridge_start(7,cal));
    assert(!movebridge_start(8,cal));
    assert(movebridge_route(7,2,0));
    uint8_t extended[224]={0};memcpy(extended,"EYE1",4);extended[4]=2;extended[6]=224;
    stamp=987654322;memcpy(extended+16,&stamp,8);memcpy(extended+24,&width,4);memcpy(extended+28,&height,4);
    d=extended+32+7*24;position[0]=44;position[1]=55;
    memcpy(d,&valid,4);memcpy(d+4,&area,4);memcpy(d+8,position,16);
    assert(sendto(camera,extended,sizeof extended,0,(struct sockaddr*)&eye_addr,sizeof eye_addr)==224);
    movebridge_set_color(7,7);
    assert(movebridge_update(7,&state,987654322));
    assert(recv(receiver,packet,sizeof packet,0)==PACKET_SIZE);
    float high_bounds[4];assert(movebridge_optical_status(7,987654322,high_bounds));assert(high_bounds[0]==44&&high_bounds[1]==55);
    /* Stationary zeroing changes only runtime bias and rejects movement. */
    pad_state still,corrected;pad_state_reset(&still);still.has_motion=1;
    still.gyro[0]=5;still.gyro[1]=-4;still.gyro[2]=3;
    unsigned progress=0;
    assert(movebridge_calibrate(7,0,1000000));
    for(unsigned sample=0;sample<=300;sample++)movebridge_calibration_sample(7,&still,1000000+(uint64_t)sample*10000);
    assert(movebridge_calibration_status(7,4000000,&progress)==2&&progress==100);
    assert(movebridge_motion(7,&still,&corrected));
    for(int a=0;a<3;a++)assert(fabsf(corrected.angular_velocity[a])<0.0001f);
    assert(movebridge_calibrate(7,0,5000000));
    pad_state moving=still;moving.gyro[0]=1000;
    movebridge_calibration_sample(7,&moving,5010000);
    assert(movebridge_calibration_status(7,5010000,&progress)==3);
    assert(movebridge_motion(7,&still,&corrected));
    for(int a=0;a<3;a++)assert(fabsf(corrected.angular_velocity[a])<0.0001f);
    assert(movebridge_calibrate(7,0,6000000));
    assert(movebridge_calibration_status(7,7100000,&progress)==3); /* silent device */
    assert(movebridge_calibrate(7,1,8000000));
    assert(movebridge_calibration_status(7,8000000,&progress)==0);
    assert(movebridge_motion(7,&still,&corrected)&&fabsf(corrected.angular_velocity[0])>0.001f);
    movebridge_disconnect(7);
    assert(movebridge_calibration_status(7,8000000,&progress)==-1);
    movebridge_close();
    close(camera);
    close(receiver);
    puts("Porpoise Move calibrated UDP packet checks passed");
    return 0;
}
