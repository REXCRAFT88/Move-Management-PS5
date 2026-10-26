/* Finite comparison with raw ugen open: Sony USB service open only.
 * No kernel node opens, interface claims, HCI traffic or pairing changes.
 * Requires a verified native DualSense reconnect baseline and observer. */
#include <dlfcn.h>
#include <libusb.h>
#include <stdio.h>
#include <unistd.h>
#include "util.h"
int main(void){
#ifdef CLAIM_ONLY
 FILE*f=fopen("/data/move-studio/native-usbd-claim-only.log","w");
#else
 FILE*f=fopen("/data/move-studio/native-usbd-open-only.log","w");
#endif
 if(!f)return 1;setvbuf(f,NULL,_IOLBF,0);
 void*m=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);if(!m)goto done;
 int(*init)(void)=dlsym(m,"sceUsbdInit");void(*quit)(void)=dlsym(m,"sceUsbdExit");
 libusb_device_handle*(*open_id)(uint16_t,uint16_t)=dlsym(m,"sceUsbdOpenDeviceWithVidPid");
 void(*close_dev)(libusb_device_handle*)=dlsym(m,"sceUsbdClose");
 if(!init||!quit||!open_id||!close_dev)goto done;
 int rc=init();fprintf(f,"Sony USB init=%d\n",rc);if(rc)goto done;
 libusb_device_handle*h=open_id(0x1286,0x2059);
 fprintf(f,"internal radio service-handle open=%d; no interface claim or reads\n",h!=NULL);
 if(h){
#ifdef CLAIM_ONLY
  int(*claim)(libusb_device_handle*,int)=dlsym(m,"sceUsbdClaimInterface");
  int(*release)(libusb_device_handle*,int)=dlsym(m,"sceUsbdReleaseInterface");
  if(!claim||!release){close_dev(h);quit();goto done;}
  int claimed=claim(h,0);fprintf(f,"claim interface 0=%d; no reads or HCI commands\n",claimed);
  if(claimed){close_dev(h);quit();goto done;}
#endif
  long end=now_ms()+45000;fputs("45-second reconnect window active\n",f);while(now_ms()<end)usleep(10000);
#ifdef CLAIM_ONLY
  fprintf(f,"release interface 0=%d\n",release(h,0));
#endif
  close_dev(h);fputs("service handle closed\n",f);
 }
 quit();fputs("Sony USB client exited\n",f);
done:fputs("diagnostic complete\n",f);fclose(f);return 0;
}
