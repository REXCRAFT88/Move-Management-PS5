/* Convert/scaled RGB into the detector's fixed GRBG grid. */
#ifndef CAMERA_RGB_NORMALIZE_H
#define CAMERA_RGB_NORMALIZE_H
#include <stddef.h>
#include <stdint.h>
static inline int camera_rgb_normalize(uint8_t out[640*480],const uint8_t *rgb,unsigned width,unsigned height,size_t stride){
 if(!out||!rgb||!width||!height||width>4096||height>4096||stride<(size_t)width*3)return 0;
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++){
  const uint8_t*p=rgb+(size_t)(y*height/240)*stride+(x*width/320)*3;
  unsigned d=y*2*640+x*2;out[d]=p[1];out[d+1]=p[0];out[d+640]=p[2];out[d+641]=p[1];
 }
 return 1;
}
#endif
