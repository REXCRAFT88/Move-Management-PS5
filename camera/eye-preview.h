/* Live preview: 160x120 top-down BGR BMP, loopback only; no disk storage. */
#ifndef EYE_PREVIEW_H
#define EYE_PREVIEW_H
#include <stdint.h>
#include <string.h>
#define EYE_PREVIEW_BYTES (54u+160u*120u*3u)
#define EYE_PREVIEW_PORT 9991
static inline void eye_preview_u32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static inline void eye_preview_build(uint8_t *out,const uint8_t *raw){
 memset(out,0,54);out[0]='B';out[1]='M';eye_preview_u32(out+2,EYE_PREVIEW_BYTES);out[10]=54;out[14]=40;
 eye_preview_u32(out+18,160);eye_preview_u32(out+22,(uint32_t)-120);out[26]=1;out[28]=24;eye_preview_u32(out+34,160*120*3);
 for(unsigned y=0;y<120;y++)for(unsigned x=0;x<160;x++){
  unsigned s=(y*4)*640+x*4,d=54+(y*160+x)*3;
  out[d]=raw[s+640];out[d+1]=(uint8_t)(((unsigned)raw[s]+raw[s+641])/2);out[d+2]=raw[s+1];
 }
}
#endif
