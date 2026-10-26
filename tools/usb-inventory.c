/* Descriptor-only inventory through Sony USB service; never opens devices. */
#include <libusb.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
int main(void) {
 alarm(10);
 FILE *f=fopen("/data/move-studio/usb-inventory.log","w"); if(!f)return 1;
 void *m=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);
 if(!m){fputs("USB library unavailable\n",f);fclose(f);return 1;}
 int(*init)(void)=dlsym(m,"sceUsbdInit");
 void(*quit)(void)=dlsym(m,"sceUsbdExit");
 ssize_t(*list)(libusb_device***)=dlsym(m,"sceUsbdGetDeviceList");
 void(*free_list)(libusb_device**,int)=dlsym(m,"sceUsbdFreeDeviceList");
 int(*desc)(libusb_device*,struct libusb_device_descriptor*)=dlsym(m,"sceUsbdGetDeviceDescriptor");
 int(*config)(libusb_device*,uint8_t,struct libusb_config_descriptor**)=dlsym(m,"sceUsbdGetConfigDescriptor");
 void(*free_config)(struct libusb_config_descriptor*)=dlsym(m,"sceUsbdFreeConfigDescriptor");
 uint8_t(*bus)(libusb_device*)=dlsym(m,"sceUsbdGetBusNumber");
 uint8_t(*addr)(libusb_device*)=dlsym(m,"sceUsbdGetDeviceAddress");
 if(!init||!quit||!list||!free_list||!desc||!config||!free_config)goto done;
 if(init())goto done;
 libusb_device **devices=NULL;ssize_t n=list(&devices);fprintf(f,"devices=%ld; descriptors only; no device opens\n",(long)n);
 for(ssize_t i=0;i<n;i++){
  struct libusb_device_descriptor d;if(desc(devices[i],&d))continue;
  fprintf(f,"bus=%u address=%u vid=%04x pid=%04x class=%02x/%02x/%02x\n",bus?bus(devices[i]):0,addr?addr(devices[i]):0,d.idVendor,d.idProduct,d.bDeviceClass,d.bDeviceSubClass,d.bDeviceProtocol);
  struct libusb_config_descriptor *c=NULL;if(config(devices[i],0,&c)||!c)continue;
  for(int j=0;j<c->bNumInterfaces;j++){ for(int k=0;k<c->interface[j].num_altsetting;k++){
   const struct libusb_interface_descriptor *a=&c->interface[j].altsetting[k];
   fprintf(f," interface=%u alt=%u class=%02x/%02x/%02x endpoints=%u\n",a->bInterfaceNumber,a->bAlternateSetting,a->bInterfaceClass,a->bInterfaceSubClass,a->bInterfaceProtocol,a->bNumEndpoints);
  } }
  free_config(c);
 }if(n>=0)free_list(devices,1);quit();
 done:fputs("complete\n",f);fclose(f);alarm(0);return 0;
}
