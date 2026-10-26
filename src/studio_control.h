#ifndef MOVE_STUDIO_CONTROL_H
#define MOVE_STUDIO_CONTROL_H
#include "move_config.h"
#include "aim_setup.h"
int studio_aim_action(int slot,const char *action);
aim_setup_state studio_aim_status(int slot);
int studio_color(int slot);
int studio_pair(unsigned player,int primary_slot,int secondary_slot,unsigned kind);
int studio_assignment(int slot,int *player,int *role);
int studio_auto_assign(int slot,unsigned player);
int studio_assign(int slot,unsigned player,unsigned role);
const move_config *studio_config_get(void);
int studio_config_apply(int player,const char *key,unsigned value);
int studio_virtual_available(void);
const char *studio_virtual_status(void);
int studio_calibrate(int slot,int reset);
int studio_calibration_status(int slot,unsigned *percent);
#endif
