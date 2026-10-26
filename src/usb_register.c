/* USB-assisted Move/Nav registration. No kernel detach or device resets.
 * Pairing host changes are backed up per identity and verified after writing. */
#include "usb_register.h"
#include "ps5_vpad.h"
#include <dlfcn.h>
#include <libusb.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static pthread_t thread;static atomic_int stopping;static int started;
static int (*devices)(libusb_device***),(*descriptor)(libusb_device*,struct libusb_device_descriptor*),(*open_device)(libusb_device*,libusb_device_handle**);
static void (*free_devices)(libusb_device**,int),(*close_device)(libusb_device_handle*);
static int (*control)(libusb_device_handle*,uint8_t,uint8_t,uint16_t,uint16_t,unsigned char*,uint16_t,uint32_t);
typedef struct{unsigned char addr[6];unsigned pid,type;} usb_notice;
static usb_notice notices[32];static unsigned notice_in,notice_out;
static pthread_mutex_t notice_mutex=PTHREAD_MUTEX_INITIALIZER;
typedef struct{unsigned char addr[6];unsigned generation,flags;} seen_usb;
static seen_usb seen[16];static unsigned scan_generation;
static int remember(const unsigned char addr[6]){
 int free_slot=-1;for(int i=0;i<16;i++){if(seen[i].generation&&!memcmp(seen[i].addr,addr,6)){if(seen[i].generation+1<scan_generation)seen[i].flags=0;seen[i].generation=scan_generation;return i;}if(!seen[i].generation)free_slot=i;}
 if(free_slot<0)return -1;memcpy(seen[free_slot].addr,addr,6);seen[free_slot].generation=scan_generation;return free_slot;
}
static void queue_notice(int index,unsigned pid,unsigned type){
 if(index<0||(seen[index].flags&(1u<<type)))return;
 pthread_mutex_lock(&notice_mutex);if(notice_in-notice_out<32){usb_notice*n=&notices[notice_in++%32];memcpy(n->addr,seen[index].addr,6);n->pid=pid;n->type=type;seen[index].flags|=1u<<type;}pthread_mutex_unlock(&notice_mutex);
}
void usb_register_poll(host_t*host){
 if(!host)return;
 for(;;){usb_notice n;pthread_mutex_lock(&notice_mutex);if(notice_in==notice_out){pthread_mutex_unlock(&notice_mutex);break;}n=notices[notice_out++%32];pthread_mutex_unlock(&notice_mutex);
 const char*name=n.pid==0x03d5?"Move":"Navigation";
 if(n.type==0)notify("Move Management: %s %02X:%02X connected by USB. Setting up...",name,n.addr[1],n.addr[0]);
 else if(n.type==1){int ready=n.pid==0x03d5?host_seed_usb_move(host,n.addr):host_seed_usb_nav(host,n.addr);if(ready)notify("Move Management: %s %02X:%02X is ready. Unplug USB, then press PS for Bluetooth.",name,n.addr[1],n.addr[0]);else notify("Move Management: controller registration is full. Keep USB connected; see the dashboard.");}
 else notify("Move Management: %s USB setup could not finish. Keep USB connected and check the dashboard.",name);
 }
}
static int exact(const char*p,unsigned char*b,size_t n){FILE*f=fopen(p,"rb");if(!f)return 0;int ok=fread(b,1,n,f)==n&&fgetc(f)==EOF;fclose(f);return ok;}
static int save(const char*p,const unsigned char*b,size_t n){char tmp[240];if(snprintf(tmp,sizeof tmp,"%s.tmp",p)>=(int)sizeof tmp)return 0;FILE*f=fopen(tmp,"wb");if(!f)return 0;int ok=fwrite(b,1,n,f)==n;if(fflush(f)||fsync(fileno(f)))ok=0;if(fclose(f))ok=0;if(ok&&rename(tmp,p))ok=0;if(!ok)unlink(tmp);return ok;}
static int host_change(libusb_device_handle*h,unsigned report,unsigned char*old,unsigned size,unsigned offset,const unsigned char*target,const char*backup){
 if(!memcmp(old+offset,target,6))return 1;
 unsigned char prior[24],next[24],verify[24];if(size>sizeof prior)return 0;
 if(backup){if(access(backup,F_OK)==0){if(!exact(backup,prior,size))return 0;}else if(!save(backup,old,size))return 0;}
 memcpy(next,old,size);if(report==0xf5){next[0]=1;next[1]=0;}memcpy(next+offset,target,6);
 if(control(h,0x21,9,(uint16_t)(0x0300|report),0,next,(uint16_t)size,300)!=(int)size||control(h,0xa1,1,(uint16_t)(0x0300|report),0,verify,(uint16_t)size,300)!=(int)size||memcmp(verify+offset,target,6)){
  (void)control(h,0x21,9,(uint16_t)(0x0300|report),0,old,(uint16_t)size,300);return 0;
 }return 1;
}
#ifdef MOVE_STUDIO_EXTERNAL_BT
static int original_backup(const char*path,const unsigned char*record,size_t size,size_t identity_size){
 unsigned char prior[24];if(size>sizeof prior)return 0;
 if(access(path,F_OK)==0)return exact(path,prior,size)&&!memcmp(prior,record,identity_size);
 return save(path,record,size)&&exact(path,prior,size)&&!memcmp(prior,record,size);
}
static int external_move_host(libusb_device_handle*h,const unsigned char id[16],const unsigned char host[6],const char*backup){
 if(!original_backup(backup,id,16,7))return 0;
 if(!memcmp(id+10,host,6))return 1;
 unsigned char report[23]={5},verify[16];memcpy(report+1,host,6);
 if(control(h,0x21,9,0x0305,0,report,23,300)==23&&control(h,0xa1,1,0x0304,0,verify,16,300)==16&&!memcmp(verify,id,7)&&!memcmp(verify+10,host,6))return 1;
 memset(report,0,23);report[0]=5;memcpy(report+1,id+10,6);
 (void)control(h,0x21,9,0x0305,0,report,23,300);return 0;
}
#endif
static void register_one(libusb_device_handle*h,unsigned pid,const unsigned char host[6],FILE*log){
 unsigned char addr[6],id[17]={0},cal[149]={0};char path[200],backup[200];
 if(pid==0x03d5){
  if(control(h,0xa1,1,0x0304,0,id,16,300)!=16||id[0]!=4)return;memcpy(addr,id+1,6);int seen_index=remember(addr);queue_notice(seen_index,pid,0);
#ifdef MOVE_STUDIO_EXTERNAL_BT
  snprintf(backup,sizeof backup,"/data/move-studio/move-%02x%02x%02x%02x%02x%02x.original-host",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
  if(!external_move_host(h,id,host,backup)){queue_notice(seen_index,pid,2);fputs("External Move pairing/backup verification failed\n",log);return;}
#else
  /* Native build never repoints a Move to a mismatching host. */
  if(memcmp(host,id+10,6)){queue_notice(seen_index,pid,2);fprintf(log,"Move host mismatch: pair with this PS5 by USB first\n");return;}
#endif
  snprintf(path,sizeof path,"/data/anypad-move-nav/move-%02x%02x%02x%02x%02x%02x.calibration",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
  if(exact(path,cal,149)&&!memcmp(cal,addr,6)){queue_notice(seen_index,pid,1);return;}
  memcpy(cal,addr,6);unsigned blocks=0;
  for(int tries=0;tries<9&&blocks!=7&&!atomic_load(&stopping);tries++){unsigned char b[49]={0};if(control(h,0xa1,1,0x0310,0,b,49,300)!=49||b[0]!=0x10)continue;int part=b[1]==0?0:b[1]==1?1:b[1]==0x82?2:-1;if(part<0)continue;int offset=part==0?0:part==1?49:96,skip=part?2:0;memcpy(cal+6+offset,b+skip,49-skip);blocks|=1u<<part;}
  if(blocks==7&&save(path,cal,149)){queue_notice(seen_index,pid,1);fprintf(log,"Move registered %02x:%02x:%02x:%02x:%02x:%02x; unplug and press PS\n",addr[5],addr[4],addr[3],addr[2],addr[1],addr[0]);}
  else {queue_notice(seen_index,pid,2);fprintf(log,"Move calibration incomplete blocks=%u; keep USB connected\n",blocks);}
 }else{
  if(control(h,0xa1,1,0x03f2,0,id,17,300)!=17||id[0]!=0xf2)return;for(int i=0;i<6;i++)addr[i]=id[9-i];int seen_index=remember(addr);queue_notice(seen_index,pid,0);
  snprintf(path,sizeof path,"/data/anypad-move-nav/nav-%02x%02x%02x%02x%02x%02x.identity",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
  unsigned char old[8],target[6],known[6];for(int i=0;i<6;i++)target[i]=host[5-i];
  if(control(h,0xa1,1,0x03f5,0,old,8,300)!=8)return;
  if(exact(path,known,6)&&!memcmp(known,addr,6)&&!memcmp(old+2,target,6)){queue_notice(seen_index,pid,1);return;}
#ifdef MOVE_STUDIO_EXTERNAL_BT
  snprintf(backup,sizeof backup,"/data/move-studio/nav-%02x%02x%02x%02x%02x%02x.original-host",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
  unsigned char record[14];memcpy(record,addr,6);memcpy(record+6,old,8);
  int paired=original_backup(backup,record,14,6)&&host_change(h,0xf5,old,8,2,target,NULL);
#else
  snprintf(backup,sizeof backup,"/data/anypad-move-nav/nav-%02x%02x%02x%02x%02x%02x.original",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5]);
  int paired=host_change(h,0xf5,old,8,2,target,backup);
#endif
  if(paired&&save(path,addr,6)){queue_notice(seen_index,pid,1);fprintf(log,"Nav registered; host verified, unplug and press PS\n");}else queue_notice(seen_index,pid,2);
 }
}
static void*worker(void*unused){
 (void)unused;void*lib=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);if(!lib)return NULL;
 int(*init)(void)=dlsym(lib,"sceUsbdInit");void(*finish)(void)=dlsym(lib,"sceUsbdExit");
 devices=dlsym(lib,"sceUsbdGetDeviceList");descriptor=dlsym(lib,"sceUsbdGetDeviceDescriptor");open_device=dlsym(lib,"sceUsbdOpen");free_devices=dlsym(lib,"sceUsbdFreeDeviceList");close_device=dlsym(lib,"sceUsbdClose");control=dlsym(lib,"sceUsbdControlTransfer");
 if(!init||!finish||!devices||!descriptor||!open_device||!free_devices||!close_device||!control)return NULL;
#ifndef MOVE_STUDIO_EXTERNAL_BT
 if(init())return NULL;
#define FINISH_USB() finish()
#define USB_HOST_FILE "/data/anypad-move-nav/local-address.bin"
#else
 /* The external transport owns USB init/exit for this process. The worker
  * must be joined before that transport is closed. */
#define FINISH_USB() ((void)0)
#define USB_HOST_FILE "/data/move-studio/asus-address.bin"
#endif
 FILE*log=fopen("/data/move-studio/usb-registration.log","a");if(!log){FINISH_USB();return NULL;}setvbuf(log,NULL,_IOLBF,0);fputs("Automatic USB setup active\n",log);
 while(!atomic_load(&stopping)){
  scan_generation++;unsigned char host[6];libusb_device**list=NULL;
  unsigned any=0;if(exact(USB_HOST_FILE,host,6))for(unsigned i=0;i<6;i++)any|=host[i];
  if(any){
   int n=devices(&list);static int prior_count=-999;if(n!=prior_count){fprintf(log,"USB device count=%d\n",n);prior_count=n;}if(n>0&&n<=256&&list)for(int i=0;i<n&&!atomic_load(&stopping);i++){struct libusb_device_descriptor d={0};if(descriptor(list[i],&d)||d.idVendor!=0x054c||(d.idProduct!=0x03d5&&d.idProduct!=0x042f))continue;libusb_device_handle*h=NULL;if(!open_device(list[i],&h)&&h){register_one(h,d.idProduct,host,log);close_device(h);}}
   if(list)free_devices(list,1);
  }
  for(int i=0;i<20&&!atomic_load(&stopping);i++)usleep(100000);
 }fputs("Automatic USB setup stopped\n",log);fclose(log);FINISH_USB();return NULL;
}
int usb_register_start(void){if(started)return 1;memset(seen,0,sizeof seen);scan_generation=0;notice_in=notice_out=0;atomic_store(&stopping,0);started=pthread_create(&thread,NULL,worker,NULL)==0;return started;}
void usb_register_stop(void){if(!started)return;atomic_store(&stopping,1);pthread_join(thread,NULL);started=0;}
