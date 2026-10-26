#ifndef PH_MOVE_CONFIG_H
#define PH_MOVE_CONFIG_H

#include "pad.h"

#include <stddef.h>
#include <stdint.h>

#define MOVE_CONFIG_PLAYERS 4

typedef enum {
    MOVE_CFG_RAW = 0,
    MOVE_CFG_COMBINED_DS = 1
} move_config_preset;

typedef enum {
    MOVE_CFG_TRIGGER_NONE = 0,
    MOVE_CFG_TRIGGER_L2 = 1,
    MOVE_CFG_TRIGGER_R2 = 2
} move_config_trigger;

typedef enum {
    MOVE_CFG_AXIS_NONE = 0,
    MOVE_CFG_AXIS_LEFT = 1,
    MOVE_CFG_AXIS_RIGHT = 2
} move_config_axis;

/* Button arrays index these source-control lists; entries are a single PAD_*
 * bit or zero (disabled). Duplicate destinations are intentionally allowed. */
enum { MOVE_CFG_BUTTON_COUNT = 17, NAV_CFG_BUTTON_COUNT = 17 };

typedef struct {
    move_config_preset preset;
    uint32_t move_button[MOVE_CFG_BUTTON_COUNT];
    uint32_t nav_button[NAV_CFG_BUTTON_COUNT];
    move_config_trigger move_trigger;
    move_config_axis move_axis;
    move_config_axis nav_axis;
    unsigned motion; /* 0 off, 1 right stick, 2 gyro, 3 both */
    unsigned motion_speed; /* degrees/second for full stick deflection */
    unsigned motion_invert; /* bit 0 horizontal, bit 1 vertical */
} move_player_config;

typedef struct {
    int virtual_combined_enabled; /* Defaults off. */
    move_player_config player[MOVE_CONFIG_PLAYERS];
} move_config;

/* Source index order: Move Create, Options, PS, Move button, T, Cross, Circle,
 * Square, Triangle, Up, Right, Down, Left, L1, R1, L3, R3. Nav order uses the
 * same controls, with Move button/T indexes reserved and identity-mapped only
 * for real Nav controls. */
void move_config_defaults(move_config *config);
void move_config_preset_set(move_player_config *player,
                            move_config_preset preset);
int move_config_validate(const move_config *config);

/* Apply mappings to a fresh output. Move remains the base state; when combined
 * mode is enabled, Nav buttons and its configured stick are merged. A caller
 * with a disconnected Nav must pass NULL to avoid stale input. */
int move_config_combine(const pad_state *move, const pad_state *nav,
                        const move_player_config *config,
                        int virtual_combined_enabled, pad_state *out);

void move_config_second_move(const pad_state *left,pad_state *out);
void move_config_motion(const move_player_config *config, pad_state *out);

/* Strict versioned INI-like format; save writes a sibling temp then renames.
 * load replaces config only after the complete file validates. */
int move_config_save(const char *path, const move_config *config);
int move_config_load(const char *path, move_config *config);

#endif
