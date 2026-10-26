/* The web page's server against a host with a simulated pad connected. */
#include "sim.h"
#include "../src/host.h"
#include "../src/web.h"
#include "../src/util.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

/* A port per run, so two runs at once do not collide. */
static int g_port;
#define PORT g_port

static host_t *g_host;
static web_t *g_web;
static volatile int g_bt_state = WEB_BT_OK, g_retry;
static char g_reason[200] = "";
static int g_phys_readable = 1;

static int fake_physical(uint32_t *buttons, int *err)
{
    *err = g_phys_readable ? 0 : (int)0xFFFF0003;
    *buttons = PAD_L1 | PAD_R1;
    return g_phys_readable;
}

static int g_ps_calls, g_ps_slot = -1, g_ps_result = 1;

static int fake_press_ps(int slot)
{
    g_ps_calls++;
    g_ps_slot = slot;
    return g_ps_result;
}

static void run(int ms)
{
    long end = sim_now() + ms;
    while (sim_now() < end) {
        host_poll(g_host, 5);
        web_poll(g_web);
    }
}

/* Sends a raw request and serves it; returns the response (status line on). */
static int request(const char *raw, size_t rawlen, char *out, int max)
{
    struct sockaddr_in sin;
    int fd = socket(AF_INET, SOCK_STREAM, 0), n = 0, i;

    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&sin, sizeof sin) != 0) {
        close(fd);
        return -1;
    }
    if (rawlen) send(fd, raw, rawlen, 0);
    for (i = 0; i < 400; i++) {             /* the server runs in this thread */
        ssize_t k;
        web_poll(g_web);
        if (g_host) host_poll(g_host, 1);
        k = recv(fd, out + n, (size_t)(max - 1 - n), MSG_DONTWAIT);
        if (k > 0) n += (int)k;
        else if (k == 0) break;
        usleep(1000);
    }
    out[n] = '\0';
    close(fd);
    return n;
}

static int get(const char *method, const char *path, char *out, int max)
{
    char raw[512];
    int n = snprintf(raw, sizeof raw, "%s %s HTTP/1.1\r\nHost: 192.168.1.50:8095\r\nX-AnyPad: 1\r\n\r\n",
                     method, path);
    return request(raw, (size_t)n, out, max);
}

static int get_with(const char *head, char *out, int max)
{
    return request(head, strlen(head), out, max);
}


#include "../src/studio_control.h"
int studio_auto_assign(int slot,unsigned player){return slot>=0&&slot<4&&player<4;}
int studio_color(int slot){(void)slot;return 4;}
int studio_pair(unsigned p,int first,int second,unsigned kind){return p<4&&first>=0&&second<4&&kind<3;}
int studio_assignment(int slot,int*p,int*r){(void)slot;*p=0;*r=0;return 1;}
int studio_assign(int slot,unsigned p,unsigned r){return slot==0&&p<4&&r<4;}
static move_config config;
static int applies;
const move_config *studio_config_get(void){return &config;}
int studio_virtual_available(void){return 1;}
int studio_aim_action(int slot,const char*action){(void)action;return slot==0;}
aim_setup_state studio_aim_status(int slot){(void)slot;return (aim_setup_state){0};}
int studio_calibrate(int slot,int reset){(void)reset;return slot==0;}
int studio_calibration_status(int slot,unsigned *percent){(void)slot;*percent=100;return 2;}
const char *studio_virtual_status(void){return "Emulator bridge";}
int studio_config_apply(int p,const char*k,unsigned v){applies++;return p==0&&!strcmp(k,"trigger")&&v==2;}
int main(void){
 char out[10000]; volatile int stop=0; web_cfg wc={0};
 (void)run;(void)fake_physical;(void)fake_press_ps;(void)g_bt_state;(void)g_retry;(void)g_reason;
 signal(SIGPIPE,SIG_IGN);g_port=22000+(getpid()%15000);move_config_defaults(&config);
 wc.retry=&g_retry;wc.host=&g_host;wc.bt_state=&g_bt_state;wc.stop=&stop;wc.version="beta1";g_web=web_start(&wc,PORT);CHECK(g_web);
 get("GET","/api/config",out,sizeof out);CHECK(strstr(out,"200 OK")&&strstr(out,"\"combined\":false"));
 get("GET","/api/aim?addr=01:02:03:04:05:06",out,sizeof out);CHECK(strstr(out,"404"));
 get("POST","/api/aim?addr=01:02:03:04:05:06&action=startjunk",out,sizeof out);CHECK(strstr(out,"400 Bad"));
 get("GET","/api/calibration?addr=01:02:03:04:05:06",out,sizeof out);CHECK(strstr(out,"404")&&strstr(out,"no longer connected"));
 get("POST","/api/calibration?addr=01:02:03:04:05:06&action=start",out,sizeof out);CHECK(strstr(out,"404"));
 get("POST","/api/calibration?addr=01:02:03:04:05:06&action=bogus",out,sizeof out);CHECK(strstr(out,"400 Bad"));
 get("GET","/api/calibration?addr=01:02:03:04:05:06junk",out,sizeof out);CHECK(strstr(out,"400 Bad"));
 get("POST","/api/config?player=1&key=trigger&value=2",out,sizeof out);CHECK(strstr(out,"200 OK")&&applies==1);
 get("POST","/api/config?player=0&key=trigger&value=2",out,sizeof out);CHECK(strstr(out,"400 Bad")&&applies==1);
 get("POST","/api/config?player=1&key=trigger&value=2junk",out,sizeof out);CHECK(strstr(out,"400 Bad")&&applies==1);
 get("POST","/api/config?player=1&key=trigger&value=99",out,sizeof out);CHECK(strstr(out,"400 Bad")&&applies==2);
 get_with("POST /api/config?player=1&key=trigger&value=2 HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",out,sizeof out);CHECK(strstr(out,"403")&&applies==2);
 get("POST","/api/disconnect-all",out,sizeof out);CHECK(strstr(out,"200 OK")&&!stop);
 get("POST","/api/restart-bluetooth",out,sizeof out);CHECK(strstr(out,"200 OK")&&g_retry==1&&!stop);
 get("GET","/api/phone-camera",out,sizeof out);CHECK(strstr(out,"200 OK")&&strstr(out,"\"url\""));
 get("POST","/api/phone-camera?url=http%3A%2F%2F8.8.8.8%3A8080%2Fvideo",out,sizeof out);CHECK(strstr(out,"400 Bad"));
 get("POST","/api/phone-camera?url=%00",out,sizeof out);CHECK(strstr(out,"400 Bad"));
 get("GET","/api/camera-exclusions",out,sizeof out);CHECK(strstr(out,"200 OK")&&strstr(out,"\"zones\""));
 get("POST","/api/camera-exclude?x=319&y=0&w=100&h=10",out,sizeof out);CHECK(strstr(out,"400 Bad"));
 puts(g_fail?"studio API FAILED":"studio API checks passed");return g_fail?1:0;
}
