#ifndef PH_PORPOISE_MOVE_H
#define PH_PORPOISE_MOVE_H

#include "pad.h"
#include <stdint.h>

/* Loopback-only v2 input producer. Calibration is the 143-byte ZCM1 blob
 * returned by the controller's calibration feature report. */
int movebridge_start(int slot, const uint8_t calibration[143]);
int movebridge_update(int slot, const pad_state *state, uint64_t monotonic_us);
void movebridge_set_color(int slot,unsigned color);
void movebridge_invalidate(int slot);
int movebridge_route(int slot,int player,int secondary);
int movebridge_motion(int slot, const pad_state *raw, pad_state *out);
void movebridge_disconnect(int slot);
void movebridge_close(void);
int movebridge_optical_status(int slot,uint64_t now_us,float bounds[4]);
/* Session-only stationary gyro zeroing; factory calibration is unchanged.
 * Status: 0 ready, 1 sampling, 2 complete, 3 moved/timed out, -1 unavailable. */
int movebridge_calibrate(int slot,int reset,uint64_t now_us);
void movebridge_calibration_sample(int slot,const pad_state *raw,uint64_t now_us);
int movebridge_calibration_status(int slot,uint64_t now_us,unsigned *percent);

#endif
