#include "porpoise_nav.h"

#include <arpa/inet.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define PACKET_SIZE 184u

static uint16_t get_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t get_le64(const uint8_t *p)
{
    return (uint64_t)get_le32(p) | ((uint64_t)get_le32(p + 4) << 32);
}

static int receive_packet(int fd, uint8_t packet[PACKET_SIZE])
{
    ssize_t n = recv(fd, packet, PACKET_SIZE, 0);
    return n == (ssize_t)PACKET_SIZE;
}

int main(void)
{
    int receiver;
    struct sockaddr_in address;
    struct timeval timeout = { 1, 0 };
    socklen_t address_size = sizeof address;
    uint8_t packet[PACKET_SIZE];
    pad_state state;

    receiver = socket(AF_INET, SOCK_DGRAM, 0);
    assert(receiver >= 0);
    assert(setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO,
                      &timeout, sizeof timeout) == 0);

    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(9980);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(receiver, (struct sockaddr *)&address, sizeof address) == 0);
    assert(getsockname(receiver, (struct sockaddr *)&address, &address_size) == 0);

    pad_state_reset(&state);
    state.buttons = 0x00010110u;
    state.lx = 17;
    state.ly = 220;
    state.rx = 111;
    state.ry = 129;
    state.l2 = 173;
    assert(navbridge_start());
    assert(navbridge_update(&state, UINT64_C(123456789)));
    assert(receive_packet(receiver, packet));
    assert(get_le32(packet) == UINT32_C(0x3142564d));
    assert(get_le16(packet + 4) == 2);
    assert(get_le16(packet + 6) == PACKET_SIZE);
    assert(get_le64(packet + 8) == 0);
    assert(get_le64(packet + 16) == UINT64_C(123456789));
    assert(get_le32(packet + 24) == 1);
    assert(get_le32(packet + 32) == state.buttons);
    assert(packet[36] == state.lx && packet[37] == state.ly);
    assert(packet[38] == state.rx && packet[39] == state.ry);
    assert(packet[40] == state.l2 && packet[41] == state.r2);
    assert(get_le32(packet + 56) == UINT32_C(0x3f800000)); /* qw = 1 */
    assert(get_le32(packet + 108) == 1); /* connected */
    assert(get_le64(packet + 112) == UINT64_C(123456789));

    navbridge_disconnect();
    assert(receive_packet(receiver, packet));
    assert(get_le64(packet + 8) == 1);
    assert(get_le32(packet + 24) == 0);
    assert(get_le32(packet + 32) == 0);
    assert(get_le32(packet + 108) == 0);

    state.buttons = 0x00010000u;
    state.l2 = 0;
    assert(navbridge_update(&state, UINT64_C(123456999)));
    assert(receive_packet(receiver, packet));
    assert(get_le64(packet + 8) == 2);
    assert(get_le32(packet + 24) == 1);
    assert(get_le32(packet + 32) == state.buttons);

    int receiver2=socket(AF_INET,SOCK_DGRAM,0);assert(receiver2>=0);
    struct sockaddr_in addr2={0};addr2.sin_family=AF_INET;addr2.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr2.sin_port=htons(9983);
    assert(bind(receiver2,(struct sockaddr*)&addr2,sizeof addr2)==0);assert(navbridge_player_start(3));
    state.buttons=0x4000;assert(navbridge_player_update(3,&state,123457000));assert(receive_packet(receiver2,packet));assert(get_le64(packet+8)==0&&get_le32(packet+32)==0x4000);
    navbridge_player_close(3);assert(receive_packet(receiver2,packet));assert(get_le32(packet+24)==0);close(receiver2);
    navbridge_close();
    assert(receive_packet(receiver, packet));
    assert(get_le64(packet + 8) == 3);
    assert(get_le32(packet + 24) == 0);
    close(receiver);
    puts("Porpoise Navigation UDP packet checks passed");
    return 0;
}
