#ifndef PHONE_CAMERA_H
#define PHONE_CAMERA_H
#include <stdint.h>
#include <stddef.h>
int camera_phone_requested(void);
int camera_phone_run(int (*stop)(void),void (*frame)(const uint8_t*,unsigned,unsigned,size_t),void (*status)(const char*,const char*));
#endif
