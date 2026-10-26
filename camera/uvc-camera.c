/* Standard UVC camera capture, isolated from controller input. */
#include "uvc-camera.h"
#include "ps5-usb.h"
#include <libuvc/libuvc.h>
#include <stdio.h>
#include <unistd.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 4096
#include "vendor/stb_image.h"

int camera_jpeg_decode(const uint8_t *data,size_t length,void (*frame)(const uint8_t*,unsigned,unsigned,size_t)){
 if(!data||!frame||length>2*1024*1024)return 0;
 int w,h,c;if(!stbi_info_from_memory(data,(int)length,&w,&h,&c)||w<=0||h<=0||w>1920||h>1080)return 0;
 uint8_t *pixels=stbi_load_from_memory(data,(int)length,&w,&h,&c,3);if(!pixels)return 0;
 frame(pixels,(unsigned)w,(unsigned)h,(size_t)w*3);stbi_image_free(pixels);return 1;
}

int camera_uvc_present(void){
 uvc_context_t *ctx=NULL;uvc_device_t **list=NULL;int found=0;
 if(!camera_usb_load()||uvc_init(&ctx,(struct libusb_context*)1))return 0;
 if(!uvc_get_device_list(ctx,&list)){found=list[0]!=NULL;uvc_free_device_list(list,1);}uvc_exit(ctx);return found;
}
int camera_uvc_run(int (*stop)(void),void (*frame)(const uint8_t*,unsigned,unsigned,size_t),void (*status)(const char*,const char*)){
 uvc_context_t *ctx=NULL;uvc_device_t **list=NULL;uvc_device_handle_t *dev=NULL;uvc_stream_handle_t *stream=NULL;uvc_frame_t *rgb=NULL;uvc_stream_ctrl_t ctrl;int started=0;
 if(uvc_init(&ctx,NULL)){status("waiting",camera_usb_error());return 0;}
 if(uvc_get_device_list(ctx,&list))goto done;
 for(int i=0;list[i]&&!stop();i++){
  if(uvc_open(list[i],&dev))continue;
  const int sizes[][2]={{640,480},{320,240},{1280,720}};int found=0;
  const enum uvc_frame_format formats[]={UVC_FRAME_FORMAT_YUYV,UVC_FRAME_FORMAT_MJPEG,UVC_FRAME_FORMAT_RGB};
  for(unsigned f=0;f<3&&!found;f++)for(unsigned s=0;s<3&&!found;s++)for(int fps=30;fps>=15&&!found;fps-=15){
   if(uvc_get_stream_ctrl_format_size(dev,&ctrl,formats[f],sizes[s][0],sizes[s][1],fps))continue;
   char attempt[96];snprintf(attempt,sizeof attempt,"UVC format %u: %dx%d at %d fps",(unsigned)formats[f],sizes[s][0],sizes[s][1],fps);status("probing",attempt);
   if(!uvc_stream_open_ctrl(dev,&stream,&ctrl)&&!uvc_stream_start(stream,NULL,NULL,0)){found=1;started=1;break;}
   if(stream){uvc_stream_close(stream);stream=NULL;}
  }
  if(found)break;
  if(stream){uvc_stream_close(stream);stream=NULL;}uvc_close(dev);dev=NULL;
 }
 if(!started)goto done;
 status("uvc","USB webcam active");rgb=uvc_allocate_frame(1280*720*3);if(!rgb)goto done;
 unsigned empty=0;
 while(!stop()){
  uvc_frame_t *in=NULL;int rc=uvc_stream_get_frame(stream,&in,300000);
  if(rc==UVC_ERROR_NO_DEVICE||rc==UVC_ERROR_IO)break;
  if(rc||!in){if(++empty>=10)break;continue;}empty=0;
  if(in->width>4096||in->height>4096||!in->width||!in->height||in->data_bytes>16*1024*1024)continue;
  if(in->frame_format==UVC_FRAME_FORMAT_MJPEG){
   int w,h,c;if(!stbi_info_from_memory(in->data,(int)in->data_bytes,&w,&h,&c)||w<=0||h<=0||w>1920||h>1080)continue;
   uint8_t *pixels=stbi_load_from_memory(in->data,(int)in->data_bytes,&w,&h,&c,3);
   if(pixels){frame(pixels,(unsigned)w,(unsigned)h,(size_t)w*3);stbi_image_free(pixels);}
  }else if(!uvc_any2rgb(in,rgb))frame(rgb->data,rgb->width,rgb->height,rgb->step?rgb->step:(size_t)rgb->width*3);
 }
done:
 if(rgb)uvc_free_frame(rgb);
 if(stream){uvc_stream_stop(stream);uvc_stream_close(stream);}
 if(dev)uvc_close(dev);
 if(list)uvc_free_device_list(list,1);
 if(ctx)uvc_exit(ctx);
 return started;
}
