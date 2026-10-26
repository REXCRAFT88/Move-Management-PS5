/* SPDX-License-Identifier: GPL-3.0-or-later
 * PS3 Eye multi-color optical producer; no Move access or native pad API.
 * OV534/OV772x tables derive from inspirit/PS3EYEDriver and Linux gspca
 * (GPL-2.0-or-later). Streams detections over loopback UDP and never saves
 * camera images.
 */
#include <dlfcn.h>
#include <libusb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "eye-registers.h"
#include "eye-multisphere.h"
#include "eye-optical-packet.h"
#include "eye-preview.h"
#include "uvc-camera.h"
#include "phone-camera.h"
#include "phone-source.h"
#include "rgb-normalize.h"
#include "ps5-usb.h"
static uint8_t preview_frame[EYE_PREVIEW_BYTES];
static uint64_t last_preview_us;

#define FRAME_BYTES (640u * 480u)
#define FRAME_WIDTH 640u
#define FRAME_HEIGHT 480u
#define STOP_FILE "/data/porpoise-eye-optical.stop"

static FILE *logfile;
static libusb_device_handle *eye;
static int socket_fd = -1;
static struct sockaddr_in destination;
static int failed;
static int claimed;
static volatile sig_atomic_t stop_requested;
static int (*control_transfer)(libusb_device_handle *, uint8_t, uint8_t,
                               uint16_t, uint16_t, unsigned char *, uint16_t,
                               uint32_t);
static int (*bulk_transfer)(libusb_device_handle *, unsigned char,
                            unsigned char *, int, int *, uint32_t);
static void (*close_device)(libusb_device_handle *);
static int (*release_interface)(libusb_device_handle *, int);
static unsigned char frame[FRAME_BYTES];
static unsigned char alternate_phase_frame[FRAME_BYTES];
static unsigned char transfer_buffer[65536];
static size_t frame_used;
static uint32_t previous_pts;
static int previous_fid = -1;
static int current_frame_valid;
static unsigned complete_frames;
static unsigned invalid_payloads;
static uint64_t packet_sequence;
static uint64_t last_log_us;
static uint64_t last_packet_us;
static uint64_t last_phase_diag_us;

static void cap_log(void) {
    long position = ftell(logfile);
    if (position <= 1024L * 1024L) return;
    if (ftruncate(fileno(logfile), 0) != 0 || fseek(logfile, 0, SEEK_SET) != 0)
        return;
    fprintf(logfile, "PS3 Eye optical producer pid=%d port=%u (log rotated)\n",
            getpid(), EYE_OPTICAL_PORT);
}

static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

static void on_stop(int sig) {
    (void)sig;
    stop_requested = 1;
}

static void write_register(uint16_t reg, uint8_t value) {
    if (failed || stop_requested) return;
    int rc = control_transfer(eye, 0x40, 1, 0, reg, &value, 1, 100);
    if (rc != 1) {
        fprintf(logfile, "register_write_failed reg=%#x rc=%d\n", reg, rc);
        failed = 1;
    }
}

static uint8_t read_register(uint16_t reg) {
    uint8_t value = 0;
    if (failed || stop_requested) return 0;
    int rc = control_transfer(eye, 0xc0, 1, 0, reg, &value, 1, 100);
    if (rc != 1) {
        fprintf(logfile, "register_read_failed reg=%#x rc=%d\n", reg, rc);
        failed = 1;
    }
    return value;
}

static int wait_sensor(void) {
    for (int i = 0; i < 5 && !failed && !stop_requested; ++i) {
        uint8_t status = read_register(OV534_REG_STATUS);
        if (status == 0) return 1;
        if (status == 4) break;
    }
    fputs("sensor_operation_failed\n", logfile);
    failed = 1;
    return 0;
}

static void sensor_write(uint8_t reg, uint8_t value) {
    write_register(OV534_REG_SUBADDR, reg);
    write_register(OV534_REG_WRITE, value);
    write_register(OV534_REG_OPERATION, OV534_OP_WRITE_3);
    (void)wait_sensor();
}

static uint8_t sensor_read(uint8_t reg) {
    write_register(OV534_REG_SUBADDR, reg);
    write_register(OV534_REG_OPERATION, OV534_OP_WRITE_2);
    (void)wait_sensor();
    write_register(OV534_REG_OPERATION, OV534_OP_READ_2);
    (void)wait_sensor();
    return read_register(OV534_REG_READ);
}

static void write_register_table(const uint8_t table[][2], size_t count) {
    for (size_t i = 0; i < count && !failed && !stop_requested; ++i)
        write_register(table[i][0], table[i][1]);
}

static void write_sensor_table(const uint8_t table[][2], size_t count) {
    for (size_t i = 0; i < count && !failed && !stop_requested; ++i) {
        if (table[i][0] != 255) {
            sensor_write(table[i][0], table[i][1]);
        } else {
            (void)sensor_read(table[i][1]);
            write_register(255, 0);
        }
    }
}

static const char *color_name(unsigned color) {
    static const char *names[EYE_COLOR_COUNT] = {
        "blue", "red", "green", "yellow", "magenta", "cyan", "orange", "violet"
    };
    return color < EYE_COLOR_COUNT ? names[color] : "unknown";
}

static void detect_alternate_phase(struct eye_color_detection out[EYE_COLOR_COUNT]) {
    /* Current GRBG interpretation is G,R / B,G. For the alternate
     * R,G / G,B phase, remap source phase 1->Gtop, 3->R, 0->B, 2->Gbottom
     * into a synthetic buffer accepted by the existing detector. */
    for (unsigned y = 0; y < EYE_MULTI_HEIGHT; ++y) {
        for (unsigned x = 0; x < EYE_MULTI_WIDTH; ++x) {
            size_t at = (size_t)y * 1280u + (size_t)x * 2u;
            alternate_phase_frame[at] = frame[at + 1u];
            alternate_phase_frame[at + 1u] = frame[at + 641u];
            alternate_phase_frame[at + 640u] = frame[at];
            alternate_phase_frame[at + 641u] = frame[at + 640u];
        }
    }
    (void)eye_detect_colors(alternate_phase_frame,
                            sizeof(alternate_phase_frame), out);
}

static int phase_means(const struct eye_color_detection *d,
                       uint32_t means[4]) {
    unsigned x0, y0, x1, y1, count = 0;
    uint64_t sums[4] = { 0, 0, 0, 0 };
    if (!d->valid || d->width < 1.0f || d->height < 1.0f) return 0;
    x0 = d->x > d->width * 0.5f ?
         (unsigned)(d->x - d->width * 0.5f) : 0;
    y0 = d->y > d->height * 0.5f ?
         (unsigned)(d->y - d->height * 0.5f) : 0;
    x1 = (unsigned)(d->x + d->width * 0.5f + 1.0f);
    y1 = (unsigned)(d->y + d->height * 0.5f + 1.0f);
    if (x1 > EYE_MULTI_WIDTH) x1 = EYE_MULTI_WIDTH;
    if (y1 > EYE_MULTI_HEIGHT) y1 = EYE_MULTI_HEIGHT;
    for (unsigned y = y0; y < y1; ++y) {
        for (unsigned x = x0; x < x1; ++x) {
            size_t at = (size_t)y * 1280u + (size_t)x * 2u;
            sums[0] += frame[at];
            sums[1] += frame[at + 1u];
            sums[2] += frame[at + 640u];
            sums[3] += frame[at + 641u];
            ++count;
        }
    }
    if (!count) return 0;
    for (unsigned phase = 0; phase < 4; ++phase)
        means[phase] = (uint32_t)(sums[phase] / count);
    return 1;
}

static void log_alt_detection(FILE *f, const char *name,
                              const struct eye_color_detection *d) {
    if (d->valid)
        fprintf(f, " alt_%s=area:%u,x:%.1f,y:%.1f", name,
                d->area, d->x, d->y);
    else
        fprintf(f, " alt_%s=none", name);
}

static void log_phase_diagnostic(
    uint64_t timestamp,
    const struct eye_color_detection current[EYE_COLOR_COUNT]) {
    struct eye_color_detection alternate[EYE_COLOR_COUNT];
    uint32_t means[4];
    if (timestamp - last_phase_diag_us < UINT64_C(1000000)) return;
    last_phase_diag_us = timestamp;
    detect_alternate_phase(alternate);
    fprintf(logfile, "phase_diag frame=%u phase_order=G0,R1,B2,G3", complete_frames);
    log_alt_detection(logfile, "magenta", &alternate[EYE_COLOR_MAGENTA]);
    log_alt_detection(logfile, "cyan", &alternate[EYE_COLOR_CYAN]);
    log_alt_detection(logfile, "red", &alternate[EYE_COLOR_RED]);
    if (phase_means(&current[EYE_COLOR_RED], means))
        fprintf(logfile, " current_red_phase_means=%u,%u,%u,%u",
                means[0], means[1], means[2], means[3]);
    else
        fputs(" current_red_phase_means=none", logfile);
    if (phase_means(&current[EYE_COLOR_GREEN], means))
        fprintf(logfile, " current_green_phase_means=%u,%u,%u,%u",
                means[0], means[1], means[2], means[3]);
    else
        fputs(" current_green_phase_means=none", logfile);
    fputc('\n', logfile);
}

static void report_frame(void) {
    struct eye_color_detection detections[EYE_COLOR_COUNT];
    uint8_t packet[EYE_OPTICAL_PACKET_BYTES];
    uint64_t timestamp = now_us();
    static uint64_t exclusions_loaded;
    if(timestamp-exclusions_loaded>=1000000){eye_exclusion_count=camera_exclusions_load(eye_exclusions);exclusions_loaded=timestamp;}
    if (!eye_detect_colors(frame, sizeof(frame), detections)) {
        fputs("detector_rejected_frame\n", logfile);
        return;
    }
    ++complete_frames;
    if(socket_fd>=0 && timestamp-last_preview_us>=200000){
        struct sockaddr_in preview_destination=destination;
        preview_destination.sin_port=htons(EYE_PREVIEW_PORT);
        eye_preview_build(preview_frame,frame);
        (void)sendto(socket_fd,preview_frame,sizeof preview_frame,0,(const struct sockaddr*)&preview_destination,sizeof preview_destination);
        last_preview_us=timestamp;
    }
#ifdef EYE_DEBUG_SINGLE_FRAME
    if(complete_frames==30){FILE*f=fopen("/data/eye-optical-debug.raw","wb");if(f){size_t saved=fwrite(frame,1,sizeof frame,f);fclose(f);fprintf(logfile,"diagnostic_frame_saved=%zu\n",saved);}}
#endif

    eye_optical_packet_build(packet, packet_sequence++, timestamp, detections);
    if (socket_fd >= 0) {
        ssize_t sent = sendto(socket_fd, packet, sizeof packet, 0,
                              (const struct sockaddr *)&destination,
                              sizeof destination);
        if (sent != (ssize_t)sizeof packet) {
            fprintf(logfile, "udp_send_failed=%d\n", (int)sent);
        } else {
            last_packet_us = timestamp;
        }
    }
    if (timestamp - last_log_us >= UINT64_C(1000000)) {
        fprintf(logfile, "frames=%u seq=%llu", complete_frames,
                (unsigned long long)(packet_sequence - 1));
        for (unsigned color = 0; color < EYE_COLOR_COUNT; ++color) {
            const struct eye_color_detection *d = &detections[color];
            if (!d->valid) continue;
            fprintf(logfile, " %s=area:%u,x:%.1f,y:%.1f,w:%.1f,h:%.1f",
                    color_name(color), d->area, d->x, d->y, d->width, d->height);
        }
        fputc('\n', logfile);
        last_log_us = timestamp;
    }
    log_phase_diagnostic(timestamp, detections);
}

static int stop_file_exists(void) {
    FILE *f = fopen(STOP_FILE, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static void consume_payloads(const unsigned char *data, size_t length) {
    while (length && !failed && !stop_requested) {
        size_t chunk = length > 2048u ? 2048u : length;
        if (chunk < 12 || data[0] != 12 || (data[1] & 0x40) || !(data[1] & 0x04)) {
            current_frame_valid = 0;
            ++invalid_payloads;
            goto next_payload;
        }

        uint32_t pts = (uint32_t)data[2] | ((uint32_t)data[3] << 8) |
                       ((uint32_t)data[4] << 16) | ((uint32_t)data[5] << 24);
        int fid = data[1] & 1;
        if (previous_fid != fid || previous_pts != pts) {
            frame_used = 0;
            current_frame_valid = 1;
            previous_fid = fid;
            previous_pts = pts;
        }
        if (current_frame_valid) {
            size_t payload = chunk - 12u;
            if (payload > sizeof(frame) - frame_used) {
                current_frame_valid = 0;
                ++invalid_payloads;
                goto next_payload;
            }
            memcpy(frame + frame_used, data + 12, payload);
            frame_used += payload;
            if (data[1] & 0x02) {
                if (frame_used == sizeof(frame)) report_frame();
                else ++invalid_payloads;
                current_frame_valid = 0;
            }
        }
next_payload:
        data += chunk;
        length -= chunk;
    }
}

static void stop_stream(void) {
    if (!eye || !claimed || !control_transfer) return;
    /* Match the previously working PS3 Eye shutdown sequence. */
    failed = 0;
    write_register(0xe0, 9);
    write_register(0x21, read_register(0x21) & 0x7f);
}

static void cleanup(void) {
    if (socket_fd >= 0 && last_packet_us) {
        struct eye_color_detection invalid[EYE_COLOR_COUNT] = {{0}};
        uint8_t packet[EYE_OPTICAL_PACKET_BYTES];
        eye_optical_packet_build(packet, packet_sequence++, now_us(), invalid);
        (void)sendto(socket_fd, packet, sizeof packet, 0,
                     (const struct sockaddr *)&destination, sizeof destination);
    }
    stop_stream();
    if (eye) {
        if (claimed) {
            fprintf(logfile, "release_interface=%d\n", release_interface(eye, 0));
            claimed = 0;
        }
        close_device(eye);
        eye = NULL;
        fputs("camera_close=done\n", logfile);
    }
    if (socket_fd >= 0) {
        close(socket_fd);
        socket_fd = -1;
    }
}

static void camera_status(const char *source,const char *message){
 if(logfile)fprintf(logfile,"camera_source=%s message=%s uvc=%s\n",source,message,camera_usb_load()?"ready":camera_usb_error());
 FILE*f=fopen("/data/move-studio/camera.json.tmp","w");
 if(f){fprintf(f,"{\"source\":\"%s\",\"message\":\"%s\",\"uvc_available\":%s,\"uvc_status\":\"%s\"}",source,message,camera_usb_load()?"true":"false",camera_usb_error());fclose(f);rename("/data/move-studio/camera.json.tmp","/data/move-studio/camera.json");}
}
static int camera_stopped(void){return stop_requested||stop_file_exists();}
static int local_camera_stopped(void){return camera_stopped()||camera_phone_requested();}
static void rgb_frame(const uint8_t *rgb,unsigned width,unsigned height,size_t stride){
 if(camera_rgb_normalize(frame,rgb,width,height,stride)){eye_multi_rgb_source=1;report_frame();}
}
static int eye_session(void) {
    failed=0;claimed=0;frame_used=0;previous_fid=-1;previous_pts=0;current_frame_valid=0;
    uint64_t next_uvc_check=now_us()+3000000;

    int usbd_initialized = 0;
    logfile = fopen("/data/porpoise-eye-optical.log", "a");
    if (!logfile) return 1;
    setvbuf(logfile, NULL, _IOLBF, 0);
    fprintf(logfile, "PS3 Eye optical producer pid=%d port=%u\n",
            getpid(), EYE_OPTICAL_PORT);
    fputs("detector=daylight-core-v2\n",logfile);
    signal(SIGALRM, on_stop);
    signal(SIGTERM, on_stop);

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        fputs("udp_socket_failed\n", logfile);
        goto done;
    }
    int preview_send_buffer=262144;
    (void)setsockopt(socket_fd,SOL_SOCKET,SO_SNDBUF,&preview_send_buffer,sizeof preview_send_buffer);
    memset(&destination, 0, sizeof destination);
    destination.sin_family = AF_INET;
#if defined(__ORBIS__) || defined(__FreeBSD__) || defined(__APPLE__)
    destination.sin_len = sizeof destination;
#endif
    destination.sin_port = htons(EYE_OPTICAL_PORT);
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (stop_file_exists()) {
        fputs("stop_file_present_before_camera_open\n", logfile);
        goto done;
    }
    {
        struct eye_color_detection invalid[EYE_COLOR_COUNT] = {{0}};
        uint8_t packet[EYE_OPTICAL_PACKET_BYTES];
        uint64_t timestamp = now_us();
        eye_optical_packet_build(packet, packet_sequence++, timestamp, invalid);
        if (sendto(socket_fd, packet, sizeof packet, 0,
                   (const struct sockaddr *)&destination, sizeof destination) ==
            (ssize_t)sizeof packet)
            last_packet_us = timestamp;
    }

    void *lib = dlopen("/system/common/lib/libSceUsbd.sprx", RTLD_LAZY);
    if (!lib) {
        fprintf(logfile, "usbd_load_failed=%s\n", dlerror());
        goto done;
    }
    int (*usbd_init)(void) = dlsym(lib, "sceUsbdInit");
    void (*usbd_exit)(void) = dlsym(lib, "sceUsbdExit");
    libusb_device_handle *(*open_eye)(uint16_t, uint16_t) =
        dlsym(lib, "sceUsbdOpenDeviceWithVidPid");
    close_device = dlsym(lib, "sceUsbdClose");
    int (*claim_interface)(libusb_device_handle *, int) =
        dlsym(lib, "sceUsbdClaimInterface");
    release_interface = dlsym(lib, "sceUsbdReleaseInterface");
    control_transfer = dlsym(lib, "sceUsbdControlTransfer");
    bulk_transfer = dlsym(lib, "sceUsbdBulkTransfer");
    if (!usbd_init || !usbd_exit || !open_eye || !close_device ||
        !claim_interface || !release_interface || !control_transfer || !bulk_transfer) {
        fputs("missing_usb_exports\n", logfile);
        goto close_library;
    }

    int rc = usbd_init();
    fprintf(logfile, "usbd_init=%d\n", rc);
    if (rc != 0) goto exit_usbd;
    usbd_initialized = 1;
    if (stop_requested || stop_file_exists()) goto exit_usbd;
    eye = open_eye(0x1415, 0x2000);
    fprintf(logfile, "eye_open=%d\n", eye != NULL);
    if (!eye) goto exit_usbd;
    rc = claim_interface(eye, 0);
    fprintf(logfile, "camera_claim=%d\n", rc);
    if (rc != 0) goto cleanup_camera;
    claimed = 1;

    write_register(0xe7, 0x3a);
    write_register(0xe0, 8);
    usleep(100000);
    write_register(OV534_REG_ADDRESS, 0x42);
    sensor_write(0x12, 0x80);
    usleep(10000);
    (void)sensor_read(0x0a);
    unsigned sensor = (unsigned)sensor_read(0x0a) << 8;
    (void)sensor_read(0x0b);
    sensor |= sensor_read(0x0b);
    fprintf(logfile, "sensor=%#x failed=%d\n", sensor, failed);
    if ((sensor & 0xfff0u) != 0x7720u || failed || stop_requested)
        goto cleanup_camera;

    write_register_table(ov534_reg_initdata, ARRAY_SIZE(ov534_reg_initdata));
    write_register(0x21, read_register(0x21) | 0x80);
    write_sensor_table(ov772x_reg_initdata, ARRAY_SIZE(ov772x_reg_initdata));
    write_register(0xe0, 9);
    write_register(0x21, read_register(0x21) & 0x7f);
    write_register_table(bridge_start_vga, ARRAY_SIZE(bridge_start_vga));
    write_sensor_table(sensor_start_vga, ARRAY_SIZE(sensor_start_vga));
    /* PS3EYEDriver VGA 60FPS table: r11=00,r0d=41,re5=04. */
    sensor_write(0x11, 0x00);
    sensor_write(0x0d, 0x41);
    write_register(0xe5, 0x04);
    write_register(0xe0, 0);
    fprintf(logfile, "stream_start_failed=%d\n", failed);
    if (failed || stop_requested) goto cleanup_camera;

    camera_status("ps3eye","PS3 Eye active");
    unsigned transfers = 0;
    size_t bytes = 0;
    while (!failed && !stop_requested && !stop_file_exists()) {
        if(camera_phone_requested()){fputs("Phone source configured: switching camera\n",logfile);break;}
        if(now_us()>next_uvc_check){next_uvc_check=now_us()+30000000;if(camera_phone_requested()||camera_uvc_present()){fputs("UVC attached: switching camera\n",logfile);break;}}
        int received = 0;
        rc = bulk_transfer(eye, 0x81, transfer_buffer,
                           (int)sizeof(transfer_buffer), &received, 300);
        if (received < 0 || received > (int)sizeof(transfer_buffer)) {
            failed = 1;
            fputs("bulk_length_invalid\n", logfile);
            break;
        }
        if (received > 0) {
            bytes += (unsigned)received;
            consume_payloads(transfer_buffer, (size_t)received);
        }
        ++transfers;
        cap_log();
        if (rc != 0 && rc != -7) {
            fprintf(logfile, "bulk_transfer_failed=%d received=%d\n", rc, received);
            failed = 1;
        }
    }
    fprintf(logfile, "transfers=%u bytes=%zu\n", transfers, bytes);

cleanup_camera:
    cleanup();
exit_usbd:
    if (usbd_exit && usbd_initialized) {
        usbd_exit();
        fputs("usbd_exit=done\n", logfile);
    }
close_library:
    dlclose(lib);
done:
    if (socket_fd >= 0) {
        close(socket_fd);
        socket_fd = -1;
    }
    fprintf(logfile, "frames=%u invalid_payloads=%u transfers_complete\n",
            complete_frames, invalid_payloads);
    fputs("complete\n", logfile);
    fclose(logfile);logfile=NULL;
    return 0;
}

int main(void){
 unlink(STOP_FILE);
 signal(SIGTERM,on_stop);signal(SIGALRM,on_stop);signal(SIGPIPE,SIG_IGN);
 (void)camera_usb_load();
 while(!camera_stopped()){
  logfile=fopen("/data/porpoise-eye-optical.log","a");if(!logfile)return 1;setvbuf(logfile,NULL,_IOLBF,0);
  socket_fd=socket(AF_INET,SOCK_DGRAM,0);int buffer=262144;if(socket_fd>=0)setsockopt(socket_fd,SOL_SOCKET,SO_SNDBUF,&buffer,sizeof buffer);
  memset(&destination,0,sizeof destination);destination.sin_family=AF_INET;
#if defined(__ORBIS__) || defined(__FreeBSD__) || defined(__APPLE__)
  destination.sin_len=sizeof destination;
#endif
  destination.sin_port=htons(EYE_OPTICAL_PORT);destination.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  camera_status("waiting","Looking for USB webcam or PS3 Eye");
  if(camera_phone_requested())(void)camera_phone_run(camera_stopped,rgb_frame,camera_status);
  if(!camera_stopped())(void)camera_uvc_run(local_camera_stopped,rgb_frame,camera_status);
  if(socket_fd>=0)close(socket_fd);socket_fd=-1;fclose(logfile);logfile=NULL;
  if(camera_stopped())break;
  eye_multi_rgb_source=0;
  (void)eye_session();
  if(!camera_stopped()){camera_status("waiting","Connect PS3 Eye or enable phone USB Webcam mode");for(int i=0;i<20&&!camera_stopped();i++)usleep(100000);}
 }
 camera_status("stopped","Camera service stopped");return 0;
}
