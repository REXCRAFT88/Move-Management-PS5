/* Bounded HTTP + chunked-transfer + Content-Length multipart MJPEG parser. */
#ifndef MJPEG_STREAM_H
#define MJPEG_STREAM_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#define MJPEG_MAX_FRAME (2u*1024u*1024u)
typedef void (*mjpeg_frame_fn)(const uint8_t*,size_t,void*);
typedef struct {
 char header[8192],part[4096],chunk[40];size_t hn,pn,cn,needed,used,chunk_left;
 int headers_done,chunked,chunk_state,failed,complete;uint8_t *jpeg;
 mjpeg_frame_fn frame;void *ctx;
} mjpeg_stream;
static inline void mjpeg_init(mjpeg_stream*s,uint8_t*storage,mjpeg_frame_fn frame,void*ctx){memset(s,0,sizeof *s);s->jpeg=storage;s->frame=frame;s->ctx=ctx;}
static inline int mjpeg_decimal(const char*p,size_t*value){if(!isdigit((unsigned char)*p))return 0;size_t n=0;while(isdigit((unsigned char)*p)){if(n>MJPEG_MAX_FRAME/10)return 0;n=n*10+(unsigned)(*p++-'0');}while(*p==' '||*p=='\t')p++;if(*p||!n||n>MJPEG_MAX_FRAME)return 0;*value=n;return 1;}
static inline void mjpeg_payload(mjpeg_stream*s,uint8_t c){
 if(s->needed){s->jpeg[s->used++]=c;if(s->used==s->needed){if(s->used<4||s->jpeg[0]!=255||s->jpeg[1]!=216||s->jpeg[s->used-2]!=255||s->jpeg[s->used-1]!=217){s->failed=1;return;}s->frame(s->jpeg,s->used,s->ctx);s->used=s->needed=0;s->pn=0;}return;}
 if(s->pn>=sizeof s->part-1){s->failed=1;return;}s->part[s->pn++]=(char)c;s->part[s->pn]=0;
 if(s->pn<4||memcmp(s->part+s->pn-4,"\r\n\r\n",4))return;
 size_t length=0;int jpeg=0;char *save=NULL,*line=strtok_r(s->part,"\r\n",&save);
 while(line){if(!strncasecmp(line,"Content-Length:",15)){if(length){s->failed=1;return;}const char*p=line+15;while(*p==' ')p++;if(!mjpeg_decimal(p,&length)){s->failed=1;return;}}if(!strcasecmp(line,"Content-Type: image/jpeg"))jpeg=1;line=strtok_r(NULL,"\r\n",&save);}
 if(!length||!jpeg){s->failed=1;return;}s->needed=length;s->pn=0;
}
static inline void mjpeg_byte(mjpeg_stream*s,uint8_t c){
 if(s->failed||s->complete)return;
 if(!s->headers_done){
  if(s->hn>=sizeof s->header-1){s->failed=1;return;}s->header[s->hn++]=(char)c;s->header[s->hn]=0;
  if(s->hn<4||memcmp(s->header+s->hn-4,"\r\n\r\n",4))return;
  if(strncmp(s->header,"HTTP/1.1 200 ",13)&&strncmp(s->header,"HTTP/1.0 200 ",13)){s->failed=1;return;}
  int multipart=0;char*save=NULL,*line=strtok_r(s->header,"\r\n",&save);
  while(line){if(!strncasecmp(line,"Transfer-Encoding:",18)){const char*p=line+18;while(*p==' ')p++;if(strcasecmp(p,"chunked")){s->failed=1;return;}s->chunked=1;}if(!strncasecmp(line,"Content-Type: multipart/x-mixed-replace",37))multipart=1;if(!strncasecmp(line,"Content-Encoding:",17)){s->failed=1;return;}line=strtok_r(NULL,"\r\n",&save);}
  if(!multipart){s->failed=1;return;}s->headers_done=1;return;
 }
 if(!s->chunked){mjpeg_payload(s,c);return;}
 if(s->chunk_state==0){
  if(s->cn>=sizeof s->chunk-1){s->failed=1;return;}s->chunk[s->cn++]=(char)c;s->chunk[s->cn]=0;
  if(c!='\n')return;
  if(s->cn<3||s->chunk[s->cn-2]!='\r'){s->failed=1;return;}s->chunk[s->cn-2]=0;
  size_t n=0;for(size_t i=0;i<s->cn-2;i++){unsigned char ch=(unsigned char)s->chunk[i];if(!isxdigit(ch)){s->failed=1;return;}if(n>MJPEG_MAX_FRAME/16){s->failed=1;return;}n=n*16+(ch<='9'?(unsigned)(ch-'0'):(unsigned)tolower(ch)-'a'+10);}
  s->cn=0;if(!n){s->complete=1;return;}if(n>MJPEG_MAX_FRAME){s->failed=1;return;}s->chunk_left=n;s->chunk_state=1;return;
 }
 if(s->chunk_state==1){mjpeg_payload(s,c);if(!--s->chunk_left)s->chunk_state=2;return;}
 if(s->chunk_state==2){if(c!='\r')s->failed=1;else s->chunk_state=3;return;}
 if(c!='\n')s->failed=1;else s->chunk_state=0;
}
static inline void mjpeg_feed(mjpeg_stream*s,const uint8_t*p,size_t n){
 size_t i=0;
 while(i<n&&!s->failed&&!s->complete){
  /* JPEG bytes have no HTTP syntax. Copy whole spans, stopping at both
   * frame and HTTP chunk boundaries; headers still use the bounded parser. */
  if(s->headers_done&&s->needed&&(!s->chunked||s->chunk_state==1)){
   size_t take=s->needed-s->used;
   if(take>n-i)take=n-i;
   if(s->chunked&&take>s->chunk_left)take=s->chunk_left;
   memcpy(s->jpeg+s->used,p+i,take);s->used+=take;i+=take;
   if(s->chunked){s->chunk_left-=take;if(!s->chunk_left)s->chunk_state=2;}
   if(s->used==s->needed){
    if(s->used<4||s->jpeg[0]!=255||s->jpeg[1]!=216||s->jpeg[s->used-2]!=255||s->jpeg[s->used-1]!=217){s->failed=1;break;}
    s->frame(s->jpeg,s->used,s->ctx);s->used=s->needed=0;s->pn=0;
   }
  }else mjpeg_byte(s,p[i++]);
 }
}
#endif
