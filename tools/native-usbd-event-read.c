/* Finite Sony-service event-read comparison, interface 0 only.
 * No HCI commands, ACL traffic, resets, pairing changes or raw ugen opens. */
#include <dlfcn.h>
#include <libusb.h>
#include <stdio.h>
#include <unistd.h>
#include "util.h"
int main(void){
 FILE*f=fopen("/data/move-studio/native-usbd-event-read.log","w");if(!f)return 1;setvbuf(f,NULL,_IOLBF,0);
 void*m=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);if(!m)goto done;
 int(*init)(void)=dlsym(m,"sceUsbdInit");void(*quit)(void)=dlsym(m,"sceUsbdExit");
 libusb_device_handle*(*open_id)(uint16_t,uint16_t)=dlsym(m,"sceUsbdOpenDeviceWithVidPid");
 void(*close_dev)(libusb_device_handle*)=dlsym(m,"sceUsbdClose");
 int(*claim)(libusb_device_handle*,int)=dlsym(m,"sceUsbdClaimInterface");int(*release)(libusb_device_handle*,int)=dlsym(m,"sceUsbdReleaseInterface");
 libusb_device*(*device)(libusb_device_handle*)=dlsym(m,"sceUsbdGetDevice");
 int(*config)(libusb_device*,uint8_t,struct libusb_config_descriptor**)=dlsym(m,"sceUsbdGetConfigDescriptor");
 void(*free_cfg)(struct libusb_config_descriptor*)=dlsym(m,"sceUsbdFreeConfigDescriptor");
 int(*intr)(libusb_device_handle*,unsigned char,unsigned char*,int,int*,unsigned int)=dlsym(m,"sceUsbdInterruptTransfer");
 if(!init||!quit||!open_id||!close_dev||!claim||!release||!device||!config||!free_cfg||!intr)goto done;
 int rc=init();fprintf(f,"Sony USB init=%d\n",rc);if(rc)goto done;
 libusb_device_handle*h=open_id(0x1286,0x2059);if(!h){quit();goto done;}
 unsigned char endpoint=0;struct libusb_config_descriptor*c=NULL;
 if(!config(device(h),0,&c)&&c){
  for(int i=0;i<c->bNumInterfaces;i++)for(int j=0;j<c->interface[i].num_altsetting;j++){
   const struct libusb_interface_descriptor*a=&c->interface[i].altsetting[j];
   if(a->bInterfaceNumber||a->bAlternateSetting||a->bInterfaceClass!=0xe0||a->bInterfaceSubClass!=1||a->bInterfaceProtocol!=1)continue;
   for(int k=0;k<a->bNumEndpoints;k++)if((a->endpoint[k].bmAttributes&3)==3&&(a->endpoint[k].bEndpointAddress&0x80))endpoint=a->endpoint[k].bEndpointAddress;
  }
  free_cfg(c);
 }
 if(!endpoint){fputs("No verified interface 0 event endpoint\n",f);close_dev(h);quit();goto done;}
 rc=claim(h,0);fprintf(f,"claim=%d endpoint=%02x\n",rc,endpoint);if(rc){close_dev(h);quit();goto done;}
 long end=now_ms()+45000;unsigned reads=0,bytes=0;fputs("45-second event-read window active; no HCI commands\n",f);
 while(now_ms()<end){unsigned char b[64];int n=0;rc=intr(h,endpoint,b,sizeof b,&n,10);reads++;if(n>0&&n<=64){bytes+=(unsigned)n;fprintf(f,"event bytes=%d first_code=%02x rc=%d\n",n,b[0],rc);}else usleep(1000);}
 fprintf(f,"reads=%u bytes=%u release=%d\n",reads,bytes,release(h,0));close_dev(h);quit();
done:fputs("diagnostic complete\n",f);fclose(f);return 0;
}
