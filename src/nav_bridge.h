#ifndef PH_PORPOISE_NAV_H
#define PH_PORPOISE_NAV_H

#include "pad.h"

#include <stdint.h>

/* Loopback-only experimental Navigation producer for Porpoise's v2 input
 * packet. The sequence number is process-local and includes invalidations. */
int navbridge_player_start(int player);
int navbridge_player_update(int player,const pad_state *state,uint64_t us);
void navbridge_player_close(int player);
int navbridge_start(void);
int navbridge_update(const pad_state *state, uint64_t monotonic_us);
void navbridge_disconnect(void);
void navbridge_close(void);

#endif
