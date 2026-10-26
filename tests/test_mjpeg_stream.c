#include "../camera/mjpeg-stream.h"
#include "../camera/phone-source.h"
#include <assert.h>
#include <stdio.h>
static uint8_t storage[MJPEG_MAX_FRAME];static unsigned frames;
static void large_received(const uint8_t*p,size_t n,void*ctx){(void)ctx;assert(n==1024*1024&&p[0]==255&&p[1]==216&&p[n-2]==255&&p[n-1]==217);frames++;}
static void received(const uint8_t*p,size_t n,void*ctx){(void)ctx;assert(n==4&&p[0]==255&&p[3]==217);frames++;}
static void feed(mjpeg_stream*s,const char*p){mjpeg_feed(s,(const uint8_t*)p,strlen(p));}
int main(void){
 phone_source source;assert(phone_source_parse("http://192.168.0.42:8080/video",&source));assert(source.port==8080&&!strcmp(source.path,"/video"));
 assert(phone_source_parse("http://192.168.0.42:8080",&source)&&!strcmp(source.path,"/video"));
 assert(phone_source_parse("http://192.168.0.42:8080/",&source)&&!strcmp(source.path,"/video"));
 assert(phone_source_parse("http://192.168.0.42:8080/custom.mjpeg",&source)&&!strcmp(source.path,"/custom.mjpeg"));
 const char*invalid[]={"https://192.168.0.42:8080/video","http://127.0.0.1:8095/api/stop","http://8.8.8.8:8080/video","http://192.168.0.1:0/video","http://192.168.0.1:65536/video","http://user@192.168.0.1:8080/video","http://192.168.0.1:8080/video\r\nBad: x","http://192.168.0.1:8080/video\""};
 for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++)assert(!phone_source_parse(invalid[i],&source));
 mjpeg_stream s;mjpeg_init(&s,storage,received,NULL);feed(&s,"HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");
 const char body[]="--frame\r\nContent-Type: image/jpeg\r\nContent-Length: 4\r\n\r\n\xff\xd8\xff\xd9\r\n";
 for(size_t i=0;i<sizeof body-1;i++)mjpeg_feed(&s,(const uint8_t*)body+i,1);
 assert(frames==1&&!s.failed);
 mjpeg_init(&s,storage,received,NULL);feed(&s,"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");
 for(unsigned j=0;j<2;j++)for(size_t i=0;i<sizeof body-1;i+=3){size_t n=sizeof body-1-i;if(n>3)n=3;char h[30];snprintf(h,sizeof h,"%zx\r\n",n);feed(&s,h);mjpeg_feed(&s,(const uint8_t*)body+i,n);feed(&s,"\r\n");}
 feed(&s,"0\r\n\r\n");assert(s.complete&&!s.failed&&frames==3);
 mjpeg_init(&s,storage,received,NULL);feed(&s,"HTTP/1.1 302 Found\r\nContent-Type: multipart/x-mixed-replace\r\n\r\n");assert(s.failed);
 mjpeg_init(&s,storage,received,NULL);feed(&s,"HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace\r\n\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: 99999999\r\n\r\n");assert(s.failed);
 mjpeg_init(&s,storage,received,NULL);feed(&s,"HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n");assert(s.failed);
 /* A full-size JPEG is received over multiple socket reads, then another
  * frame starts in the same stream. Exercise the block-copy fast path. */
 mjpeg_init(&s,storage,large_received,NULL);
 feed(&s,"HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace\r\n\r\n");
 uint8_t block[16384];memset(block,42,sizeof block);
 unsigned before=frames;
 for(unsigned frame=0;frame<2;frame++){
  feed(&s,"--frame\r\nContent-Type: image/jpeg\r\nContent-Length: 1048576\r\n\r\n");
  for(unsigned i=0;i<64;i++){
   memset(block,42,sizeof block);
   if(!i){block[0]=255;block[1]=216;}
   if(i==63){block[16382]=255;block[16383]=217;}
   mjpeg_feed(&s,block,sizeof block);
  }
  feed(&s,"\r\n");
 }
 assert(!s.failed&&frames==before+2);
 puts("MJPEG raw/chunked framing, block copying, fragmented frames, URL restrictions and malformed input checks passed");
}
