/* SPDX-License-Identifier: GPL-3.0-or-later
 * Explicit one-controller USB host migration; never opens a Bluetooth radio.
 * ZCM1 report layout referenced from thp/psmoveapi (BSD-2-Clause): report 5,
 * 23 bytes, six host-address bytes at 1..6; report 4 reads host at 10..15.
 * Build RESTORE_ORIGINAL=1 for the same controller's backed-up original host.
 */
#include <libusb.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifndef RESTORE_ORIGINAL
#define RESTORE_ORIGINAL 0
#endif
#ifndef DRY_RUN
#define DRY_RUN 0
#endif
static int read_exact(const char*path,unsigned char*p,size_t n){FILE*f=fopen(path,"rb");if(!f)return 0;int ok=fread(p,1,n,f)==n&&fgetc(f)==EOF;fclose(f);return ok;}
static int backup_once(const char*path,const unsigned char id[16]){
 unsigned char old[16];if(access(path,F_OK)==0)return read_exact(path,old,16)&&!memcmp(old,id,7);
 int fd=open(path,O_CREAT|O_EXCL|O_WRONLY,0600);if(fd<0)return 0;
 int ok=write(fd,id,16)==16;if(fsync(fd))ok=0;if(close(fd))ok=0;
 if(!ok)unlink(path);return ok;
}
static int valid_address(const unsigned char*p){unsigned any=0,all=255;for(int i=0;i<6;i++){any|=p[i];all&=p[i];}return any&&all!=255;}
int main(void){
 FILE*f=fopen("/data/move-studio/usb-registration.log","a");if(!f)return 1;setvbuf(f,NULL,_IOLBF,0);
 int result=1;fputs("Explicit external Move pairing started; exactly one USB controller required\n",f);
 void*m=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);if(!m)goto done;
 int(*init)(void)=dlsym(m,"sceUsbdInit");void(*quit)(void)=dlsym(m,"sceUsbdExit");
 ssize_t(*list_devices)(libusb_device***)=dlsym(m,"sceUsbdGetDeviceList");
 void(*free_list)(libusb_device**,int)=dlsym(m,"sceUsbdFreeDeviceList");
 int(*descriptor)(libusb_device*,struct libusb_device_descriptor*)=dlsym(m,"sceUsbdGetDeviceDescriptor");
 int(*open_device)(libusb_device*,libusb_device_handle**)=dlsym(m,"sceUsbdOpen");
 void(*close_device)(libusb_device_handle*)=dlsym(m,"sceUsbdClose");
 int(*control)(libusb_device_handle*,uint8_t,uint8_t,uint16_t,uint16_t,unsigned char*,uint16_t,uint32_t)=dlsym(m,"sceUsbdControlTransfer");
 if(!init||!quit||!list_devices||!free_list||!descriptor||!open_device||!close_device||!control)goto done;
 if(init())goto done;
 libusb_device**devices=NULL;ssize_t n=list_devices(&devices);int count=0;libusb_device*selected=NULL;
 if(n>0&&n<=256)for(ssize_t i=0;i<n;i++){struct libusb_device_descriptor d;if(!descriptor(devices[i],&d)&&d.idVendor==0x054c&&(d.idProduct==0x03d5||d.idProduct==0x042f)){count++;if(d.idProduct==0x03d5)selected=devices[i];}}
 fprintf(f,"Move/Nav devices attached by USB=%d\n",count);
 libusb_device_handle*h=NULL;if(count!=1||!selected||open_device(selected,&h)||!h){fputs("Aborted: connect exactly one ZCM1 Move and no USB Nav\n",f);goto release_list;}
 unsigned char id[16]={0},target[6]={0},original[16],verify[16]={0};char backup[200];
 if(control(h,0xa1,1,0x0304,0,id,16,500)!=16||id[0]!=4||!valid_address(id+1)){fputs("Identity read failed; no write\n",f);goto close_handle;}
 snprintf(backup,sizeof backup,"/data/move-studio/move-%02x%02x%02x%02x%02x%02x.original-host",id[1],id[2],id[3],id[4],id[5],id[6]);
 if(RESTORE_ORIGINAL){
  if(!read_exact(backup,original,16)||memcmp(original,id,7)){fputs("Restore backup missing or identity mismatch; no write\n",f);goto close_handle;}
  memcpy(target,original+10,6);
 }else{
  if(!read_exact("/data/move-studio/asus-address.bin",target,6)||!valid_address(target)){fputs("External host address unavailable; no write\n",f);goto close_handle;}
  if(!backup_once(backup,id)){fputs("Original host backup could not be verified; no write\n",f);goto close_handle;}
  fputs("Original host backup preserved and verified\n",f);
 }
 if(!valid_address(target)){fputs("Target address invalid; no write\n",f);goto close_handle;}
 if(DRY_RUN){fputs("Preflight passed: one Move, identity and backup valid, external host available; no write\n",f);result=0;goto close_handle;}
 unsigned char report[23]={5};memcpy(report+1,target,6);
 int rc=control(h,0x21,9,0x0305,0,report,23,500);
 fprintf(f,"Set host feature result=%d\n",rc);
 usleep(100000);
 int ok=rc==23&&control(h,0xa1,1,0x0304,0,verify,16,500)==16&&!memcmp(verify,id,7)&&!memcmp(verify+10,target,6);
 if(!ok){
  memset(report,0,23);report[0]=5;memcpy(report+1,id+10,6);
  int wr=control(h,0x21,9,0x0305,0,report,23,500);
  int restored=wr==23&&control(h,0xa1,1,0x0304,0,verify,16,500)==16&&!memcmp(verify,id,16);
  fprintf(f,"Target verification failed; rollback verified=%d. Keep USB connected.\n",restored);
 }else{fputs(RESTORE_ORIGINAL?"Original host restored and verified\n":"ASUS host written and verified. Unplug this Move and press PS.\n",f);result=0;}
close_handle:close_device(h);
release_list:if(n>=0&&devices)free_list(devices,1);quit();
done:fprintf(f,"Explicit Move pairing complete result=%d\n",result);fclose(f);return result;
}
