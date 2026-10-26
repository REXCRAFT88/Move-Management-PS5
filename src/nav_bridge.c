#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "nav_bridge.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define NAV_PACKET_MAGIC UINT32_C(0x3142564d)
#define NAV_PACKET_VERSION 2u
#define NAV_PACKET_BYTES 184u
#define NAV_PACKET_PORT 9980u
#define NAV_PACKET_PAD_OFFSET 32u

typedef struct {
    int fd;
    uint64_t sequence;
    struct sockaddr_in destination;
} navbridge_context;

static navbridge_context g_navs[4]={{.fd=-1},{.fd=-1},{.fd=-1},{.fd=-1}};

static void put_le16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static void put_le64(uint8_t *out, uint64_t value)
{
    put_le32(out, (uint32_t)value);
    put_le32(out + 4, (uint32_t)(value >> 32));
}

static void put_le_float(uint8_t *out, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    put_le32(out, bits);
}

static int publish(int player,const pad_state *state, uint64_t monotonic_us)
{
    if(player<0||player>=4)return 0;
    navbridge_context *ctx=&g_navs[player];
    uint8_t packet[NAV_PACKET_BYTES] = { 0 };
    uint8_t *pad = packet + NAV_PACKET_PAD_OFFSET;
    ssize_t sent;

    if (ctx->fd < 0) return 0;
    if (!monotonic_us) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        monotonic_us=(uint64_t)ts.tv_sec*1000000u+(uint64_t)ts.tv_nsec/1000u;
    }


    put_le32(packet, NAV_PACKET_MAGIC);
    put_le16(packet + 4, NAV_PACKET_VERSION);
    put_le16(packet + 6, NAV_PACKET_BYTES);
    put_le64(packet + 8, ctx->sequence++);
    put_le64(packet + 16, monotonic_us);
    put_le32(packet + 24, state ? 1u : 0u);

    if (state) {
        put_le32(pad, state->buttons);
        pad[4] = state->lx;
        pad[5] = state->ly;
        pad[6] = state->rx;
        pad[7] = state->ry;
        pad[8] = state->l2;
        pad[9] = state->r2;
        put_le_float(pad + 24, 1.0f); /* neutral orientation; no Nav IMU */
        put_le32(pad + 76, 1u);
        put_le64(pad + 80, monotonic_us);
    }

    sent = sendto(ctx->fd, packet, sizeof packet, 0,
                  (const struct sockaddr *)&ctx->destination,
                  sizeof ctx->destination);
    return sent == (ssize_t)sizeof packet;
}

int navbridge_player_start(int player)
{
    if(player<0||player>=4)return 0;
    navbridge_context *ctx=&g_navs[player];
    if (ctx->fd >= 0) return 1;
    ctx->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->fd < 0) return 0;

    memset(&ctx->destination, 0, sizeof ctx->destination);
    ctx->destination.sin_family = AF_INET;
#if defined(__ORBIS__) || defined(__FreeBSD__) || defined(__APPLE__)
    ctx->destination.sin_len = sizeof ctx->destination;
#endif
    ctx->destination.sin_port = htons(NAV_PACKET_PORT+player);
    ctx->destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return 1;
}

int navbridge_player_update(int player,const pad_state*state,uint64_t us){return state?publish(player,state,us):0;}
void navbridge_player_close(int player){if(player<0||player>=4)return;navbridge_context*ctx=&g_navs[player];if(ctx->fd<0)return;(void)publish(player,NULL,0);close(ctx->fd);ctx->fd=-1;}
int navbridge_start(void){return navbridge_player_start(0);}
int navbridge_update(const pad_state*s,uint64_t us){return navbridge_player_update(0,s,us);}
void navbridge_disconnect(void){(void)publish(0,NULL,0);}
void navbridge_close(void){for(int p=0;p<4;p++)navbridge_player_close(p);}
