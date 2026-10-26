#include "phone-camera.h"
#include "phone-source.h"
#include "mjpeg-stream.h"
#include "uvc-camera.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
static char attempted[256];static long last_attempt;
static long phone_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (long)t.tv_sec*1000+t.tv_nsec/1000000;}
int camera_phone_requested(void){char url[256];return phone_source_read(url)&&(strcmp(url,attempted)||phone_ms()-last_attempt>=30000);}
typedef struct {void(*frame)(const uint8_t*,unsigned,unsigned,size_t);unsigned count;} phone_frames;
static void phone_jpeg(const uint8_t*p,size_t n,void*ctx){phone_frames*c=ctx;if(camera_jpeg_decode(p,n,c->frame))c->count++;}
static int wait_socket(int fd,int write){fd_set set;FD_ZERO(&set);FD_SET(fd,&set);struct timeval tv={0,100000};return select(fd+1,write?NULL:&set,write?&set:NULL,NULL,&tv);}
int camera_phone_run(int(*stop)(void),void(*frame)(const uint8_t*,unsigned,unsigned,size_t),void(*status)(const char*,const char*)){
 char url[256];phone_source source;if(!phone_source_read(url)||!phone_source_parse(url,&source))return 0;
 strcpy(attempted,url);last_attempt=phone_ms();int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return 0;
 fcntl(fd,F_SETFL,O_NONBLOCK);struct sockaddr_in a={0};a.sin_family=AF_INET;
#if defined(__FreeBSD__) || defined(__ORBIS__)
 a.sin_len=sizeof a;
#endif
 a.sin_port=htons((uint16_t)source.port);if(inet_pton(AF_INET,source.host,&a.sin_addr)!=1){close(fd);return 0;}
 int rc=connect(fd,(struct sockaddr*)&a,sizeof a);long deadline=phone_ms()+2000;
 if(rc<0&&errno!=EINPROGRESS){close(fd);status("phone-offline","Phone camera connection failed; using USB camera");return 0;}
 while(rc<0&&!stop()&&phone_ms()<deadline){int ready=wait_socket(fd,1);if(ready<0)break;if(!ready)continue;int error=0;socklen_t size=sizeof error;if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size)||error)break;rc=0;}
 if(rc||stop()){close(fd);return 0;}
 char request[512];int length=snprintf(request,sizeof request,"GET %s HTTP/1.1\r\nHost: %s:%u\r\nAccept: multipart/x-mixed-replace\r\nConnection: close\r\n\r\n",source.path,source.host,source.port);size_t sent=0;
 while(sent<(size_t)length&&!stop()&&phone_ms()<deadline){ssize_t n=send(fd,request+sent,(size_t)length-sent,0);if(n>0)sent+=(size_t)n;else if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))wait_socket(fd,1);else break;}
 if(sent!=(size_t)length){close(fd);return 0;}
 uint8_t *jpeg=malloc(MJPEG_MAX_FRAME);mjpeg_stream*stream=malloc(sizeof *stream);if(!jpeg||!stream){free(jpeg);free(stream);close(fd);return 0;}
 phone_frames frames={frame,0};mjpeg_init(stream,jpeg,phone_jpeg,&frames);long last_frame=phone_ms(),last_check=0;unsigned previous=0;
 status("phone-connecting","Connected to phone; waiting for camera frames");
 while(!stop()&&phone_ms()-last_frame<8000){
  if(phone_ms()-last_check>=500){char current[256];last_check=phone_ms();if(!phone_source_read(current)||strcmp(current,url))break;}
  if(wait_socket(fd,0)<0)break;
  uint8_t input[16384];ssize_t n=recv(fd,input,sizeof input,0);if(!n)break;if(n<0){if(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)continue;break;}
  mjpeg_feed(stream,input,(size_t)n);if(stream->failed||stream->complete)break;
  if(frames.count!=previous){if(!previous)status("phone","Phone MJPEG camera active");previous=frames.count;last_frame=phone_ms();}
 }
 if(stream->failed)status("phone-offline","Phone sent an unsupported camera stream");
 if(!stop())status("phone-offline",stream->failed?"Phone stream format error; falling back to USB":"Phone stream interrupted; falling back to USB");
 free(jpeg);free(stream);close(fd);return frames.count>0;
}
