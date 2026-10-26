/* SPDX-License-Identifier: BSD-3-Clause
 * USB-assisted pairing for exactly one Sony Navigation controller.
 * Host address comes from our successful HCI query; original host is saved.
 * USB requests match USB Host Shield PS3USB::setBdaddr and BlueZ sixpair.
 */
#include <dlfcn.h>
#include <libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
static int read_exact(const char*p,unsigned char*b,size_t n){FILE*f=fopen(p,"rb");if(!f)return 0;int ok=fread(b,1,n,f)==n&&fgetc(f)==EOF;fclose(f);return ok;}
static int save_exact(const char*p,const unsigned char*b,size_t n){char tmp[192];snprintf(tmp,sizeof(tmp),"%s.new",p);FILE*f=fopen(tmp,"wb");if(!f)return 0;int ok=fwrite(b,1,n,f)==n&&fflush(f)==0&&fsync(fileno(f))==0;if(fclose(f))ok=0;if(ok&&rename(tmp,p)==0)return 1;unlink(tmp);return 0;}
static int valid_addr(const unsigned char*b){unsigned nz=0,ff=0;for(unsigned i=0;i<6;i++){nz|=b[i];ff|=b[i]^255;}return nz&&ff;}
int main(void){
 FILE*f=fopen("/data/move-studio/register-nav.log","w");if(!f)return 1;setvbuf(f,NULL,_IOLBF,0);alarm(10);
 unsigned char target[6];if(!read_exact("/data/anypad-move-nav/local-address.bin",target,6)||!valid_addr(target)){fputs("missing_verified_HCI_host_address\n",f);goto done;}
 void*l=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);if(!l)goto done;
 int(*init)(void)=dlsym(l,"sceUsbdInit");void(*finish)(void)=dlsym(l,"sceUsbdExit");
 int(*list_devices)(libusb_device***)=dlsym(l,"sceUsbdGetDeviceList");void(*free_list)(libusb_device**)=dlsym(l,"sceUsbdFreeDeviceList");
 int(*descriptor)(libusb_device*,struct libusb_device_descriptor*)=dlsym(l,"sceUsbdGetDeviceDescriptor");
 int(*open_device)(libusb_device*,libusb_device_handle**)=dlsym(l,"sceUsbdOpen");void(*close_device)(libusb_device_handle*)=dlsym(l,"sceUsbdClose");
 int(*control)(libusb_device_handle*,uint8_t,uint8_t,uint16_t,uint16_t,unsigned char*,uint16_t,uint32_t)=dlsym(l,"sceUsbdControlTransfer");
 if(!init||!finish||!list_devices||!free_list||!descriptor||!open_device||!close_device||!control)goto done;
 int rc=init();fprintf(f,"usb_init=%d\n",rc);if(rc)goto done;
 libusb_device**list=NULL;int n=list_devices(&list),matches=0;libusb_device*nav=NULL;libusb_device_handle*h=NULL;
 if(n<0||n>256||!list)goto cleanup;
 for(int i=0;i<n;i++){struct libusb_device_descriptor d={0};if(descriptor(list[i],&d)){matches=-999;break;}if(d.idVendor==0x054c&&d.idProduct==0x042f){matches++;nav=list[i];}}
 fprintf(f,"navigation_count=%d\n",matches);if(matches!=1)goto cleanup;
 rc=open_device(nav,&h);fprintf(f,"usb_open=%d\n",rc);if(rc||!h)goto cleanup;
 unsigned char identity[17]={0},old[8]={0},replacement[8]={1,0},verify[8]={0},record[14],prior[14];
 rc=control(h,0xa1,1,0x03f2,0,identity,17,200);fprintf(f,"identity_read=%d\n",rc);if(rc!=17||identity[0]!=0xf2||!valid_addr(identity+4))goto cleanup;
 for(unsigned i=0;i<6;i++)record[i]=identity[9-i];
 rc=control(h,0xa1,1,0x03f5,0,old,8,200);fprintf(f,"original_host_read=%d\n",rc);if(rc!=8)goto cleanup;memcpy(record+6,old,8);
 char backup[180],seed_path[180];
 snprintf(backup,sizeof backup,"/data/anypad-move-nav/nav-%02x%02x%02x%02x%02x%02x.original",record[0],record[1],record[2],record[3],record[4],record[5]);
 snprintf(seed_path,sizeof seed_path,"/data/anypad-move-nav/nav-%02x%02x%02x%02x%02x%02x.identity",record[0],record[1],record[2],record[3],record[4],record[5]);
#ifdef NAV_RESTORE
 if(!read_exact(backup,prior,14)||memcmp(prior,record,6)){fputs("restore_backup_missing_or_wrong_controller\n",f);goto cleanup;}
 rc=control(h,0x21,9,0x03f5,0,prior+6,8,200);int restore_read=control(h,0xa1,1,0x03f5,0,verify,8,200);
 if(rc==8&&restore_read==8&&!memcmp(verify+2,prior+8,6)){unlink("/data/anypad-move-nav/nav-usb-address.bin");fputs("original_host_restored_and_verified\n",f);}else fputs("restore_failed\n",f);
 goto cleanup;
#endif
 if(access(backup,F_OK)==0){if(!read_exact(backup,prior,14)||memcmp(prior,record,6)){fputs("backup_identity_mismatch; no writes\n",f);goto cleanup;}}
 else if(!save_exact(backup,record,sizeof(record))){fputs("backup_failed; no writes\n",f);goto cleanup;}
 for(unsigned i=0;i<6;i++)replacement[i+2]=target[5-i];
 if(memcmp(old+2,replacement+2,6)){
  rc=control(h,0x21,9,0x03f5,0,replacement,8,200);fprintf(f,"set_host_address=%d\n",rc);
  int got=control(h,0xa1,1,0x03f5,0,verify,8,200);
  if(rc!=8||got!=8||memcmp(verify+2,replacement+2,6)){
   fprintf(f,"verification_failed read=%d; rollback_write=%d\n",got,control(h,0x21,9,0x03f5,0,old,8,200));goto cleanup;
  }
 }else fputs("host_address_already_matches\n",f);
 if(!save_exact(seed_path,record,6)){fprintf(f,"seed_save_failed; rollback_write=%d\n",control(h,0x21,9,0x03f5,0,old,8,200));goto cleanup;}
 fputs("paired_host_verified; exact Nav identity saved; original host backed up\n",f);
 cleanup:if(h)close_device(h);if(list)free_list(list);finish();
 done:alarm(0);fputs("complete\n",f);fclose(f);return 0;
}