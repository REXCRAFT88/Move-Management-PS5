#ifndef CAMERA_EXCLUSIONS_H
#define CAMERA_EXCLUSIONS_H
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define CAMERA_EXCLUSION_FILE "/data/move-studio/camera-exclusions.ini"
#define CAMERA_EXCLUSION_MAX 8
typedef struct {unsigned x,y,w,h;} camera_exclusion;
static inline int camera_exclusion_valid(camera_exclusion r){return r.x<320&&r.y<240&&r.w>=3&&r.h>=3&&r.w<=320-r.x&&r.h<=240-r.y;}
static inline unsigned camera_exclusions_load(camera_exclusion out[CAMERA_EXCLUSION_MAX]){
 FILE*f=fopen(CAMERA_EXCLUSION_FILE,"r");if(!f)return 0;unsigned n=0;char line[128],tail;while(fgets(line,sizeof line,f)&&n<CAMERA_EXCLUSION_MAX){camera_exclusion r;if(sscanf(line,"%u %u %u %u %c",&r.x,&r.y,&r.w,&r.h,&tail)!=4||!camera_exclusion_valid(r)){n=0;break;}out[n++]=r;}fclose(f);return n;
}
static inline int camera_exclusions_save(const camera_exclusion*r,unsigned n){if(n>CAMERA_EXCLUSION_MAX)return 0;FILE*f=fopen(CAMERA_EXCLUSION_FILE ".tmp","w");if(!f)return 0;int ok=1;for(unsigned i=0;i<n;i++)if(!camera_exclusion_valid(r[i])||fprintf(f,"%u %u %u %u\n",r[i].x,r[i].y,r[i].w,r[i].h)<0)ok=0;if(fflush(f)||fsync(fileno(f)))ok=0;if(fclose(f))ok=0;return ok&&!rename(CAMERA_EXCLUSION_FILE ".tmp",CAMERA_EXCLUSION_FILE);}
static inline int camera_excluded(const camera_exclusion*r,unsigned n,unsigned x,unsigned y){for(unsigned i=0;i<n&&i<CAMERA_EXCLUSION_MAX;i++)if(x>=r[i].x&&y>=r[i].y&&x-r[i].x<r[i].w&&y-r[i].y<r[i].h)return 1;return 0;}
#endif
