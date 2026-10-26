#include <assert.h>
#include "../src/usb_register.c"
static int notification_count,seed_count;
void notify(const char*fmt,...){notification_count++;if(strstr(fmt,"is ready"))assert(seed_count>0);}
int host_seed_usb_move(host_t*h,const unsigned char a[6]){(void)h;(void)a;seed_count++;return 1;}
int host_seed_usb_nav(host_t*h,const unsigned char a[6]){(void)h;(void)a;seed_count++;return 1;}
static unsigned char device_host[8];static int writes,fail_verify;
static int mock_control(libusb_device_handle*h,uint8_t type,uint8_t request,uint16_t value,uint16_t index,unsigned char*b,uint16_t len,uint32_t timeout){
 (void)h;(void)request;(void)value;(void)index;(void)timeout;assert(len==8);
 if(type==0x21){writes++;memcpy(device_host,b,8);return 8;}
 memcpy(b,device_host,8);if(fail_verify)b[2]^=1;return 8;
}
int main(void){
 const char*backup="/tmp/studio-usb-pairing-test.original";unlink(backup);control=mock_control;
 unsigned char old[8]={1,0,1,2,3,4,5,6},target[6]={6,5,4,3,2,1},saved[8];memcpy(device_host,old,8);
 assert(host_change((void*)1,0xf5,old,8,2,old+2,backup));assert(!writes&&access(backup,F_OK));
 assert(host_change((void*)1,0xf5,old,8,2,target,backup));assert(writes==1&&!memcmp(device_host+2,target,6));assert(exact(backup,saved,8)&&!memcmp(saved,old,8));
 fail_verify=1;memcpy(device_host,old,8);writes=0;assert(!host_change((void*)1,0xf5,old,8,2,target,backup));assert(writes==2&&!memcmp(device_host,old,8));
 fail_verify=0;writes=0;assert(!host_change((void*)1,0xf5,old,8,2,target,"/nonexistent/studio-test/backup"));assert(!writes);
 assert(exact(backup,saved,8)&&!memcmp(saved,old,8));unlink(backup);
 unsigned char addr[6]={1,2,3,4,5,6};scan_generation=1;int remembered=remember(addr);queue_notice(remembered,0x03d5,0);queue_notice(remembered,0x03d5,0);queue_notice(remembered,0x03d5,1);usb_register_poll((host_t*)1);assert(notification_count==2&&seed_count==1);
 scan_generation=2;remembered=remember(addr);queue_notice(remembered,0x03d5,0);queue_notice(remembered,0x03d5,1);usb_register_poll((host_t*)1);assert(notification_count==2);
 scan_generation=4;remembered=remember(addr);queue_notice(remembered,0x03d5,0);queue_notice(remembered,0x03d5,1);usb_register_poll((host_t*)1);assert(notification_count==4&&seed_count==2);
 puts("USB pairing backup, readback, rollback and no-write-on-failure checks passed");return 0;
}
