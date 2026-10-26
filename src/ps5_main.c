/* AnyPad PS5 -- Bluetooth game controllers on the PlayStation 5.
 *
 * Pads from other consoles and brands connect to the console's own
 * Bluetooth chip, beside the DualSense, and appear to games as DualSense
 * controllers of the main user.
 *
 *   hci_usb.c   the Bluetooth chip, shared with the system's driver
 *   host.c      pairing, reconnection, L2CAP, SDP and HID (Classic)
 *   le.c        the same over Bluetooth LE
 *   profiles.c  each controller's reports
 *   ps5_vpad.c  the virtual DualSense each pad drives
 *   web.c       the menu: a page on port 8095
 *   (no console hotkey: a plain payload cannot read the DualSense; the menu opens from the media row)
 *
 * ALPHA: see PRUEBAS.md for what has been tried on a console.
 *
 * Stability comes first. It only touches what it created: its own Bluetooth
 * links, its own virtual pads, its own files under /data/anypad. When
 * something it depends on fails, it says so (log, notification, the menu)
 * and stays out of the way instead of retrying forever.
 *
 * The menu is started first, before anything that can fail, so that it is
 * there to explain a failure. It is a web page: from a phone or a PC at the
 * address the start-up notification gives, or on the console itself by
 * holding a button combination on the DualSense (L2 + R2 + OPTIONS for 1.5
 * seconds unless /data/anypad/config.ini says otherwise).
 *
 * Stopping: the menu's button, or create /data/anypad/stop. It disconnects
 * its pads, removes its virtual pads, puts back the controller setting it
 * changed, and exits. It does the same by itself when the console starts
 * going to rest mode (read from SceSystemStateMgrInfo), so nothing of it is left half-way across a sleep.
 * Pairing: the menu's button, or create /data/anypad/pair.
 */
#include "hci_usb.h"
#ifdef MOVE_STUDIO_EXTERNAL_BT
#include "hci_external.h"
#endif
#include "host.h"
#include "studio_control.h"
#include "players.h"
#include "usb_register.h"
#include <dirent.h>
#include "porpoise_move.h"
#include "porpoise_nav.h"
#include "lock.h"
#include "log.h"
#include "netinfo.h"
#include "ps5_power.h"
#include "ps5_ui.h"
#include "ps5_apps.h"
#include "ps5_sysinfo.h"
#include "ps5_vpad.h"
#include "util.h"
#include "version.h"
#include "web.h"

#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STATE_DIR    "/data/anypad-move-nav"
#define LOG_PATH     "/data/move-studio/bridge.log"
#ifdef MOVE_STUDIO_EXTERNAL_BT
#define DB_PATH      STATE_DIR "/pads-asus.db"
#define LOCAL_ADDRESS_PATH "/data/move-studio/asus-address.bin"
#else
#define DB_PATH      STATE_DIR "/pads.db"
#define LOCAL_ADDRESS_PATH STATE_DIR "/local-address.bin"
#endif
#define LOCK_PATH    STATE_DIR "/anypad.lock"
#define CONFIG_PATH  STATE_DIR "/config.ini"
#define PAIR_FLAG    STATE_DIR "/pair"
#define STOP_FLAG    STATE_DIR "/stop"
#define NO_ICON_FLAG STATE_DIR "/no_icon"          /* do not put AnyPad in the media row */
#define RM_ICON_FLAG STATE_DIR "/remove_icon"      /* take it out, once */
#define PAIR_SECONDS 60

#define OPEN_TRIES      5       /* opening Bluetooth: 5 tries, 3 s apart */
#define RAISED_TRIES    2       /* ... the first two with raised credentials */

/* Findable in the binary: strings AnyPad-PS5-*.elf | grep anypad-version */
static const char g_version_tag[] __attribute__((used)) = "anypad-version " ANYPAD_VERSION;

static volatile sig_atomic_t g_stop;
static volatile int g_web_stop;         /* the menu's stop button */
static volatile int g_retry;            /* the menu's retry button */
static volatile int g_bt_state = WEB_BT_TRYING;
static char g_bt_reason[200];

static host_t *g_host;                  /* NULL until Bluetooth is up */
static web_t *g_web;
static int g_usb_nav_seeded, g_usb_move_seeded;
static int g_move_stream[HOST_MAX_PADS], g_move_player[HOST_MAX_PADS] = {-1,-1,-1,-1,-1,-1,-1,-1};
static player_registry g_players;
static int g_device[HOST_MAX_PADS]={-1,-1,-1,-1,-1,-1,-1,-1};
static int g_vpad_ok;                   /* virtual pads are possible */
static int g_bt_attempt;                /* Bluetooth tries made in this round */
static long g_bt_next;                  /* when the next one is due */
static int g_bt_plain_creds;            /* the chip answered only without raised credentials */

static move_config g_studio_config;
static pad_state g_studio_state[HOST_MAX_PADS];
static unsigned g_studio_pid[HOST_MAX_PADS];
static long g_studio_seen[HOST_MAX_PADS],g_virtual_started[4];
static int g_virtual_ready,g_virtual_failed,g_virtual_requested[4];
static const char*g_virtual_status="Emulator bridge";
int studio_assignment(int slot,int *player,int *role){
 if(slot<0||slot>=HOST_MAX_PADS||g_device[slot]<0)return 0;player_device*d=&g_players.device[g_device[slot]];*player=(int)d->player;*role=(int)d->role;return 1;
}
int studio_color(int slot){
 static const int primary_colors[4]={0,1,2,4},spare_colors[8]={5,3,6,7,4,0,1,2};unsigned used=0;
 int selected[HOST_MAX_PADS]={4,5,3,2,4,5,3,2};
 for(int i=0;i<HOST_MAX_PADS;i++){int p,role;if(studio_assignment(i,&p,&role)&&role==ROLE_MOVE){selected[i]=primary_colors[p];used|=1u<<selected[i];}}
 for(int i=0;i<HOST_MAX_PADS;i++){int p,role;if(studio_assignment(i,&p,&role)&&role==ROLE_SECOND_MOVE){for(int j=0;j<8;j++)if(!(used&(1u<<spare_colors[j]))){selected[i]=spare_colors[j];used|=1u<<selected[i];break;}}}
 return slot>=0&&slot<HOST_MAX_PADS?selected[slot]:4;
}
static void studio_light(int slot){
 static const uint8_t rgb[8][3]={{0,0,63},{63,0,0},{0,63,0},{63,63,0},{63,0,63},{0,63,63},{63,25,0},{25,0,63}};
 int color=studio_color(slot);movebridge_set_color(slot,(unsigned)color);pad_output out={0};out.r=rgb[color][0];out.g=rgb[color][1];out.b=rgb[color][2];host_set_output(g_host,slot,&out);
}
static int studio_commit_registry(const player_registry*next){
 if(!players_save("/data/move-studio/players.ini",next))return 0;
 int changed[HOST_MAX_PADS]={0};
 for(int i=0;i<HOST_MAX_PADS;i++){if(g_device[i]<0)continue;const player_device*old=&g_players.device[g_device[i]],*n=&next->device[g_device[i]];
  changed[i]=old->player!=n->player||old->role!=n->role;if(!changed[i])continue;
  if(old->role==ROLE_NAV)navbridge_player_close((int)old->player);
  if(g_move_stream[i])movebridge_invalidate(i);
 }
 g_players=*next;
 for(int i=0;i<HOST_MAX_PADS;i++){int p,role;if(!studio_assignment(i,&p,&role))continue;
  if(changed[i]){if(g_studio_pid[i]==0x03d5){g_move_player[i]=role==ROLE_OFF?-1:p;if(g_move_stream[i]&&role!=ROLE_OFF)movebridge_route(i,p,role==ROLE_SECOND_MOVE);}else if(role==ROLE_NAV)navbridge_player_start(p);}
  if(g_studio_pid[i]==0x03d5)studio_light(i);
 }return 1;
}
int studio_auto_assign(int slot,unsigned player){
 if(slot<0||slot>=HOST_MAX_PADS||g_device[slot]<0)return 0;player_registry next=g_players;unsigned char active[PLAYER_DEVICES]={0};for(int i=0;i<HOST_MAX_PADS;i++)if(g_device[i]>=0)active[g_device[i]]=1;players_release_offline(&next,active);
 return players_auto_assign(&next,(unsigned)g_device[slot],player)&&studio_commit_registry(&next);
}
int studio_assign(int slot,unsigned player,unsigned role){
 if(slot<0||slot>=HOST_MAX_PADS||g_device[slot]<0)return 0;player_registry next=g_players;
 return players_assign(&next,(unsigned)g_device[slot],player,role)&&studio_commit_registry(&next);
}
int studio_pair(unsigned player,int primary_slot,int secondary_slot,unsigned kind){
 if(primary_slot< -1||primary_slot>=HOST_MAX_PADS||secondary_slot< -1||secondary_slot>=HOST_MAX_PADS)return 0;
 int first=primary_slot<0?-1:g_device[primary_slot],second=secondary_slot<0?-1:g_device[secondary_slot];
 if((primary_slot>=0&&first<0)||(secondary_slot>=0&&second<0))return 0;
 player_registry next=g_players;return players_pair(&next,player,first,second,kind)&&studio_commit_registry(&next);
}
const move_config*studio_config_get(void){return &g_studio_config;}
int studio_aim_action(int slot,const char*action){
    int player,role;
    if(slot<0||slot>=HOST_MAX_PADS||g_studio_pid[slot]!=0x03d5||!studio_assignment(slot,&player,&role)||role!=ROLE_MOVE)return 0;
    uint64_t now=(uint64_t)now_ms()*1000;
    if(!strcmp(action,"save")){char path[128];snprintf(path,sizeof path,"/data/move-studio/aim-player-%d.ini",player+1);return aim_setup_save(slot,path,now);}
    return aim_setup_action(slot,!strcmp(action,"cancel"),now);
}
aim_setup_state studio_aim_status(int slot){return aim_setup_status(slot,(uint64_t)now_ms()*1000);}
int studio_calibrate(int slot,int reset){
    if(slot<0||slot>=HOST_MAX_PADS||g_studio_pid[slot]!=0x03d5||now_ms()-g_studio_seen[slot]>500)return 0;
    return movebridge_calibrate(slot,reset,(uint64_t)now_ms()*1000);
}
int studio_calibration_status(int slot,unsigned *percent){return movebridge_calibration_status(slot,(uint64_t)now_ms()*1000,percent);}
int studio_virtual_available(void){
#ifdef MOVE_STUDIO_VIRTUAL_BETA
    return 1; /* Explicit test artifact; normal release remains guarded. */
#else
    return access("/data/move-studio/enable-experimental-virtual",F_OK)==0;
#endif
}
const char*studio_virtual_status(void){return studio_virtual_available()?g_virtual_status:"DualSense output paused: compatibility repair required";}
int studio_config_apply(int player,const char*key,unsigned value){
    if(player<0||player>=4||!key)return 0;move_config next=g_studio_config;
    move_player_config*c=&next.player[player];unsigned index;char tail;
    if(!strcmp(key,"mode")){if(value>1||(value&&!studio_virtual_available()))return 0;next.virtual_combined_enabled=(int)value;}
    else if(!strcmp(key,"preset")){if(value>1)return 0;move_config_preset_set(c,(move_config_preset)value);}
    else if(!strcmp(key,"trigger"))c->move_trigger=(move_config_trigger)value;
    else if(!strcmp(key,"motion"))c->motion=value;
    else if(!strcmp(key,"motion_speed"))c->motion_speed=value;
    else if(!strcmp(key,"motion_invert"))c->motion_invert=value;
    else if(!strcmp(key,"nav_axis"))c->nav_axis=(move_config_axis)value;
    else if(sscanf(key,"move%u%c",&index,&tail)==1&&index<17)c->move_button[index]=value;
    else if(sscanf(key,"nav%u%c",&index,&tail)==1&&index<17)c->nav_button[index]=value;
    else return 0;
    if(!move_config_validate(&next)||!move_config_save("/data/move-studio/layouts.ini",&next))return 0;
    g_studio_config=next;return 1;
}
static void studio_output_tick(long now){
    if(!studio_virtual_available()||!g_studio_config.virtual_combined_enabled){
        for(int p=0;p<4;p++)if(g_virtual_requested[p]){if(vpad_creds_raise()){vpad_remove(p);vpad_creds_restore();}g_virtual_requested[p]=0;}
        g_virtual_failed=0;g_virtual_status="Emulator bridge";return;
    }
    if(!g_host){g_virtual_status="Waiting for Bluetooth";return;}
    if(!g_virtual_ready&&!g_virtual_failed){g_virtual_ready=vpad_init();g_virtual_failed=!g_virtual_ready;vpad_creds_restore();}
    if(!g_virtual_ready){g_virtual_status="Virtual controller unavailable; see log";return;}
    if(!vpad_creds_raise()){g_virtual_status="Virtual controller credentials unavailable";return;}

    int active=0,waiting=0,failed=0;
    for(int p=0;p<4;p++){
        int source=-1,nav_slot=-1,second=-1;
        for(int i=0;i<HOST_MAX_PADS;i++){int player,role;if(!studio_assignment(i,&player,&role)||player!=p||now-g_studio_seen[i]>500)continue;if(role==ROLE_MOVE)source=i;else if(role==ROLE_NAV)nav_slot=i;else if(role==ROLE_SECOND_MOVE)second=i;}
        if(source<0||nav_slot<0||second>=0){if(g_virtual_requested[p]){vpad_remove(p);g_virtual_requested[p]=0;}continue;}
        if(!g_virtual_requested[p]){g_virtual_requested[p]=vpad_add(p);g_virtual_started[p]=now;}
        if(g_virtual_requested[p]){
            pad_state combined;const pad_state*nav=nav_slot>=0?&g_studio_state[nav_slot]:NULL;
            if(move_config_combine(&g_studio_state[source],nav,&g_studio_config.player[p],1,&combined)){
                combined.calibrated_motion=0;
                movebridge_motion(source,&g_studio_state[source],&combined);
                if(second>=0)move_config_second_move(&g_studio_state[second],&combined);
                move_config_motion(&g_studio_config.player[p],&combined);
                vpad_update(p,&combined);
            }
            if(vpad_live(p))active++;else if(now-g_virtual_started[p]>6000)failed++;else waiting++;
        }
    }
    vpad_poll(now);vpad_creds_restore();
    g_virtual_status=active?"Virtual DualSense active":failed?"Virtual device not confirmed; see log":waiting?"Starting virtual controller":"Waiting for a Move + Nav pair";
}

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* ---- virtual pads follow the controllers ---------------------------------- */

static void on_connect(void *ud, int slot, const pad_info *info)
{
    (void)ud;
#ifdef ANYPAD_RAW_DIAGNOSTIC
    log_line("diagnostic connect slot=%d vid=%04x pid=%04x profile=%s",slot,info->vid,info->pid,info->profile);
    if (slot >= 0 && slot < HOST_MAX_PADS) {
        g_studio_pid[slot]=info->pid;g_studio_seen[slot]=now_ms();pad_state_reset(&g_studio_state[slot]);
        g_device[slot]=players_register(&g_players,info->addr,info->pid);
        if(g_device[slot]>=0){unsigned char active[PLAYER_DEVICES]={0};for(int i=0;i<HOST_MAX_PADS;i++)if(g_device[i]>=0)active[g_device[i]]=1;players_release_offline(&g_players,active);player_device*d=&g_players.device[g_device[slot]];if(d->role==ROLE_OFF)(void)players_auto_assign(&g_players,(unsigned)g_device[slot],d->player);(void)players_save("/data/move-studio/players.ini",&g_players);}
        int player=0,role=ROLE_OFF;studio_assignment(slot,&player,&role);
        if(info->vid==0x054c&&info->pid==0x03d5){
            char path[180];unsigned char record[149];
            snprintf(path,sizeof path,STATE_DIR "/move-%02x%02x%02x%02x%02x%02x.calibration",info->addr[0],info->addr[1],info->addr[2],info->addr[3],info->addr[4],info->addr[5]);
            FILE*f=fopen(path,"rb");if(f){int ok=fread(record,1,149,f)==149&&fgetc(f)==EOF;fclose(f);if(ok&&!memcmp(record,info->addr,6))g_move_stream[slot]=movebridge_start(slot,record+6);}
            g_move_player[slot]=role==ROLE_OFF?-1:player;
            if(g_move_stream[slot]&&role!=ROLE_OFF)movebridge_route(slot,player,role==ROLE_SECOND_MOVE);
            log_line("Move slot=%d player=%d role=%d calibration=%d",slot,player+1,role,g_move_stream[slot]);
            studio_light(slot);
        }else if(role==ROLE_NAV){navbridge_player_start(player);}

    }

    notify("Move/Nav diagnostic: %s connected (slot %d)",info->profile,slot+1);
    return;
#endif
    if (vpad_add(slot)) notify("Move Management: %s connected", info->profile);
    else notify("Move Management: %s connected, but virtual controller creation failed. See the log.", info->profile);
}

static void on_state(void *ud, int slot, const pad_state *st)
{
    (void)ud;
#ifdef ANYPAD_RAW_DIAGNOSTIC
    static pad_state last[HOST_MAX_PADS];static long printed[HOST_MAX_PADS];static unsigned count[HOST_MAX_PADS];
    if(slot<0||slot>=HOST_MAX_PADS)return;
    g_studio_state[slot]=*st;g_studio_seen[slot]=now_ms();
    struct timespec packet_ts;
    clock_gettime(CLOCK_MONOTONIC,&packet_ts);
    uint64_t packet_us=(uint64_t)packet_ts.tv_sec*1000000u+(uint64_t)packet_ts.tv_nsec/1000u;
    int assigned_player=0,assigned_role=ROLE_OFF;studio_assignment(slot,&assigned_player,&assigned_role);
    movebridge_calibration_sample(slot,st,packet_us);
    {pad_state motion={0};if(movebridge_motion(slot,st,&motion))aim_setup_sample(slot,motion.angular_velocity,motion.acceleration_g,st->buttons,packet_us);}
    movebridge_set_color(slot,(unsigned)studio_color(slot));
    if(g_move_stream[slot]&&assigned_role!=ROLE_OFF)movebridge_update(slot,st,packet_us);
    if(assigned_role==ROLE_NAV)navbridge_player_update(assigned_player,st,packet_us);
    if(g_studio_pid[slot]==0x03d5&&assigned_role!=ROLE_OFF){
        static long led_sent[HOST_MAX_PADS];long now=now_ms();
        if(now-led_sent[slot]>=1000){
            int player=slot;studio_light(slot);led_sent[slot]=now;
            float bounds[4]={0};int tracked=movebridge_optical_status(player,packet_us,bounds);
            log_line("Move optical player=%d valid=%d x=%.1f y=%.1f width=%.1f height=%.1f",player+1,tracked,bounds[0],bounds[1],bounds[2],bounds[3]);
        }
    }
    count[slot]++;
    if(now_ms()-printed[slot]>=100 && memcmp(&last[slot],st,sizeof(*st))){
        log_line("diagnostic slot=%d samples=%u buttons=%08x L2=%u R2=%u stick=%u,%u",slot,count[slot],st->buttons,st->l2,st->r2,st->lx,st->ly);
        if(st->has_motion)log_line("raw motion slot=%d accel=%d,%d,%d gyro=%d,%d,%d",slot,st->accel[0],st->accel[1],st->accel[2],st->gyro[0],st->gyro[1],st->gyro[2]);
        last[slot]=*st;printed[slot]=now_ms();
    }
    return;
#endif
    vpad_update(slot, st);
}

/* The menu's PS button: the same as holding PS on that pad. */
#define PS_HOLD_MS 700
static int web_press_ps(int slot)
{
#ifdef ANYPAD_RAW_DIAGNOSTIC
    (void)slot;return 0;
#else
    return g_vpad_ok && vpad_press_ps(slot, PS_HOLD_MS);
#endif
}

static void on_disconnect(void *ud, int slot)
{
    aim_setup_disconnect(slot);
    (void)ud;
    if(slot>=0 && slot<HOST_MAX_PADS){
        g_studio_pid[slot]=0;g_studio_seen[slot]=0;
        if(g_move_stream[slot])movebridge_disconnect(slot);
        g_move_stream[slot]=0;g_move_player[slot]=-1;
        int player,role;if(studio_assignment(slot,&player,&role)&&role==ROLE_NAV)navbridge_player_close(player);g_device[slot]=-1;
        player_registry next=g_players;unsigned char active[PLAYER_DEVICES]={0};for(int i=0;i<HOST_MAX_PADS;i++)if(g_device[i]>=0)active[g_device[i]]=1;players_release_offline(&next,active);(void)studio_commit_registry(&next);
    }
#ifndef ANYPAD_RAW_DIAGNOSTIC
    vpad_remove(slot);
#endif
    notify("Move Management: controller %d disconnected", slot + 1);
}

/* ---- Bluetooth, one try at a time ------------------------------------------ */

/* The host calls this whenever it waits for the chip, so the menu answers
 * even while a Bluetooth try is in progress (it used to freeze for ~5 s each
 * time, and the browser's pile-up of connections got "busy"). */
static void on_idle(void *ud)
{
    (void)ud;
    web_poll(g_web);
}

static void bt_begin(void)
{
    g_bt_attempt = 0;
    g_bt_next = 0;
    g_bt_reason[0] = '\0';
    g_bt_state = WEB_BT_TRYING;
#ifndef ANYPAD_RAW_DIAGNOSTIC
    if (g_vpad_ok) vpad_creds_raise();
#endif
}

/* One try. The controller may answer only to a process that does not carry
 * raised credentials (not known), so the first
 * tries are made as they are and the rest with the original ones. Which
 * worked is logged. Virtual pads raise them again when they need to. */
static void bt_try(long now)
{
    host_events ev = { on_connect, on_state, on_disconnect, NULL, on_idle };
    hci_t hci;
    int attempt = ++g_bt_attempt;

    if (attempt == RAISED_TRIES + 1 && g_vpad_ok) {
#ifndef ANYPAD_RAW_DIAGNOSTIC
        log_line("Bluetooth silent with raised credentials; trying with the original ones");
        if (vpad_creds_restore()) g_bt_plain_creds = 1;
#endif
    }
#ifdef MOVE_STUDIO_EXTERNAL_BT
    if (hci_external_open(&hci)) {
        /* External sessions deliberately require fresh USB registration.
         * Never resurrect stale keys/identities after adapter replacement. */
        if(unlink(DB_PATH)&&errno!=ENOENT){
            log_line("Could not clear external registrations; refusing stale session");
            hci.ops->close(hci.ctx);g_bt_state=WEB_BT_FAILED;g_bt_next=now_ms()+10000;return;
        }
#else
    if (hci_usb_open(&hci)) {
#endif
        g_host = host_open(hci, DB_PATH, &ev);
        if (g_host) {
            g_usb_nav_seeded=0;g_usb_move_seeded=0;
            unsigned char local_addr[6];
            if(host_local_address(g_host,local_addr)){
                FILE *address_file=fopen(LOCAL_ADDRESS_PATH,"wb");
                if(address_file){fwrite(local_addr,1,6,address_file);fclose(address_file);}
            }
            g_bt_state = WEB_BT_OK;
            log_line("Bluetooth ready after %d tr%s%s", attempt, attempt == 1 ? "y" : "ies",
                     g_bt_plain_creds ? ", answering only with the original credentials" : "");
#ifdef MOVE_STUDIO_EXTERNAL_BT
            notify("Move Management: Bluetooth ready. Connect Move/Nav by USB to register them for this session.");
#else
            notify("Move Management: Bluetooth ready (%d saved controllers)", host_paired_count(g_host));
#endif
            return;
        }
        hci.ops->close(hci.ctx);
    }
    log_line("Bluetooth not ready (try %d of %d)", attempt, OPEN_TRIES);
    if (attempt >= OPEN_TRIES) {
        g_bt_state = WEB_BT_FAILED;
#ifdef MOVE_STUDIO_EXTERNAL_BT
        g_bt_next = now_ms() + 10000;
#endif
        snprintf(g_bt_reason, sizeof g_bt_reason,
                 "Bluetooth did not respond after %d attempts. Check the adapter connection and diagnostic log.", attempt);
        log_line("Bluetooth unavailable: %s", g_bt_reason);
        notify("Move Management: Bluetooth is unavailable.\nThe dashboard is still running; open it for details.");
        return;
    }
    g_bt_next = now + 3000;
}

static void bt_lost(const char *why)
{
    log_line("Bluetooth lost: %s", why);
    for(int p=0;p<4;p++)if(g_virtual_requested[p]){if(vpad_creds_raise()){vpad_remove(p);vpad_creds_restore();}g_virtual_requested[p]=0;}
    usb_register_stop(); /* join USB worker before releasing the USB runtime */
    host_close(g_host);                 /* drops every pad and its virtual pad */
    g_host = NULL;
#ifdef MOVE_STUDIO_EXTERNAL_BT
    unlink(DB_PATH);
    unlink(LOCAL_ADDRESS_PATH);
    notify("Move Management: adapter disconnected. Old connections cleared. Reconnect controllers by USB when Bluetooth is ready.");
#endif
    g_bt_state = WEB_BT_FAILED;
#ifdef MOVE_STUDIO_EXTERNAL_BT
    g_bt_next = now_ms() + 3000;
#endif
    snprintf(g_bt_reason, sizeof g_bt_reason, "Bluetooth connection lost (%s). Use Restart Bluetooth to retry.", why);
    notify("Move Management: Bluetooth connection lost.\nOpen the dashboard to check recovery or retry.");
}

/* ---- flags ---------------------------------------------------------------- */

static void check_flags(long now)
{
    static long last_second;

    if (now / 1000 == last_second) return;
    last_second = now / 1000;
    if (access(STOP_FLAG, F_OK) == 0) {
        unlink(STOP_FLAG);
        g_stop = 1;
    }
    if (g_host && access(PAIR_FLAG, F_OK) == 0) {
        unlink(PAIR_FLAG);
        host_pair(g_host, PAIR_SECONDS);
        notify("Move Management: pairing for %d seconds. Put the controller into pairing mode.", PAIR_SECONDS);
    }
}

int main(void)
{
    web_cfg wc;
    char ip[16], url[64];
    const char *why = "stop requested";
    long last_power = 0;

    mkdir(STATE_DIR, 0755);
    mkdir("/data/move-studio",0755);move_config_defaults(&g_studio_config);
    (void)move_config_load("/data/move-studio/layouts.ini",&g_studio_config);
    if(!studio_virtual_available()&&g_studio_config.virtual_combined_enabled){g_studio_config.virtual_combined_enabled=0;(void)move_config_save("/data/move-studio/layouts.ini",&g_studio_config);}
    (void)players_load("/data/move-studio/players.ini",&g_players);
    {unsigned char active[PLAYER_DEVICES]={0};players_release_offline(&g_players,active);}
    log_open(LOG_PATH);
    log_line("Move Management %s pid=%d", ANYPAD_VERSION,getpid());
    FILE*pidfile=fopen("/data/move-studio/bridge.pid","w");if(pidfile){fprintf(pidfile,"%d\n",getpid());fclose(pidfile);}
    (void)g_version_tag;

    if (!lock_take(LOCK_PATH)) {
        log_line("another instance is running");
        notify("Move Management is already running.");
        log_close();
        return 1;
    }
    unlink(STOP_FLAG);                  /* an old request is not for us */
    unlink("/data/porpoise-eye-optical.stop");
    syscall(SYS_thr_set_name, -1, "anypad");    /* how others find us */
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);
    signal(SIGPIPE, SIG_IGN);           /* a browser closing early */


    /* The menu first: it is what explains everything that follows. */
    memset(&wc, 0, sizeof wc);
    wc.host = &g_host;
    wc.stop = &g_web_stop;
    wc.retry = &g_retry;
    wc.bt_state = &g_bt_state;
    wc.bt_reason = g_bt_reason;
    wc.log_path = LOG_PATH;
    wc.version = ANYPAD_VERSION;
    wc.press_ps = web_press_ps;
    g_web = web_start(&wc, WEB_PORT);

    local_ip(ip);
    snprintf(url, sizeof url, "http://%s:%d/", ip, WEB_PORT);
#ifdef ANYPAD_RAW_DIAGNOSTIC
    if(g_web)notify("Move Management %s\nMenu: %s\nMove + Nav controller dashboard. Runs until stopped or rest mode.",ANYPAD_VERSION,url);
#else
    if (g_web)
        notify("Move Management %s\nDashboard: %s\nCreate a separate user and assign the controller when prompted. Return to your usual user to play.", ANYPAD_VERSION, url);
    else
        notify("Move Management %s\nCould not start the dashboard. Port %d may be in use.",
               ANYPAD_VERSION, WEB_PORT);

#endif
    sysinfo_log();

    /* The entry in the media row that opens the menu in the browser. Done
     * before the credentials are raised for the pads, as Payload Manager does
     * it with its own; /data/anypad/no_icon skips it, remove_icon undoes it. */
#ifndef ANYPAD_RAW_DIAGNOSTIC
    if (access(RM_ICON_FLAG, F_OK) == 0) {
        apps_remove_launcher();
        unlink(RM_ICON_FLAG);
    } else if (access(NO_ICON_FLAG, F_OK) != 0) {
        apps_install_launcher();
    }

    /* Without virtual pads there is nothing to connect controllers to:
     * Bluetooth is not touched at all, and the menu says why. */
#endif
#ifdef ANYPAD_RAW_DIAGNOSTIC
    g_vpad_ok = 1; /* transport allowed, no virtual pad/credential writes */
    notify("Move Management: No session timeout. Move controller service ready.");
#else
    g_vpad_ok = vpad_init();
#endif
    if (!g_vpad_ok) {
        g_bt_state = WEB_BT_FAILED;
        snprintf(g_bt_reason, sizeof g_bt_reason,
                 "Virtual controller creation failed. See the log. Bluetooth was not opened.");
        notify("Move Management: virtual controller creation failed.\nSee the dashboard or log. Bluetooth was not opened.");
    } else {
        bt_begin();
    }


    while (!g_stop && !g_web_stop) {
#ifdef ANYPAD_RAW_DIAGNOSTIC

#endif
        long now;

        if (g_host) host_poll(g_host, 4);
        else usleep(4000);
        studio_output_tick(now_ms());
        now = now_ms();
        web_poll(g_web);
#ifndef ANYPAD_RAW_DIAGNOSTIC
        if (g_vpad_ok) vpad_poll(now);
#endif

        /* Rest mode on its way: leave cleanly before it, not after. */
        if (now - last_power >= 200) {
            int ps = power_state();
            last_power = now;
            if (ps == POWER_GOING_TO_REST || ps == POWER_IN_REST) {
                why = "rest mode";
                notify("Move Management stopped for rest mode.\nLaunch it again after waking the PS5.");
                break;
            }
        }

        /* The Bluetooth: tries, loss, and the menu's retry button. */
        if (g_bt_state == WEB_BT_TRYING && now >= g_bt_next) bt_try(now);
#ifdef MOVE_STUDIO_EXTERNAL_BT
        if (g_bt_state == WEB_BT_FAILED && now >= g_bt_next) bt_begin();
#endif
        if (g_host && host_transport_lost(g_host)) bt_lost("adapter stopped responding");
        if (g_retry) {
            g_retry = 0;
            if (g_vpad_ok) {
                log_line("restarting Bluetooth at the user's request");
                usb_register_stop();
                if(g_host){host_close(g_host);g_host=NULL;}
                bt_begin();
            } else {
                log_line("retry ignored (virtual pads not available, or not failed)");
            }
        }

        if(g_host){
#ifndef MOVE_STUDIO_MANUAL_USB
            usb_register_start();usb_register_poll(g_host);
#endif
#ifndef MOVE_STUDIO_EXTERNAL_BT
            static long next_seed_check;
            if(now>=next_seed_check){
                next_seed_check=now+2000;
                DIR*dir=opendir(STATE_DIR);struct dirent*entry;
                while(dir&&(entry=readdir(dir))){unsigned a[6];char extra;int n=0;
                    if(sscanf(entry->d_name,"nav-%2x%2x%2x%2x%2x%2x.identity%n%c",a,a+1,a+2,a+3,a+4,a+5,&n,&extra)==6&&n&&!entry->d_name[n]){
                        char path[256];unsigned char addr[6],expected[6];for(int i=0;i<6;i++)expected[i]=(unsigned char)a[i];snprintf(path,sizeof path,STATE_DIR "/%s",entry->d_name);FILE*f=fopen(path,"rb");if(f){int ok=fread(addr,1,6,f)==6&&fgetc(f)==EOF;fclose(f);if(ok&&!memcmp(addr,expected,6))host_seed_usb_nav(g_host,addr);}continue;
                    }n=0;

                    if(sscanf(entry->d_name,"move-%2x%2x%2x%2x%2x%2x.calibration%n%c",a,a+1,a+2,a+3,a+4,a+5,&n,&extra)!=6||!n||entry->d_name[n])continue;
                    char path[256];snprintf(path,sizeof path,STATE_DIR "/%s",entry->d_name);FILE*f=fopen(path,"rb");unsigned char record[149],addr[6];for(int i=0;i<6;i++)addr[i]=(unsigned char)a[i];
                    if(f){int ok=fread(record,1,149,f)==149&&fgetc(f)==EOF;fclose(f);if(ok&&!memcmp(addr,record,6))host_seed_usb_move(g_host,addr);}
                }if(dir)closedir(dir);
                for(unsigned i=0;i<g_players.count;i++){player_device*d=&g_players.device[i];if(d->pid==0x042f)host_seed_usb_nav(g_host,d->addr);}

                if(!g_usb_move_seeded){
                    FILE*move_file=fopen(STATE_DIR "/move-usb-identity.bin","rb");
                    if(move_file){unsigned char report[16],local[6];int ok=fread(report,1,16,move_file)==16&&fgetc(move_file)==EOF;fclose(move_file);
                        if(ok&&report[0]==4&&host_local_address(g_host,local)&&!memcmp(local,report+10,6)&&host_seed_usb_move(g_host,report+1)){
                            g_usb_move_seeded=1;log_line("USB Move identity allowlisted; existing PS5 host address matched; no pairing write");
                        }
                    }
                }
                FILE*seed_file=!g_usb_nav_seeded?fopen(STATE_DIR "/nav-usb-address.bin","rb"):NULL;
                if(seed_file){unsigned char address[6];int ok=fread(address,1,6,seed_file)==6&&fgetc(seed_file)==EOF;fclose(seed_file);
                    if(ok&&host_seed_usb_nav(g_host,address)){g_usb_nav_seeded=1;log_line("USB Navigation identity allowlisted; waiting for wireless reconnect");}
                }
            }
#endif
        }
        check_flags(now);
    }
    if (g_web_stop) why = "stop from the menu";
    else if (g_stop && strcmp(why, "stop requested") == 0) why = "stop requested (flag or signal)";

    log_line("stopping: %s", why);
    web_stop(g_web);
    g_web = NULL; /* host_close may still pump the idle callback */
    for(int p=0;p<4;p++)if(g_virtual_requested[p]){if(vpad_creds_raise()){vpad_remove(p);vpad_creds_restore();}g_virtual_requested[p]=0;}
    usb_register_stop();
    host_close(g_host);                 /* our links; removes our virtual pads */
    g_host = NULL;
#ifndef ANYPAD_RAW_DIAGNOSTIC
    if (g_vpad_ok) {
        vpad_creds_restore();           /* leave the credentials as found */
    }
#endif
    FILE *eye_stop=fopen("/data/porpoise-eye-optical.stop","w");
    if(eye_stop){fputs("bridge stopped\n",eye_stop);fclose(eye_stop);}
    movebridge_close();
    navbridge_close();
    lock_release();
    log_line("stopped");
    log_close();
    notify("Move Management stopped.");
    return 0;
}
