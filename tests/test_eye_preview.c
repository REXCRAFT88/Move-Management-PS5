#include "../camera/eye-preview.h"
#include <assert.h>
#include "../camera/rgb-normalize.h"
#include <stdio.h>
static uint8_t raw[640*480],bmp[EYE_PREVIEW_BYTES];
int main(void){raw[0]=20;raw[1]=100;raw[640]=200;raw[641]=40;unsigned s=476*640+636;raw[s+1]=77;eye_preview_build(bmp,raw);assert(bmp[0]=='B'&&bmp[1]=='M');assert(bmp[18]==160&&bmp[22]==136&&bmp[25]==255);assert(bmp[54]==200&&bmp[55]==30&&bmp[56]==100);assert(bmp[EYE_PREVIEW_BYTES-1]==77);uint8_t color[3]={70,80,90};assert(camera_rgb_normalize(raw,color,1,1,3));assert(raw[0]==80&&raw[1]==70&&raw[640]==90&&raw[641]==80);assert(!camera_rgb_normalize(raw,color,1,1,2));puts("preview BMP dimensions, Bayer colors and corner orientation passed");}
