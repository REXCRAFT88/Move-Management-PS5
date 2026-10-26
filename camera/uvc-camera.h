#include <stddef.h>
#include <stdint.h>
int camera_jpeg_decode(const uint8_t*,size_t,void (*frame)(const uint8_t*,unsigned,unsigned,size_t));
int camera_uvc_present(void);
int camera_uvc_run(int (*stop)(void),void (*frame)(const uint8_t*,unsigned,unsigned,size_t),void (*status)(const char*,const char*));
