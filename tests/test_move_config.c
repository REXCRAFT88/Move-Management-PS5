#define _POSIX_C_SOURCE 200809L
#include "move_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void test_defaults_and_presets(void) {
    move_config c;
    move_config_defaults(&c);
    assert(!c.virtual_combined_enabled);
    for (unsigned p = 0; p < MOVE_CONFIG_PLAYERS; ++p) {
        assert(c.player[p].preset == MOVE_CFG_COMBINED_DS);
        assert(c.player[p].move_button[3] == PAD_CROSS); /* physical Move */
        assert(c.player[p].move_button[5] == PAD_L1); /* Move Cross */
        assert(c.player[p].move_button[4] == PAD_R2);
        assert(c.player[p].move_trigger == MOVE_CFG_TRIGGER_R2);
        assert(c.player[p].nav_button[13] == PAD_L1);
        assert(c.player[p].nav_button[4] == PAD_L2);
        assert(c.player[p].nav_button[5] == PAD_CROSS);
        assert(c.player[p].nav_axis == MOVE_CFG_AXIS_LEFT);
    }
    move_config_preset_set(&c.player[0], MOVE_CFG_RAW);
    assert(c.player[0].move_button[3] == PAD_TOUCHPAD);
    assert(c.player[0].move_button[5] == PAD_CROSS);
    assert(c.player[0].move_trigger == MOVE_CFG_TRIGGER_L2);
}

static void test_combination(void) {
    move_config c;
    pad_state move, nav, out;
    move_config_defaults(&c);
    pad_state_reset(&move);
    pad_state_reset(&nav);
    move.buttons = PAD_TOUCHPAD | PAD_CROSS | PAD_CIRCLE | PAD_L2;
    move.l2 = 201;
    nav.buttons = PAD_CROSS | PAD_L1 | PAD_L2;
    nav.lx = 20; nav.ly = 220; nav.l2 = 73;

    assert(move_config_combine(&move, &nav, &c.player[0], 0, &out));
    assert(out.buttons == (PAD_CROSS | PAD_L1 | PAD_CIRCLE | PAD_R2));
    assert(out.r2 == 201 && out.l2 == 0);
    assert(out.lx == 128 && out.ly == 128);

    assert(move_config_combine(&move, &nav, &c.player[0], 1, &out));
    assert(out.buttons == (PAD_L1 | PAD_CIRCLE | PAD_R2 | PAD_CROSS | PAD_L2));
    assert(out.lx == 20 && out.ly == 220);
    assert(out.r2 == 201 && out.l2 == 73);

    /* Remapping Nav L2 must also route its analog pressure correctly. */
    c.player[0].nav_button[4] = PAD_R2;
    move.l2 = 0;
    assert(move_config_combine(&move, &nav, &c.player[0], 1, &out));
    assert(out.l2 == 0 && out.r2 == 73);
    c.player[0].nav_button[4] = PAD_CIRCLE;
    assert(move_config_combine(&move, &nav, &c.player[0], 1, &out));
    assert(out.l2 == 0 && out.r2 == 0);
    c.player[0].nav_button[4] = PAD_L2;
    move.l2 = 201;

    /* A missing Nav state contributes no stale buttons, axes or triggers. */
    assert(move_config_combine(&move, NULL, &c.player[0], 1, &out));
    assert(out.buttons == (PAD_CROSS | PAD_L1 | PAD_CIRCLE | PAD_R2));
    assert(out.lx == 128 && out.ly == 128 && out.l2 == 0);
}

static void test_validation_and_file(void) {
    move_config c, loaded;
    char path[128];
    move_config_defaults(&c);
    c.virtual_combined_enabled = 1;
    c.player[2].motion=1;c.player[2].motion_speed=180;c.player[2].motion_invert=3;
    c.player[2].move_button[5] = PAD_L1; /* Deliberate duplicate destination. */
    assert(move_config_validate(&c));
    c.player[1].nav_button[0] = PAD_CROSS | PAD_CIRCLE;
    assert(!move_config_validate(&c));
    c.player[1].nav_button[0] = PAD_CREATE;

    snprintf(path, sizeof path, "/tmp/move-config-test-%ld.ini", (long)getpid());
    unlink(path);
    assert(move_config_save(path, &c));
    move_config_defaults(&loaded);
    assert(move_config_load(path, &loaded));
    assert(loaded.virtual_combined_enabled == 1);
    assert(loaded.player[2].motion==1&&loaded.player[2].motion_speed==180&&loaded.player[2].motion_invert==3);
    assert(loaded.player[2].move_button[5] == PAD_L1);
    assert(loaded.player[1].nav_button[0] == PAD_CREATE);

    FILE *f = fopen(path, "w");
    assert(f);
    fputs("version=1\ncombined=0\np0.m.0=0xdeadbeef\n", f);
    fclose(f);
    move_config before = loaded;
    assert(!move_config_load(path, &loaded));
    assert(memcmp(&before, &loaded, sizeof loaded) == 0);
    unlink(path);
}

static void test_motion(void){
    move_config c;move_config_defaults(&c);pad_state out;pad_state_reset(&out);
    out.calibrated_motion=1;out.angular_velocity[1]=-4.0f;
    move_config_motion(&c.player[0],&out);assert(out.rx==255&&out.ry==128&&out.calibrated_motion);
    out.angular_velocity[1]=0;out.angular_velocity[0]=4;
    move_config_motion(&c.player[0],&out);assert(out.rx==128&&out.ry==0);
    c.player[0].motion_invert=2;move_config_motion(&c.player[0],&out);assert(out.ry==255);
    out.angular_velocity[0]=0.01f;out.angular_velocity[2]=5;
    move_config_motion(&c.player[0],&out);assert(out.rx==128&&out.ry==128); /* roll is not pitch */
    c.player[0].motion=2;out.rx=151;move_config_motion(&c.player[0],&out);assert(out.rx==151&&out.calibrated_motion);
    c.player[0].motion=1;move_config_motion(&c.player[0],&out);assert(!out.calibrated_motion&&!out.has_motion);
    out.calibrated_motion=1;out.angular_velocity[1]=__builtin_nanf("");move_config_motion(&c.player[0],&out);assert(out.rx==128);
    c.player[0].motion_speed=0;assert(!move_config_validate(&c));
}

int main(void) {
    test_defaults_and_presets();
    test_combination();
    test_motion();
    {pad_state primary,left;pad_state_reset(&primary);pad_state_reset(&left);primary.buttons=PAD_R2;primary.r2=200;primary.calibrated_motion=1;primary.angular_velocity[0]=1;
     left.buttons=PAD_L2|PAD_TOUCHPAD;left.l2=123;left.gyro[0]=1000;move_config_second_move(&left,&primary);
     assert(primary.buttons==(PAD_R2|PAD_L2|PAD_L1)&&primary.l2==123&&primary.r2==200);assert(primary.angular_velocity[0]==1);}

    test_validation_and_file();
    puts("move configuration tests passed");
    return 0;
}
