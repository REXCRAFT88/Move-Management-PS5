/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MOVE_AIM_SETUP_H
#define MOVE_AIM_SETUP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 0 idle, 1 center, 2 top-left, 3 bottom-right, 4 try, 5 rejected. */
typedef struct { int step,fresh;float x,y,half_x,half_y; } aim_setup_state;
void aim_setup_sample(int slot,const float gyro[3],const float accel[3],
                      uint32_t buttons,uint64_t timestamp);
int aim_setup_action(int slot,int action,uint64_t now); /* 0 begin, 1 cancel */
aim_setup_state aim_setup_status(int slot,uint64_t now);
int aim_setup_save(int slot,const char *path,uint64_t now);
void aim_setup_disconnect(int slot);
#ifdef __cplusplus
}
#endif
#endif
