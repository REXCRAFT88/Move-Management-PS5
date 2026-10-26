#define MOVE_STUDIO_EXTERNAL_BT 1
#include "../src/usb_register.c"
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
void notify(const char*fmt,...){(void)fmt;}
int host_seed_usb_move(host_t*h,const unsigned char a[6]){(void)h;(void)a;return 1;}
int host_seed_usb_nav(host_t*h,const unsigned char a[6]){(void)h;(void)a;return 1;}
static unsigned char device_id[16]={4,1,2,3,4,5,6,0,0,0,11,12,13,14,15,16};
static int writes,fail_verify;
static int transfer(libusb_device_handle*h,uint8_t type,uint8_t request,uint16_t value,uint16_t index,unsigned char*p,uint16_t n,uint32_t timeout){
 (void)h;(void)index;(void)timeout;
 if(type==0x21){assert(request==9&&value==0x0305&&n==23&&p[0]==5);writes++;memcpy(device_id+10,p+1,6);return n;}
 assert(type==0xa1&&request==1&&value==0x0304&&n==16);memcpy(p,device_id,16);if(fail_verify){p[10]^=1;fail_verify=0;}return n;
}
int main(void){
 char dir[]="/tmp/move-registration-XXXXXX";assert(mkdtemp(dir));char path[200];snprintf(path,sizeof path,"%s/original",dir);
 unsigned char old[16],stored[16],target[6]={21,22,23,24,25,26};memcpy(old,device_id,16);control=transfer;
 assert(external_move_host((void*)1,old,target,path));assert(writes==1&&!memcmp(device_id+10,target,6));
 assert(exact(path,stored,16)&&!memcmp(stored,old,16));
 unsigned char current[16];memcpy(current,device_id,16);
 assert(external_move_host((void*)1,current,target,path)&&writes==1);
 unsigned char next[6]={31,32,33,34,35,36};fail_verify=1;
 assert(!external_move_host((void*)1,current,next,path));assert(writes==3&&!memcmp(device_id+10,target,6));
 assert(exact(path,stored,16)&&!memcmp(stored,old,16));
 current[1]^=1;int before=writes;assert(!external_move_host((void*)1,current,next,path)&&writes==before);
 unlink(path);rmdir(dir);
 puts("external USB registration preserves original backup, verifies writes, avoids repeat writes and rolls back failure");
}
