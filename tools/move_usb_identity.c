/* Read-only Sony Navigation USB identity/host-address query. No pairing writes. */
#include <dlfcn.h>
#include <libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
int main(void){FILE*f=fopen("/data/anypad-move-usb-identity.log","w");if(!f)return 1;setvbuf(f,NULL,_IOLBF,0);alarm(8);
 void*l=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);if(!l)goto done;
 int(*init)(void)=dlsym(l,"sceUsbdInit");void(*exit_usb)(void)=dlsym(l,"sceUsbdExit");
 libusb_device_handle*(*open)(uint16_t,uint16_t)=dlsym(l,"sceUsbdOpenDeviceWithVidPid");void(*close)(libusb_device_handle*)=dlsym(l,"sceUsbdClose");
 int(*control)(libusb_device_handle*,uint8_t,uint8_t,uint16_t,uint16_t,unsigned char*,uint16_t,uint32_t)=dlsym(l,"sceUsbdControlTransfer");
 if(!init||!exit_usb||!open||!close||!control)goto done;int rc=init();fprintf(f,"init=%d\n",rc);if(rc)goto done;
 libusb_device_handle*h=open(0x054c,0x03d5);fprintf(f,"move_open=%d\n",h!=NULL);if(h){
 unsigned char b[64];const unsigned ids[]={0x04};const unsigned sizes[]={16};
 for(unsigned i=0;i<1;i++){for(unsigned j=0;j<64;j++)b[j]=0;rc=control(h,0xa1,1,0x0300|ids[i],0,b,sizes[i],200);fprintf(f,"feature=%02x rc=%d bytes=",ids[i],rc);if(rc>0&&rc<=64)for(int j=0;j<rc;j++)fprintf(f,"%02x",b[j]);fputc('\n',f);
 if(rc==16&&b[0]==4){FILE*out=fopen("/data/anypad-move-nav/move-usb-identity.bin","wb");if(out){fwrite(b,1,16,out);fclose(out);}}
 }close(h);}
 exit_usb();done:alarm(0);fputs("complete; read only\n",f);fclose(f);return 0;}