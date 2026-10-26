#include "web.h"
#include "log.h"
#include "util.h"
#ifdef MOVE_STUDIO
#include "studio_control.h"
#include "../camera/phone-source.h"
#include "../camera/exclusions.h"
#endif
#ifdef ANYPAD_RAW_DIAGNOSTIC
#include "move_bridge.h"
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define WEB_CLIENTS     8
#define REQ_MAX         8192
#define T_REQUEST       3000    /* ms for a client to send its request */
#define LOG_TAIL        6144

extern const unsigned char web_page_html[];
extern const unsigned int web_page_html_len;

typedef struct {
    int fd;
    int local;                          /* the peer is on the local network */
    long t_start;
    int len;
    char buf[REQ_MAX + 1];
} client;

struct web {
    web_cfg cfg;
    int fd;
    client c[WEB_CLIENTS];
};

static int g_allow_any_peer;            /* tests only */

/* ---- helpers ------------------------------------------------------------- */

static void set_nonblock(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

/* Sends all of it, giving up on a client too slow to take it. */
static void send_all(int fd, const char *p, size_t n)
{
    long deadline = now_ms() + 1000;

    while (n > 0) {
        ssize_t k = send(fd, p, n, 0);
        if (k > 0) {
            p += k;
            n -= (size_t)k;
        } else if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && now_ms() < deadline) {
            usleep(1000);
        } else {
            return;
        }
    }
}

static void respond(int fd, int code, const char *type, const char *body, size_t len)
{
    char head[256];
    const char *msg = code == 200 ? "OK" : code == 400 ? "Bad Request" : code == 403 ? "Forbidden" : code == 404 ? "Not Found" : code == 429 ? "Too Many Requests" : "Error";
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n"
                     "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
                     "Connection: close\r\n\r\n",
                     code, msg, type, (unsigned)len);
    send_all(fd, head, (size_t)n);
    send_all(fd, body, len);
}

#ifdef MOVE_STUDIO
#include "../camera/eye-preview.h"
static int preview_socket=-1;
static uint8_t preview_bmp[EYE_PREVIEW_BYTES],preview_in[EYE_PREVIEW_BYTES+1];
static long preview_received;
static void preview_poll(void){
 if(preview_socket<0){
  struct sockaddr_in a;memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(EYE_PREVIEW_PORT);
  preview_socket=socket(AF_INET,SOCK_DGRAM,0);if(preview_socket<0)return;
  int receive_buffer=262144;(void)setsockopt(preview_socket,SOL_SOCKET,SO_RCVBUF,&receive_buffer,sizeof receive_buffer);
#if defined(__FreeBSD__) || defined(__ORBIS__)
  a.sin_len=sizeof a;
#endif
  if(bind(preview_socket,(struct sockaddr*)&a,sizeof a)){close(preview_socket);preview_socket=-1;return;}set_nonblock(preview_socket);
 }
 for(int i=0;i<8;i++){
  ssize_t n=recv(preview_socket,preview_in,sizeof preview_in,0);if(n<0)break;
  if(n==EYE_PREVIEW_BYTES && preview_in[0]=='B' && preview_in[1]=='M' && preview_in[18]==160 && preview_in[22]==136){memcpy(preview_bmp,preview_in,sizeof preview_bmp);preview_received=now_ms();}
 }
}
#endif

static void respond_text(int fd, int code, const char *text)
{
    respond(fd, code, "application/json", text, strlen(text));
}

/* The value of a header, name compared without regard to case. */
static int header(const char *req, const char *name, char *out, size_t max)
{
    size_t nl = strlen(name);
    const char *p = strstr(req, "\r\n");

    while (p && p[2] != '\r') {
        const char *line = p + 2, *end = strstr(line, "\r\n");
        if (!end) return 0;
        if ((size_t)(end - line) > nl && line[nl] == ':' && strncasecmp(line, name, nl) == 0) {
            const char *v = line + nl + 1;
            size_t k = 0;
            while (*v == ' ') v++;
            while (v < end && k + 1 < max) out[k++] = *v++;
            out[k] = '\0';
            return 1;
        }
        p = end;
    }
    return 0;
}

/* A Host that is an IPv4 address (or localhost), with an optional port: not
 * a name. A web page on the Internet cannot make a browser use one to reach
 * the console (DNS rebinding hands the browser the console's address under
 * the attacker's name, and the name is what arrives here). */
static int host_is_literal(const char *h)
{
    unsigned a, b, c, d;
    char tail;
    int n;

    if (strncmp(h, "localhost", 9) == 0 && (h[9] == '\0' || h[9] == ':')) return 1;
    n = sscanf(h, "%3u.%3u.%3u.%3u%c", &a, &b, &c, &d, &tail);
    if (n == 4) return a < 256 && b < 256 && c < 256 && d < 256;
    return n == 5 && tail == ':' && a < 256 && b < 256 && c < 256 && d < 256;
}

/* Loopback or a private range: the page is for the local network only. */
static int peer_is_local(const struct sockaddr_in *sin)
{
    uint32_t a = ntohl(sin->sin_addr.s_addr);

    return (a >> 24) == 127 || (a >> 24) == 10 || (a >> 20) == 0xAC1 ||
           (a >> 16) == 0xC0A8 || (a >> 16) == 0xA9FE;
}

static int parse_addr(const char *s, unsigned char a[6])
{
    unsigned v[6];
    int i;

    if (sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
        return 0;
    for (i = 0; i < 6; i++) a[5 - i] = (unsigned char)v[i];   /* shown MSB first */
    return 1;
}

static void fmt_addr(char *out, const unsigned char *a)
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
}

/* ---- API --------------------------------------------------------------------- */

/* Copies `s` into out as JSON string content: quotes, backslashes and
 * control characters are dropped, so nothing can break the document. */
static void json_text(char *out, size_t max, const char *s)
{
    size_t n = 0;

    for (; s && *s && n + 1 < max; s++)
        if ((unsigned char)*s >= 0x20 && *s != '"' && *s != '\\') out[n++] = *s;
    out[n] = '\0';
}

static void api_state(web_t *w, int fd)
{
    static const struct { uint32_t bit; const char *name; } names[] = {
        { PAD_CROSS, "cross" }, { PAD_CIRCLE, "circle" }, { PAD_SQUARE, "square" },
        { PAD_TRIANGLE, "triangle" }, { PAD_L1, "l1" }, { PAD_R1, "r1" }, { PAD_L2, "l2" },
        { PAD_R2, "r2" }, { PAD_L3, "l3" }, { PAD_R3, "r3" }, { PAD_UP, "up" },
        { PAD_DOWN, "down" }, { PAD_LEFT, "left" }, { PAD_RIGHT, "right" },
        { PAD_OPTIONS, "options" }, { PAD_CREATE, "create" }, { PAD_PS, "ps" },
        { PAD_TOUCHPAD, "touchpad" },
    };
    host_t *h = *w->cfg.host;
    char out[8192], a[18];
    size_t n = 0;
    int i, first = 1;

#define PUT(...) do { int k_ = snprintf(out + n, sizeof out - n, __VA_ARGS__); \
                      if (k_ > 0) n += (size_t)k_ < sizeof out - n ? (size_t)k_ : sizeof out - n - 1; } while (0)

    {
        static const char *const bt[] = { "trying", "ok", "failed" };
        char reason[200], combo[100];
        uint32_t pb = 0;
        int perr = 0, readable = w->cfg.physical ? w->cfg.physical(&pb, &perr) : 0;
        int st = *w->cfg.bt_state;
        size_t b;
        int fb = 1;

        json_text(reason, sizeof reason, w->cfg.bt_reason);
        json_text(combo, sizeof combo, w->cfg.combo_text);
        PUT("{\"version\":\"%s\",\"running\":%s,\"pairing\":%d,\"bluetooth\":\"%s\","
            "\"reason\":\"%s\",\"combo\":\"%s\",\"physical\":{\"readable\":%s,\"error\":\"%08x\",\"buttons\":[",
            w->cfg.version, h ? "true" : "false", h ? host_pairing_left(h) : 0,
            bt[st >= 0 && st <= 2 ? st : 0], reason, combo, readable ? "true" : "false",
            (unsigned)perr);
        for (b = 0; readable && b < sizeof names / sizeof names[0]; b++)
            if (pb & names[b].bit) {
                PUT("%s\"%s\"", fb ? "" : ",", names[b].name);
                fb = 0;
            }
        PUT("]},\"pads\":[");
    }
    for (i = 0; h && i < HOST_MAX_PADS; i++) {
        pad_info info;
        pad_state st;
        size_t b;
        int fb = 1;
        if (!host_pad_get(h, i, &info, &st)) continue;
        fmt_addr(a, info.addr);
        PUT("%s{\"slot\":%d,\"profile\":\"%s\",\"vid\":\"%04x\",\"pid\":\"%04x\",\"addr\":\"%s\","
            "\"battery\":%d,\"charging\":%s,\"lx\":%u,\"ly\":%u,\"rx\":%u,\"ry\":%u,\"l2\":%u,\"r2\":%u,"
            "\"buttons\":[",
            first ? "" : ",", i + 1, info.profile, info.vid, info.pid, a,
            st.has_battery ? st.battery_pct : -1, st.charging ? "true" : "false",
            st.lx, st.ly, st.rx, st.ry, st.l2, st.r2);
#ifdef MOVE_STUDIO
        /* Assignment is appended after the button array below. */
#endif
        for (b = 0; b < sizeof names / sizeof names[0]; b++)
            if (st.buttons & names[b].bit) {
                PUT("%s\"%s\"", fb ? "" : ",", names[b].name);
                fb = 0;
            }
        PUT("]");
#ifdef MOVE_STUDIO
        PUT(",\"color\":%d",studio_color(i));
        {int player,role;if(studio_assignment(i,&player,&role))PUT(",\"player\":%d,\"role\":%d",player+1,role);}
#endif
        PUT("}");
        first = 0;
    }
    PUT("],\"paired\":[");
    first = 1;
    for (i = 0; h && i < host_paired_count(h); i++) {
        host_paired_info p;
        if (!host_paired_get(h, i, &p)) break;
        fmt_addr(a, p.addr);
        PUT("%s{\"addr\":\"%s\",\"vid\":\"%04x\",\"pid\":\"%04x\",\"le\":%s,\"connected\":%s}",
            first ? "" : ",", a, p.vid, p.pid, p.le ? "true" : "false",
            p.connected ? "true" : "false");
        first = 0;
    }
    PUT("]");
#ifdef ANYPAD_RAW_DIAGNOSTIC
    PUT(",\"tracking\":[");
    for(int player=0;player<HOST_MAX_PADS;player++){
        float bounds[4]={0};int valid=movebridge_optical_status(player,((uint64_t)now_ms()+1)*1000,bounds);
#ifdef MOVE_STUDIO
        int assigned=player,role=3;studio_assignment(player,&assigned,&role);if(role==3)valid=0;
        PUT("%s{\"controller\":%d,\"player\":%d,\"role\":%d,\"color\":%d,\"valid\":%s,\"x\":%.2f,\"y\":%.2f,\"width\":%.2f,\"height\":%.2f}",player?",":"",player+1,assigned+1,role,studio_color(player),valid?"true":"false",bounds[0],bounds[1],bounds[2],bounds[3]);
#else
        PUT("%s{\"player\":%d,\"valid\":%s,\"x\":%.2f,\"y\":%.2f,\"width\":%.2f,\"height\":%.2f}",player?",":"",player+1,valid?"true":"false",bounds[0],bounds[1],bounds[2],bounds[3]);
#endif
    }
    PUT("]");
#endif
    PUT("}");
#undef PUT
    respond(fd, 200, "application/json", out, n);
}

#ifdef MOVE_STUDIO
static void api_config(int fd){
    char out[8192];size_t n=0;const move_config*c=studio_config_get();
#define CFGPUT(...) do{int k=snprintf(out+n,sizeof out-n,__VA_ARGS__);if(k>0&&(size_t)k<sizeof out-n)n+=(size_t)k;}while(0)
    CFGPUT("{\"combined\":%s,\"virtual_status\":\"%s\",\"virtual_available\":%s,\"players\":[",c->virtual_combined_enabled?"true":"false",studio_virtual_status(),studio_virtual_available()?"true":"false");
    for(int p=0;p<4;p++){const move_player_config*v=&c->player[p];CFGPUT("%s{\"preset\":%u,\"trigger\":%u,\"nav_axis\":%u,",p?",":"",v->preset,v->move_trigger,v->nav_axis);CFGPUT("\"motion\":%u,\"motion_speed\":%u,\"motion_invert\":%u,",v->motion,v->motion_speed,v->motion_invert);CFGPUT("\"move\":[");
        for(int b=0;b<17;b++){CFGPUT("%s%u",b?",":"",v->move_button[b]);}CFGPUT("],\"nav\":[");
        for(int b=0;b<17;b++){CFGPUT("%s%u",b?",":"",v->nav_button[b]);}CFGPUT("]}");
    }CFGPUT("]}");
#undef CFGPUT
    respond(fd,200,"application/json",out,n);
}
#endif

/* The end of the log, as plain text with anything unprintable dropped. */
static void api_log(web_t *w, int fd)
{
    char buf[LOG_TAIL];
    FILE *f = fopen(w->cfg.log_path, "rb");
    size_t n = 0, i;
    long size;

    if (!f) {
        respond(fd, 200, "text/plain; charset=utf-8", "", 0);
        return;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, size > LOG_TAIL ? size - LOG_TAIL : 0, SEEK_SET);
    n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    for (i = 0; i < n; i++)
        if ((unsigned char)buf[i] < 0x20 && buf[i] != '\n') buf[i] = ' ';
    respond(fd, 200, "text/plain; charset=utf-8", buf, n);
}

static void handle(web_t *w, int fd, char *req)
{
    char method[8], path[256], hostname[64], custom[8];
    host_t *h = *w->cfg.host;

    if (sscanf(req, "%7s %255s", method, path) != 2) {
        respond_text(fd, 400, "{\"error\":\"bad request\"}");
        return;
    }
    if (!header(req, "Host", hostname, sizeof hostname) || !host_is_literal(hostname)) {
        respond_text(fd, 400, "{\"error\":\"host\"}");
        return;
    }
    if (strcmp(method, "GET") == 0 && (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)) {
        respond(fd, 200, "text/html; charset=utf-8", (const char *)web_page_html, web_page_html_len);
        return;
    }
    if (strncmp(path, "/api/", 5) != 0) {
        respond_text(fd, 404, "{\"error\":\"not found\"}");
        return;
    }

    /* Anything that changes something must carry a header only this page
     * sends: a browser will not add it to a request made by another site
     * without asking us first, and we never agree. */
    if (strcmp(method, "POST") == 0 &&
        (!header(req, "X-AnyPad", custom, sizeof custom) || strcmp(custom, "1") != 0)) {
        respond_text(fd, 403, "{\"error\":\"forbidden\"}");
        return;
    }
#ifdef MOVE_STUDIO
    if(!strcmp(method,"POST")&&!strcmp(path,"/api/disconnect-all")){
        for(int i=0;h&&i<HOST_MAX_PADS;i++){pad_info info;pad_state state;if(host_pad_get(h,i,&info,&state))(void)host_disconnect(h,info.addr);}
        respond_text(fd,200,"{\"ok\":true}");return;
    }
    if(!strcmp(method,"POST")&&!strcmp(path,"/api/restart-bluetooth")){
        *w->cfg.retry=1;respond_text(fd,200,"{\"ok\":true}");return;
    }
    if((!strcmp(method,"GET")||!strcmp(method,"POST"))&&!strncmp(path,"/api/aim?addr=",14)){
        char address[18],action[8]={0},extra;unsigned char addr[6];int slot=-1;
        int fields=sscanf(path,"/api/aim?addr=%17[0-9A-Fa-f:]&action=%7[a-z]%c",address,action,&extra);
        if((fields!=1&&fields!=2)||!parse_addr(address,addr)||
           (!strcmp(method,"POST")&&(fields!=2||(strcmp(action,"start")&&strcmp(action,"cancel")&&strcmp(action,"save"))))||
           (!strcmp(method,"GET")&&(fields!=1||strlen(path)!=31))){respond_text(fd,400,"{\"error\":\"Invalid aiming request\"}");return;}
        for(int i=0;h&&i<HOST_MAX_PADS;i++){pad_info info;pad_state state;if(host_pad_get(h,i,&info,&state)&&info.pid==0x03d5&&!memcmp(info.addr,addr,6)){slot=i;break;}}
        if(slot<0){respond_text(fd,404,"{\"error\":\"Move is no longer connected\"}");return;}
        if(!strcmp(method,"POST")&&!studio_aim_action(slot,action)){respond_text(fd,409,"{\"error\":\"Assign this as a primary Move, load factory calibration by USB, and finish the targets before saving\"}");return;}
        aim_setup_state s=studio_aim_status(slot);char body[192];
        snprintf(body,sizeof body,"{\"step\":%d,\"fresh\":%s,\"x\":%.5f,\"y\":%.5f,\"half_x\":%.5f,\"half_y\":%.5f}",s.step,s.fresh?"true":"false",s.x,s.y,s.half_x,s.half_y);respond_text(fd,200,body);return;
    }
    if((!strcmp(method,"GET")||!strcmp(method,"POST"))&&!strncmp(path,"/api/calibration?addr=",22)){
        char address[18],action[8]={0},extra;unsigned char addr[6];int slot=-1;
        int fields=sscanf(path,"/api/calibration?addr=%17[0-9A-Fa-f:]&action=%7[a-z]%c",address,action,&extra);
        if((fields!=1&&fields!=2)||!parse_addr(address,addr)||
           (!strcmp(method,"POST")&&(fields!=2||(strcmp(action,"start")&&strcmp(action,"reset"))))||
           (!strcmp(method,"GET")&&(fields!=1||strlen(path)!=22+17))){respond_text(fd,400,"{\"error\":\"Invalid calibration request\"}");return;}
        for(int i=0;h&&i<HOST_MAX_PADS;i++){pad_info info;pad_state state;if(host_pad_get(h,i,&info,&state)&&info.pid==0x03d5&&!memcmp(info.addr,addr,6)){slot=i;break;}}
        if(slot<0){respond_text(fd,404,"{\"error\":\"Move is no longer connected\"}");return;}
        if(!strcmp(method,"POST")&&!studio_calibrate(slot,!strcmp(action,"reset"))){respond_text(fd,409,"{\"error\":\"Reconnect this Move by USB to load factory calibration, then try again\"}");return;}
        unsigned progress=0;int status=studio_calibration_status(slot,&progress);char body[96];
        snprintf(body,sizeof body,"{\"status\":%d,\"progress\":%u}",status,progress);respond_text(fd,200,body);return;
    }
    if(!strcmp(method,"POST")&&!strncmp(path,"/api/assign-auto?",17)){
        char address[18],extra;unsigned player;unsigned char addr[6];int slot=-1;
        if(sscanf(path,"/api/assign-auto?addr=%17[0-9A-Fa-f:]&player=%u%c",address,&player,&extra)!=2||player<1||player>4||!parse_addr(address,addr)){respond_text(fd,400,"{\"error\":\"Invalid controller or player\"}");return;}
        for(int i=0;h&&i<HOST_MAX_PADS;i++){pad_info info;pad_state state;if(host_pad_get(h,i,&info,&state)&&!memcmp(info.addr,addr,6)){slot=i;break;}}
        if(slot<0)respond_text(fd,404,"{\"error\":\"Controller is no longer connected\"}");
        else if(!studio_auto_assign(slot,player-1))respond_text(fd,409,"{\"error\":\"Player already has two Moves, or a Move + Nav. Choose another player.\"}");
        else respond_text(fd,200,"{\"ok\":true}");
        return;
    }
    if(!strcmp(method,"POST")&&!strncmp(path,"/api/disconnect?addr=",21)){
        unsigned char addr[6];
        if(!parse_addr(path+21,addr))respond_text(fd,400,"{\"error\":\"Invalid controller address\"}");
        else if(!host_disconnect(h,addr))respond_text(fd,404,"{\"error\":\"Controller is no longer connected\"}");
        else respond_text(fd,200,"{\"ok\":true}");
        return;
    }
    if(!strcmp(method,"GET")&&!strcmp(path,"/api/usb")){web_t copy=*w;copy.cfg.log_path="/data/move-studio/usb-registration.log";api_log(&copy,fd);return;}
    if(!strcmp(method,"POST")&&!strncmp(path,"/api/player?",12)){
        unsigned player,first,second,kind;char extra;
        if(sscanf(path,"/api/player?player=%u&move=%u&second=%u&kind=%u%c",&player,&first,&second,&kind,&extra)!=4||player<1||player>4||first>4||second>4||kind>2||!studio_pair(player-1,(int)first-1,(int)second-1,kind))respond_text(fd,400,"{\"error\":\"Choose a valid controller pair; each controller can belong to one player\"}");
        else {respond_text(fd,200,"{\"ok\":true}");}return;
    }
    if(!strcmp(method,"POST")&&!strncmp(path,"/api/assign?",12)){
        unsigned slot,player,role;char extra;
        if(sscanf(path,"/api/assign?slot=%u&player=%u&role=%u%c",&slot,&player,&role,&extra)!=3||slot<1||slot>HOST_MAX_PADS||player<1||player>4||role>3||!studio_assign((int)slot-1,player-1,role))respond_text(fd,400,"{\"error\":\"Assignment conflicts with another controller or could not be saved\"}");
        else {respond_text(fd,200,"{\"ok\":true}");}return;
    }
    if(!strcmp(method,"GET")&&!strcmp(path,"/api/config")){api_config(fd);return;}
    if(!strcmp(method,"POST")&&!strncmp(path,"/api/config?",12)){
        unsigned player,value;char key[24],extra;
        if(sscanf(path,"/api/config?player=%u&key=%23[a-z0-9_]&value=%u%c",&player,key,&value,&extra)!=3||player<1||player>4||!studio_config_apply((int)player-1,key,value))respond_text(fd,400,"{\"error\":\"Invalid or unsaved configuration\"}");
        else {api_config(fd);}return;
    }
#endif
#ifdef MOVE_STUDIO
    if(!strcmp(method,"GET")&&!strcmp(path,"/api/camera-exclusions")){
        camera_exclusion zones[CAMERA_EXCLUSION_MAX];unsigned count=camera_exclusions_load(zones);char body[1024];size_t n=(size_t)snprintf(body,sizeof body,"{\"zones\":[");
        for(unsigned i=0;i<count;i++)n+=(size_t)snprintf(body+n,sizeof body-n,"%s{\"x\":%u,\"y\":%u,\"w\":%u,\"h\":%u}",i?",":"",zones[i].x,zones[i].y,zones[i].w,zones[i].h);
        snprintf(body+n,sizeof body-n,"]}");respond_text(fd,200,body);return;
    }
    if(!strcmp(method,"POST")&&!strncmp(path,"/api/camera-exclude?",20)){
        camera_exclusion zones[CAMERA_EXCLUSION_MAX],r;unsigned n=camera_exclusions_load(zones);char extra;
        if(!strcmp(path,"/api/camera-exclude?clear=1"))n=0;
        else if(n>=CAMERA_EXCLUSION_MAX||sscanf(path,"/api/camera-exclude?x=%u&y=%u&w=%u&h=%u%c",&r.x,&r.y,&r.w,&r.h,&extra)!=4||!camera_exclusion_valid(r)){respond_text(fd,400,"{\"error\":\"Draw a valid area; up to eight ignored areas are supported\"}");return;}else zones[n++]=r;
        int ok=camera_exclusions_save(zones,n);respond_text(fd,ok?200:500,ok?"{\"ok\":true}":"{\"error\":\"Could not save ignored areas\"}");return;
    }
    if(!strcmp(method,"GET")&&!strcmp(path,"/api/phone-camera")){
        char url[256],body[320];phone_source_read(url);snprintf(body,sizeof body,"{\"url\":\"%s\"}",url);respond_text(fd,200,body);return;
    }
    if(!strcmp(method,"POST")&&!strncmp(path,"/api/phone-camera?url=",22)){
        char url[256];size_t n=0;const char*p=path+22;int valid=1;
        while(*p&&n<255){unsigned c=(unsigned char)*p++;if(c=='%'){unsigned v;char hex[3]={0};if(!p[0]||!p[1]){valid=0;break;}hex[0]=p[0];hex[1]=p[1];if(!isxdigit((unsigned char)hex[0])||!isxdigit((unsigned char)hex[1])||sscanf(hex,"%x",&v)!=1){valid=0;break;}c=v;p+=2;}if(c<32||c>126){valid=0;break;}url[n++]=(char)c;}url[n]=0;
        phone_source source;if(*p||!valid||(n&&!phone_source_parse(url,&source))){respond_text(fd,400,"{\"error\":\"Use http:// followed by a local IPv4 address and port (1024-65535), for example http://192.168.0.10:8080. The video path is optional.\"}");return;}
        if(n){snprintf(url,sizeof url,"http://%s:%u%s",source.host,source.port,source.path);n=strlen(url);}
        FILE*f=fopen(PHONE_URL_FILE ".tmp","w");int ok=f!=NULL;if(f){if(fwrite(url,1,n,f)!=n||fflush(f)||fsync(fileno(f)))ok=0;if(fclose(f))ok=0;if(ok&&rename(PHONE_URL_FILE ".tmp",PHONE_URL_FILE))ok=0;}
        respond_text(fd,ok?200:500,ok?"{\"ok\":true}":"{\"error\":\"Could not save camera source\"}");return;
    }
    if(!strcmp(method,"GET")&&!strcmp(path,"/api/camera")){
        char status[512];FILE*f=fopen("/data/move-studio/camera.json","r");size_t n=0;if(f){n=fread(status,1,sizeof status-1,f);fclose(f);}if(n){status[n]=0;respond(fd,200,"application/json",status,n);}else respond_text(fd,200,"{\"source\":\"waiting\",\"message\":\"Camera service not started\"}");return;
    }
    if(!strcmp(method,"GET")&&!strcmp(path,"/api/camera-preview")){
        if(preview_received && now_ms()-preview_received<1500)respond(fd,200,"image/bmp",(const char*)preview_bmp,sizeof preview_bmp);
        else respond_text(fd,503,"{\"error\":\"Camera preview unavailable\"}");
        return;
    }
#endif
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/state") == 0) {
        api_state(w, fd);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/log") == 0) {
        api_log(w, fd);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/pair") == 0 && h) {
        if(!host_pair(h,60)){
            respond_text(fd,423,"{\"error\":\"pairing_locked\"}");return;
        }
        log_line("web: pairing started");
        respond_text(fd, 200, "{\"ok\":true}");
    } else if (strcmp(method, "POST") == 0 && strncmp(path, "/api/forget?addr=", 17) == 0 && h) {
        unsigned char a[6];
        if (!parse_addr(path + 17, a)) respond_text(fd, 400, "{\"error\":\"address\"}");
        else if (!host_forget(h, a)) respond_text(fd, 404, "{\"error\":\"not paired\"}");
        else respond_text(fd, 200, "{\"ok\":true}");
    } else if (strcmp(method, "POST") == 0 && strncmp(path, "/api/ps?slot=", 13) == 0) {
        int n = atoi(path + 13);
        if (n < 1 || n > HOST_MAX_PADS) respond_text(fd, 400, "{\"error\":\"slot\"}");
        else if (!w->cfg.press_ps || !w->cfg.press_ps(n - 1)) respond_text(fd, 404, "{\"error\":\"no pad\"}");
        else respond_text(fd, 200, "{\"ok\":true}");
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/retry") == 0) {
        if (*w->cfg.bt_state == WEB_BT_FAILED) {
            log_line("web: Bluetooth retry requested");
            *w->cfg.retry = 1;
            respond_text(fd, 200, "{\"ok\":true}");
        } else {
            respond_text(fd, 400, "{\"error\":\"not failed\"}");
        }
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/stop") == 0) {
        log_line("web: stop requested");
        *w->cfg.stop = 1;
        respond_text(fd, 200, "{\"ok\":true}");
    } else {
        respond_text(fd, 404, "{\"error\":\"not found\"}");
    }
}

/* ---- serving ------------------------------------------------------------------- */

web_t *web_start(const web_cfg *cfg, int port)
{
    struct sockaddr_in sin;
    web_t *w;
    int one = 1, i;

    w = calloc(1, sizeof *w);
    if (!w) return NULL;
    w->cfg = *cfg;
    for (i = 0; i < WEB_CLIENTS; i++) w->c[i].fd = -1;

    w->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (w->fd < 0) {
        free(w);
        return NULL;
    }
    setsockopt(w->fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons((uint16_t)port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(w->fd, (struct sockaddr *)&sin, sizeof sin) != 0 || listen(w->fd, 4) != 0) {
        log_line("web: port %d not available (errno %d), no web page", port, errno);
        close(w->fd);
        free(w);
        return NULL;
    }
    set_nonblock(w->fd);
    log_line("web: listening on port %d", port);
    return w;
}

void web_poll(web_t *w)
{
    long now = now_ms();
    int i;

    if (!w) return;
#ifdef MOVE_STUDIO
    preview_poll();
#endif
    for (;;) {                          /* new connections */
        struct sockaddr_in peer;
        socklen_t plen = sizeof peer;
        int fd = accept(w->fd, (struct sockaddr *)&peer, &plen), slot = -1;
        if (fd < 0) break;
        if (plen < sizeof peer || (!g_allow_any_peer && !peer_is_local(&peer))) {
            close(fd);                  /* not from the local network */
            continue;
        }
#ifdef SO_NOSIGPIPE
        {
            int one = 1;
            setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
        }
#endif
        set_nonblock(fd);
        for (i = 0; i < WEB_CLIENTS; i++) if (w->c[i].fd < 0) { slot = i; break; }
        if (slot < 0) {
            respond_text(fd, 429, "{\"error\":\"busy\"}");
            close(fd);
            continue;
        }
        w->c[slot].fd = fd;
        w->c[slot].len = 0;
        w->c[slot].t_start = now;
    }

    for (i = 0; i < WEB_CLIENTS; i++) {
        client *c = &w->c[i];
        ssize_t k;

        if (c->fd < 0) continue;
        k = recv(c->fd, c->buf + c->len, (size_t)(REQ_MAX - c->len), 0);
        if (k > 0) c->len += (int)k;
        c->buf[c->len] = '\0';

        if (strstr(c->buf, "\r\n\r\n")) {
            handle(w, c->fd, c->buf);
        } else if (k == 0 || (k < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            /* the client went away */
        } else if (c->len >= REQ_MAX) {
            respond_text(c->fd, 400, "{\"error\":\"too large\"}");
        } else if (now - c->t_start <= T_REQUEST) {
            continue;                   /* still coming */
        }
        close(c->fd);
        c->fd = -1;
    }
}

void web_stop(web_t *w)
{
    int i;

    if (!w) return;
    for (i = 0; i < WEB_CLIENTS; i++) if (w->c[i].fd >= 0) close(w->c[i].fd);
    close(w->fd);
    free(w);
}
