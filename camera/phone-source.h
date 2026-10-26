#ifndef PHONE_SOURCE_H
#define PHONE_SOURCE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define PHONE_URL_FILE "/data/move-studio/phone-camera.url"
typedef struct {char host[16],path[192];unsigned port;} phone_source;
static inline int phone_source_parse(const char *url,phone_source *out){
 if(!url||!out||strlen(url)>255||strncmp(url,"http://",7))return 0;
 const char *p=url+7,*colon=strchr(p,':');if(!colon||colon-p<7||colon-p>15)return 0;
 memset(out,0,sizeof *out);memcpy(out->host,p,(size_t)(colon-p));
 unsigned a,b,c,d;char extra;if(sscanf(out->host,"%u.%u.%u.%u%c",&a,&b,&c,&d,&extra)!=4||a>255||b>255||c>255||d>255||!(a==10||(a==172&&b>=16&&b<=31)||(a==192&&b==168)))return 0;
 p=colon+1;if(!isdigit((unsigned char)*p))return 0;char *end=NULL;unsigned long port=strtoul(p,&end,10);if(port<1024||port>65535||!end||(*end&&*end!='/'))return 0;out->port=(unsigned)port;
 if(strlen(end)>=sizeof out->path)return 0;
 for(const char*q=end;*q;q++)if(!isalnum((unsigned char)*q)&&!strchr("/_-.?=&",*q))return 0;
 strcpy(out->path,(!*end||!strcmp(end,"/"))?"/video":end);return 1;
}
static inline int phone_source_read(char url[256]){FILE*f=fopen(PHONE_URL_FILE,"r");if(!f){url[0]=0;return 0;}size_t n=fread(url,1,255,f);int more=fgetc(f);fclose(f);url[n]=0;phone_source source;if(more!=EOF||!phone_source_parse(url,&source)){url[0]=0;return 0;}return 1;}
#endif
