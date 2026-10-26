/* Finite ASUS-only HCI information probe. No resets, pairing or internal-radio opens. */
#include <libusb.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
int main(void){
 alarm(12);FILE*f=fopen("/data/move-studio/external-bt-probe.log","w");if(!f)return 1;setvbuf(f,NULL,_IOLBF,0);
 void*m=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);if(!m)goto done;
 int(*init)(void)=dlsym(m,"sceUsbdInit");void(*quit)(void)=dlsym(m,"sceUsbdExit");
 libusb_device_handle*(*open_id)(uint16_t,uint16_t)=dlsym(m,"sceUsbdOpenDeviceWithVidPid");
 void(*close_dev)(libusb_device_handle*)=dlsym(m,"sceUsbdClose");
 int(*claim)(libusb_device_handle*,int)=dlsym(m,"sceUsbdClaimInterface");
 int(*release)(libusb_device_handle*,int)=dlsym(m,"sceUsbdReleaseInterface");
 libusb_device*(*device)(libusb_device_handle*)=dlsym(m,"sceUsbdGetDevice");
 int(*config)(libusb_device*,uint8_t,struct libusb_config_descriptor**)=dlsym(m,"sceUsbdGetConfigDescriptor");
 void(*free_cfg)(struct libusb_config_descriptor*)=dlsym(m,"sceUsbdFreeConfigDescriptor");
 int(*ctrl)(libusb_device_handle*,uint8_t,uint8_t,uint16_t,uint16_t,unsigned char*,uint16_t,uint32_t)=dlsym(m,"sceUsbdControlTransfer");
 int(*intr)(libusb_device_handle*,unsigned char,unsigned char*,int,int*,unsigned int)=dlsym(m,"sceUsbdInterruptTransfer");
 if(!init||!quit||!open_id||!close_dev||!claim||!release||!device||!config||!free_cfg||!ctrl||!intr){fputs("required export missing\n",f);goto done;}
 int rc=init();fprintf(f,"usb_init=%d\n",rc);if(rc)goto done;
 libusb_device_handle*h=open_id(0x0b05,0x190e);fprintf(f,"ASUS 0b05:190e opened=%d\n",h!=NULL);if(!h){quit();goto done;}
 struct libusb_config_descriptor*c=NULL;unsigned char ep=0;int iface=-1;
 if(!config(device(h),0,&c)&&c){for(int i=0;i<c->bNumInterfaces;i++){for(int j=0;j<c->interface[i].num_altsetting;j++){
 const struct libusb_interface_descriptor*a=&c->interface[i].altsetting[j];if(a->bAlternateSetting||a->bInterfaceClass!=0xe0||a->bInterfaceSubClass!=1||a->bInterfaceProtocol!=1)continue;
 for(int k=0;k<a->bNumEndpoints;k++)if((a->endpoint[k].bmAttributes&3)==3&&(a->endpoint[k].bEndpointAddress&0x80)){ep=a->endpoint[k].bEndpointAddress;iface=a->bInterfaceNumber;}
 }}
 free_cfg(c); }
 fprintf(f,"interface=%d event_endpoint=%02x\n",iface,ep);
 if(iface>=0&&ep){rc=claim(h,iface);fprintf(f,"claim=%d\n",rc);if(!rc){
#ifdef EXTERNAL_RESET
 unsigned char cmd[]={0x03,0x0c,0}; /* Explicit external-only recovery experiment. */
#else
 unsigned char cmd[]={0x01,0x10,0}; /* Read Local Version Information, not reset. */
#endif
 rc=ctrl(h,0x20,0,0,(uint16_t)iface,cmd,sizeof cmd,1000);fprintf(f,"opcode=%02x%02x send=%d\n",cmd[1],cmd[0],rc);
 if(rc==3)for(int attempt=0;attempt<3;attempt++){unsigned char b[260];int n=0;rc=intr(h,ep,b,sizeof b,&n,1000);fprintf(f,"event rc=%d length=%d bytes=",rc,n);if(n>0&&n<=(int)sizeof b)for(int k=0;k<n;k++)fprintf(f,"%02x",b[k]);fputc('\n',f);if(!rc&&n>=5&&b[0]==0x0e&&b[3]==cmd[0]&&b[4]==cmd[1])break;}
 fprintf(f,"release=%d\n",release(h,iface));}}
 close_dev(h);quit();done:
#ifdef EXTERNAL_RESET
 fputs("complete; external ASUS reset experiment; no pairing writes\n",f);
#else
 fputs("complete; no reset or pairing commands\n",f);
#endif
 fclose(f);alarm(0);return 0;
}
