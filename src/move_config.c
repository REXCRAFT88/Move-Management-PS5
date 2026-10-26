#define _POSIX_C_SOURCE 200809L
#include "move_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const uint32_t source_buttons[MOVE_CFG_BUTTON_COUNT] = {
    PAD_CREATE, PAD_OPTIONS, PAD_PS, PAD_TOUCHPAD, PAD_L2, PAD_CROSS,
    PAD_CIRCLE, PAD_SQUARE, PAD_TRIANGLE, PAD_UP, PAD_RIGHT, PAD_DOWN,
    PAD_LEFT, PAD_L1, PAD_R1, PAD_L3, PAD_R3
};

static const uint32_t allowed_buttons =
    PAD_CREATE | PAD_L3 | PAD_R3 | PAD_OPTIONS | PAD_UP | PAD_RIGHT |
    PAD_DOWN | PAD_LEFT | PAD_L2 | PAD_R2 | PAD_L1 | PAD_R1 | PAD_TRIANGLE |
    PAD_CIRCLE | PAD_CROSS | PAD_SQUARE | PAD_PS | PAD_TOUCHPAD;

static uint32_t move_ds_button(unsigned i) {
    if (i == 3) return PAD_CROSS;
    if (i == 5) return PAD_L1;
    if (i == 4) return PAD_R2;
    return source_buttons[i];
}

void move_config_preset_set(move_player_config *player,
                            move_config_preset preset) {
    if (!player) return;
    memset(player, 0, sizeof *player);
    player->preset = preset;
    for (unsigned i = 0; i < MOVE_CFG_BUTTON_COUNT; ++i) {
        player->move_button[i] = preset == MOVE_CFG_COMBINED_DS ?
                                 move_ds_button(i) : source_buttons[i];
        player->nav_button[i] = source_buttons[i];
    }
    player->move_trigger = preset == MOVE_CFG_COMBINED_DS ?
                           MOVE_CFG_TRIGGER_R2 : MOVE_CFG_TRIGGER_L2;
    player->move_axis = MOVE_CFG_AXIS_NONE;
    player->nav_axis = MOVE_CFG_AXIS_LEFT;
    player->motion=3;player->motion_speed=120;
}

void move_config_defaults(move_config *config) {
    if (!config) return;
    memset(config, 0, sizeof *config);
    for (unsigned i = 0; i < MOVE_CONFIG_PLAYERS; ++i)
        move_config_preset_set(&config->player[i], MOVE_CFG_COMBINED_DS);
}

static int valid_button(uint32_t button) {
    return button == 0 || ((button & (button - 1u)) == 0 &&
                           (button & allowed_buttons) != 0);
}

int move_config_validate(const move_config *config) {
    if (!config || (config->virtual_combined_enabled != 0 &&
                    config->virtual_combined_enabled != 1)) return 0;
    for (unsigned p = 0; p < MOVE_CONFIG_PLAYERS; ++p) {
        const move_player_config *c = &config->player[p];
        if(c->motion>3||c->motion_speed<20||c->motion_speed>360||c->motion_invert>3)return 0;
        if (c->preset != MOVE_CFG_RAW && c->preset != MOVE_CFG_COMBINED_DS)
            return 0;
        if (c->move_trigger < MOVE_CFG_TRIGGER_NONE ||
            c->move_trigger > MOVE_CFG_TRIGGER_R2 ||
            c->move_axis < MOVE_CFG_AXIS_NONE ||
            c->move_axis > MOVE_CFG_AXIS_RIGHT ||
            c->nav_axis < MOVE_CFG_AXIS_NONE ||
            c->nav_axis > MOVE_CFG_AXIS_RIGHT) return 0;
        for (unsigned i = 0; i < MOVE_CFG_BUTTON_COUNT; ++i)
            if (!valid_button(c->move_button[i]) ||
                !valid_button(c->nav_button[i])) return 0;
    }
    return 1;
}

static void map_buttons(uint32_t in, const uint32_t map[MOVE_CFG_BUTTON_COUNT],
                        uint32_t *out) {
    for (unsigned i = 0; i < MOVE_CFG_BUTTON_COUNT; ++i)
        if ((in & source_buttons[i]) && map[i]) *out |= map[i];
}

static void set_axis(pad_state *out, move_config_axis target,
                     const pad_state *in) {
    if (target == MOVE_CFG_AXIS_LEFT) {
        out->lx = in->lx; out->ly = in->ly;
    } else if (target == MOVE_CFG_AXIS_RIGHT) {
        out->rx = in->lx; out->ry = in->ly;
    }
}

static void set_trigger(pad_state *out, move_config_trigger target,
                        uint8_t value) {
    if (target == MOVE_CFG_TRIGGER_L2 && value > out->l2) out->l2 = value;
    if (target == MOVE_CFG_TRIGGER_R2 && value > out->r2) out->r2 = value;
}

int move_config_combine(const pad_state *move, const pad_state *nav,
                        const move_player_config *config,
                        int virtual_combined_enabled, pad_state *out) {
    if (!move || !config || !out || !move_config_validate(&(move_config){
            .virtual_combined_enabled = !!virtual_combined_enabled,
            .player = { *config, *config, *config, *config }
        })) return 0;
    pad_state_reset(out);
    *out = *move;
    out->buttons = 0;
    out->lx = out->ly = out->rx = out->ry = 128;
    out->l2 = out->r2 = 0;
    map_buttons(move->buttons, config->move_button, &out->buttons);
    if (config->move_axis != MOVE_CFG_AXIS_NONE) {
        set_axis(out, config->move_axis, move);
    }
    if (config->move_trigger == MOVE_CFG_TRIGGER_L2) out->l2 = move->l2;
    else if (config->move_trigger == MOVE_CFG_TRIGGER_R2) out->r2 = move->l2;
    else { out->l2 = 0; out->r2 = 0; }

    if (virtual_combined_enabled && nav) {
        map_buttons(nav->buttons, config->nav_button, &out->buttons);
        set_axis(out, config->nav_axis, nav);
        set_trigger(out,config->nav_button[4]==PAD_L2?MOVE_CFG_TRIGGER_L2:config->nav_button[4]==PAD_R2?MOVE_CFG_TRIGGER_R2:MOVE_CFG_TRIGGER_NONE,nav->l2);
        set_trigger(out, MOVE_CFG_TRIGGER_R2, nav->r2);
        if (nav->has_battery) {
            out->has_battery = 1;
            out->battery_pct = nav->battery_pct;
            out->charging = nav->charging;
        }
    }
    return 1;
}

void move_config_second_move(const pad_state*left,pad_state*out){
 if(!left||!out)return;
 out->buttons|=left->buttons&~(PAD_L2|PAD_TOUCHPAD|PAD_CROSS);
 if(left->buttons&(PAD_TOUCHPAD|PAD_CROSS))out->buttons|=PAD_L1;
 if(left->buttons&PAD_L2)out->buttons|=PAD_L2;
 out->l2=left->l2;
}
static uint8_t motion_stick(float rate,unsigned speed,int invert){
    float degrees=rate*57.2957795131f;
    if(!__builtin_isfinite(degrees))return 128;
    if(invert)degrees=-degrees;
    float magnitude=degrees<0?-degrees:degrees;
    if(magnitude<=1.5f)return 128;
    float v=(magnitude-1.5f)/((float)speed-1.5f);
    if(v>1)v=1;
    return degrees<0?(uint8_t)(128-v*128):(uint8_t)(128+v*127);
}
void move_config_motion(const move_player_config *c,pad_state *out){
    if(!c||!out)return;
    out->has_motion=0;
    if(!out->calibrated_motion||!c->motion){out->calibrated_motion=0;return;}
    if(c->motion&1){
        out->rx=motion_stick(-out->angular_velocity[1],c->motion_speed,c->motion_invert&1);
        out->ry=motion_stick(-out->angular_velocity[0],c->motion_speed,c->motion_invert&2);
    }
    if(!(c->motion&2))out->calibrated_motion=0;
}

static int parse_long(const char *s, int base, long *value) {
    char *end;
    long v;
    if (!s || !*s) return 0;
    v = strtol(s, &end, base);
    if (*end) return 0;
    *value = v;
    return 1;
}

static int parse_key(move_config *c, const char *key, const char *value,
                     unsigned char seen[4][2][MOVE_CFG_BUTTON_COUNT + 7],
                     unsigned char *version_seen,
                     unsigned char *combined_seen) {
    long v;
    int player, kind, index = 0;
    if (!strcmp(key, "version")) {
        if (*version_seen || !parse_long(value, 10, &v) || v != 1) return 0;
        *version_seen = 1; return 1;
    }
    if (!strcmp(key, "combined")) {
        if (*combined_seen || !parse_long(value, 10, &v) || v < 0 || v > 1)
            return 0;
        c->virtual_combined_enabled = (int)v;
        *combined_seen = 1; return 1;
    }
    if (sscanf(key, "p%d.%n", &player, &index) != 1 || !index ||
        player < 0 || player >= 4)
        return 0;
    const char *tail = key + index;
    move_player_config *p = &c->player[player];
    if (!strcmp(tail, "preset")) kind = 0;
    else if (!strcmp(tail, "trigger")) kind = 1;
    else if (!strcmp(tail, "move_axis")) kind = 2;
    else if (!strcmp(tail, "nav_axis")) kind = 3;
    else if (!strcmp(tail,"motion")) kind=4;
    else if (!strcmp(tail,"motion_speed")) kind=5;
    else if (!strcmp(tail,"motion_invert")) kind=6;
    else kind = -1;
    if (kind >= 0) {
        if (seen[player][0][kind] || !parse_long(value, 10, &v)) return 0;
        seen[player][0][kind] = 1;
        if (kind == 0) p->preset = (move_config_preset)v;
        else if (kind == 1) p->move_trigger = (move_config_trigger)v;
        else if (kind == 2) p->move_axis = (move_config_axis)v;
        else if(kind==3)p->nav_axis = (move_config_axis)v;
        else if(kind==4)p->motion=(unsigned)v;
        else if(kind==5)p->motion_speed=(unsigned)v;
        else p->motion_invert=(unsigned)v;
        return 1;
    }
    char *end;
    long idx;
    if (tail[0] != 'm' && tail[0] != 'n') return 0;
    if (tail[1] != '.') return 0;
    idx = strtol(tail + 2, &end, 10);
    if (*end || idx < 0 || idx >= MOVE_CFG_BUTTON_COUNT ||
        !parse_long(value, 0, &v) || v < 0 || v > UINT32_MAX) return 0;
    kind = tail[0] == 'm' ? 0 : 1;
    if (seen[player][kind][idx + 7]) return 0;
    seen[player][kind][idx + 7] = 1;
    if (kind == 0) p->move_button[idx] = (uint32_t)v;
    else p->nav_button[idx] = (uint32_t)v;
    return 1;
}

int move_config_save(const char *path, const move_config *config) {
    size_t n;
    char *tmp;
    FILE *f;
    int ok = 1;
    if (!path || !*path || !move_config_validate(config)) return 0;
    n = strlen(path);
    if (n > 4000) return 0;
    tmp = malloc(n + 5);
    if (!tmp) return 0;
    memcpy(tmp, path, n);
    memcpy(tmp + n, ".tmp", 5);
    f = fopen(tmp, "w");
    if (!f) { free(tmp); return 0; }
    if (fprintf(f, "version=1\ncombined=%d\n", config->virtual_combined_enabled) < 0)
        ok = 0;
    for (unsigned p = 0; ok && p < 4; ++p) {
        const move_player_config *c = &config->player[p];
        if (fprintf(f, "p%u.preset=%d\np%u.trigger=%d\np%u.move_axis=%d\n"
                       "p%u.nav_axis=%d\n", p, c->preset, p, c->move_trigger,
                    p, c->move_axis, p, c->nav_axis) < 0) ok = 0;
        if(fprintf(f,"p%u.motion=%u\np%u.motion_speed=%u\np%u.motion_invert=%u\n",p,c->motion,p,c->motion_speed,p,c->motion_invert)<0)ok=0;
        for (unsigned i = 0; ok && i < MOVE_CFG_BUTTON_COUNT; ++i)
            if (fprintf(f, "p%u.m.%u=0x%08x\np%u.n.%u=0x%08x\n", p, i,
                        c->move_button[i], p, i, c->nav_button[i]) < 0) ok = 0;
    }
    if (fflush(f) != 0 || ferror(f)) ok = 0;
    if (fclose(f) != 0) ok = 0;
    if (ok && rename(tmp, path) != 0) ok = 0;
    if (!ok) unlink(tmp);
    free(tmp);
    return ok;
}

int move_config_load(const char *path, move_config *config) {
    FILE *f;
    char line[160];
    move_config parsed;
    unsigned char seen[4][2][MOVE_CFG_BUTTON_COUNT + 7] = {{{0}}};
    unsigned char version_seen = 0, combined_seen = 0;
    if (!path || !config || !(f = fopen(path, "r"))) return 0;
    move_config_defaults(&parsed);
    while (fgets(line, sizeof line, f)) {
        char *eq, *key, *value, *end;
        size_t len = strlen(line);
        if (!len || line[len - 1] != '\n') { fclose(f); return 0; }
        line[--len] = '\0';
        key = line;
        while (*key == ' ' || *key == '\t') ++key;
        if (!*key || *key == '#') continue;
        eq = strchr(key, '=');
        if (!eq) { fclose(f); return 0; }
        *eq = '\0'; value = eq + 1;
        end = key + strlen(key);
        while (end > key && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
        while (*value == ' ' || *value == '\t') ++value;
        if (!parse_key(&parsed, key, value, seen, &version_seen,
                       &combined_seen)) { fclose(f); return 0; }
    }
    int read_error = ferror(f);
    int close_error = fclose(f) != 0;
    if (read_error || close_error || !version_seen || !combined_seen ||
        !move_config_validate(&parsed)) return 0;
    *config = parsed;
    return 1;
}
