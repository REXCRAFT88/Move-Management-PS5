#include "../camera/uvc-camera.c"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int empty_list,starts,delivered,frames,stops,selected;
static uvc_device_t *fake_list[2];
int camera_usb_load(void){return 1;}
const char *camera_usb_error(void){return "";}
uvc_error_t uvc_init(uvc_context_t **c,struct libusb_context *u){(void)u;*c=(uvc_context_t*)1;return 0;}
void uvc_exit(uvc_context_t*c){(void)c;}
uvc_error_t uvc_get_device_list(uvc_context_t*c,uvc_device_t***l){(void)c;fake_list[0]=empty_list?NULL:(uvc_device_t*)2;fake_list[1]=NULL;*l=fake_list;return 0;}
void uvc_free_device_list(uvc_device_t**l,uint8_t unref){(void)l;(void)unref;}
uvc_error_t uvc_open(uvc_device_t*d,uvc_device_handle_t**h){(void)d;*h=(uvc_device_handle_t*)3;return 0;}
void uvc_close(uvc_device_handle_t*h){(void)h;}
uvc_error_t uvc_get_stream_ctrl_format_size(uvc_device_handle_t*d,uvc_stream_ctrl_t*c,enum uvc_frame_format f,int w,int h,int fps){(void)d;(void)c;(void)w;(void)h;(void)fps;selected=f;return 0;}
uvc_error_t uvc_stream_open_ctrl(uvc_device_handle_t*d,uvc_stream_handle_t**s,uvc_stream_ctrl_t*c){(void)d;(void)c;*s=(uvc_stream_handle_t*)4;return 0;}
uvc_error_t uvc_stream_start(uvc_stream_handle_t*s,uvc_frame_callback_t*cb,void*u,uint8_t f){(void)s;(void)cb;(void)u;(void)f;starts++;return selected==UVC_FRAME_FORMAT_MJPEG?0:UVC_ERROR_IO;}
uvc_error_t uvc_stream_get_frame(uvc_stream_handle_t*s,uvc_frame_t**f,int32_t t){(void)s;(void)t;static uint8_t pixel[3]={1,2,3};static uvc_frame_t in; if(delivered++)return UVC_ERROR_NO_DEVICE;memset(&in,0,sizeof in);in.width=1;in.height=1;in.data=pixel;in.data_bytes=3;in.frame_format=UVC_FRAME_FORMAT_RGB;*f=&in;return 0;}
uvc_error_t uvc_stream_stop(uvc_stream_handle_t*s){(void)s;stops++;return 0;}
void uvc_stream_close(uvc_stream_handle_t*s){(void)s;}
uvc_frame_t *uvc_allocate_frame(size_t n){uvc_frame_t*f=calloc(1,sizeof*f);f->data=malloc(n);return f;}
void uvc_free_frame(uvc_frame_t*f){free(f->data);free(f);}
uvc_error_t uvc_any2rgb(uvc_frame_t*i,uvc_frame_t*o){memcpy(o->data,i->data,3);o->width=i->width;o->height=i->height;o->step=3;return 0;}
static int no_stop(void){return 0;}
static void frame_cb(const uint8_t*p,unsigned w,unsigned h,size_t stride){assert(w==1&&h==1&&stride==3&&p[0]==1&&p[2]==3);frames++;}
static void status_cb(const char*s,const char*m){(void)m;assert(!strcmp(s,"uvc")||!strcmp(s,"probing"));}
int main(void){assert(camera_uvc_present());assert(camera_uvc_run(no_stop,frame_cb,status_cb));assert(starts==7&&frames==1&&stops==1);empty_list=1;assert(!camera_uvc_present());assert(!camera_uvc_run(no_stop,frame_cb,status_cb));puts("UVC discovery, format fallback, RGB delivery, disconnect cleanup and absent camera checks passed");}
