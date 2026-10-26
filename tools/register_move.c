/* Read-only USB feature requests; stores identity-specific calibration locally.
 * ZCM1 block layout documented by thp/psmoveapi src/psmove.c. No pairing writes. */
#include <dlfcn.h>
#include <libusb.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
int main(void){
 mkdir("/data/move-studio",0755);FILE*log=fopen("/data/move-studio/register.log","w");if(!log)return 1;setvbuf(log,NULL,_IOLBF,0);alarm(15);int ok=0;
 void*lib=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);if(!lib)goto done;
 int(*init)(void)=dlsym(lib,"sceUsbdInit");void(*finish)(void)=dlsym(lib,"sceUsbdExit");libusb_device_handle*(*open)(uint16_t,uint16_t)=dlsym(lib,"sceUsbdOpenDeviceWithVidPid");void(*close)(libusb_device_handle*)=dlsym(lib,"sceUsbdClose");int(*control)(libusb_device_handle*,uint8_t,uint8_t,uint16_t,uint16_t,unsigned char*,uint16_t,uint32_t)=dlsym(lib,"sceUsbdControlTransfer");
 if(!init||!finish||!open||!close||!control||init())goto done;
 libusb_device_handle*h=open(0x054c,0x03d5);if(!h){fputs("No USB ZCM1 Move found\n",log);finish();goto done;}
 unsigned char id[16]={0},host[6],blob[143]={0};unsigned blocks=0;
 FILE*f=fopen("/data/anypad-move-nav/local-address.bin","rb");int hostok=f&&fread(host,1,6,f)==6;if(f)fclose(f);
 int rc=control(h,0xa1,1,0x0304,0,id,16,500);fprintf(log,"identity_read=%d\n",rc);
 if(rc!=16||id[0]!=4||!hostok||memcmp(host,id+10,6)){fputs("Host mismatch or invalid identity; nothing written\n",log);goto usb_done;}
 for(int tries=0;tries<9&&blocks!=7;tries++){unsigned char b[49]={0};rc=control(h,0xa1,1,0x0310,0,b,49,500);if(rc!=49||b[0]!=0x10)continue;
  int part=b[1]==0?0:b[1]==1?1:b[1]==0x82?2:-1;if(part<0)continue;int offset=part==0?0:part==1?49:96;int skip=part?2:0;memcpy(blob+offset,b+skip,49-skip);blocks|=1u<<part;
 }
 fprintf(log,"calibration_blocks=%u\n",blocks);if(blocks!=7)goto usb_done;
 char path[200],tmp[210];snprintf(path,sizeof path,"/data/anypad-move-nav/move-%02x%02x%02x%02x%02x%02x.calibration",id[1],id[2],id[3],id[4],id[5],id[6]);snprintf(tmp,sizeof tmp,"%s.tmp",path);
 f=fopen(tmp,"wb");if(f){ok=fwrite(id+1,1,6,f)==6&&fwrite(blob,1,143,f)==143;if(fflush(f)||fsync(fileno(f)))ok=0;if(fclose(f))ok=0;if(ok&&rename(tmp,path))ok=0;if(!ok)unlink(tmp);}
 fprintf(log,"identity=%02x:%02x:%02x:%02x:%02x:%02x saved=%d\n",id[6],id[5],id[4],id[3],id[2],id[1],ok);
usb_done:close(h);finish();
done:alarm(0);fprintf(log,"complete=%d; no controller pairing writes\n",ok);fclose(log);return ok?0:1;
}
