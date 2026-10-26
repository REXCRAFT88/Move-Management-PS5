#include "host.h"
#include "sim.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; \
} } while (0)

typedef struct {
    int connects;
    pad_info info;
} seen;

static void on_connect(void *ud, int slot, const pad_info *info)
{
    seen *s = ud;
    (void)slot;
    s->connects++;
    s->info = *info;
}

static void run(host_t *h, int ms)
{
    while (sim_now() < ms) host_poll(h, 5);
}

static int test_seeded_nav(void)
{
    static const unsigned char addr[6] = { 0x10, 0x21, 0x32, 0x43, 0x54, 0x65 };
    sim_pad pad;
    sim_t *sim;
    host_t *h;
    host_events ev;
    seen s;
    unsigned char local[6];

    memset(&pad, 0, sizeof pad);
    memset(&s, 0, sizeof s);
    memset(&ev, 0, sizeof ev);
    ev.on_connect = on_connect;
    ev.ud = &s;
    pad.mode = SIM_RECONNECT;
    memcpy(pad.addr, addr, sizeof addr);
    pad.cod = 0x002508;
    pad.vid = 0x054c;
    pad.pid = 0x03d5;
    pad.report[0] = 1;
    pad.report_len = 49;
    sim = sim_new(&pad);
    CHECK(sim != NULL);
    h = host_open(sim_hci(sim), "build/test_usb_nav_seed.db", &ev);
    CHECK(h != NULL);
    CHECK(!host_local_address(h, local)); /* the simulator has no local BD_ADDR */
    CHECK(!host_local_address(NULL, local));
    CHECK(!host_local_address(h, NULL));
    CHECK(host_seed_usb_move(h, addr));
    /* Adding another controller must preserve the first identity. */
    {unsigned char other[6]={7,8,9,10,11,12};CHECK(host_seed_usb_move(h,other));other[0]=8;CHECK(host_seed_usb_nav(h,other));}
    sim_power_on(sim);
    run(h, 10000);
    CHECK(host_paired_count(h) == 0); /* no fake or persisted link key */
    CHECK(s.connects == 1);
    CHECK(s.info.vid == 0x054c && s.info.pid == 0x03d5);
    host_close(h);
    sim_free(sim);
    return 0;
}

static int test_unseeded_nav_rejected(void)
{
    static const unsigned char addr[6] = { 0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF5 };
    static const unsigned char other_seed[6] = { 0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF4 };
    sim_pad pad;
    sim_t *sim;
    host_t *h;

    memset(&pad, 0, sizeof pad);
    pad.mode = SIM_RECONNECT;
    memcpy(pad.addr, addr, sizeof addr);
    pad.cod = 0x002508;
    pad.vid = 0x054c;
    pad.pid = 0x03d5;
    sim = sim_new(&pad);
    CHECK(sim != NULL);
    h = host_open(sim_hci(sim), "build/test_usb_nav_seed_unpaired.db", NULL);
    CHECK(h != NULL);
    CHECK(host_seed_usb_move(h, other_seed));
    sim_power_on(sim);
    run(h, 1000);
    CHECK(!sim_link_up(sim));
    CHECK(host_paired_count(h) == 0);
    host_close(h);
    sim_free(sim);
    return 0;
}

static int test_seed_validation(void)
{
    static const unsigned char good[6] = { 0x01, 2, 3, 4, 5, 6 };
    static const unsigned char zero[6] = { 0 };
    static const unsigned char all_ff[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    sim_pad pad;
    sim_t *sim;
    host_t *h;

    memset(&pad, 0, sizeof pad);
    sim = sim_new(&pad);
    CHECK(sim != NULL);
    h = host_open(sim_hci(sim), "build/test_usb_nav_seed_validation.db", NULL);
    CHECK(h != NULL);
    CHECK(!host_seed_usb_move(h, zero));
    CHECK(!host_seed_usb_move(h, all_ff));
    CHECK(!host_seed_usb_move(h, NULL));
    CHECK(host_seed_usb_move(h, good));
    CHECK(!host_seed_usb_move(NULL, good));
    host_close(h);
    sim_free(sim);
    return 0;
}

int main(void)
{
    puts("USB-seeded Move acceptance");
    CHECK(test_seed_validation() == 0);
    CHECK(test_unseeded_nav_rejected() == 0);
    CHECK(test_seeded_nav() == 0);
    puts("USB Move seed checks passed");
    return 0;
}
