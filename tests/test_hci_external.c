#include "hci_external.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int closes,releases,quits,wrong_id,claim_fail,absent,event_part,acl_part,removed;
static struct libusb_endpoint_descriptor eps[]={{0x81,3},{0x82,2},{0x02,2}};
static struct libusb_interface_descriptor alt={0,0xe0,1,1,0,3,eps};
static struct libusb_interface iface={1,&alt};
static struct libusb_config_descriptor cfg={1,&iface};
static int init(void){return 0;}static void quit(void){quits++;}
static libusb_device_handle*open_id(uint16_t v,uint16_t p){return absent||v!=0x0b05||p!=0x190e?NULL:(libusb_device_handle*)1;}
static void close_dev(libusb_device_handle*h){assert(h);closes++;}
static libusb_device*device(libusb_device_handle*h){assert(h);return (libusb_device*)2;}
static int descriptor(libusb_device*d,struct libusb_device_descriptor*v){assert(d);memset(v,0,sizeof *v);v->idVendor=wrong_id?0x1286:0x0b05;v->idProduct=0x190e;return 0;}
static int config(libusb_device*d,uint8_t index,struct libusb_config_descriptor**v){assert(d&&!index);*v=&cfg;return 0;}
static void free_config(struct libusb_config_descriptor*v){assert(v==&cfg);}
static int claim(libusb_device_handle*h,int i){assert(h&&!i);return claim_fail?-1:0;}
static int release(libusb_device_handle*h,int i){assert(h&&!i);releases++;return 0;}
static int control(libusb_device_handle*h,uint8_t t,uint8_t req,uint16_t val,uint16_t i,unsigned char*p,uint16_t n,uint32_t timeout){assert(h&&t==0x20&&!req&&!val&&!i&&timeout==100);assert(n==3&&p[0]==1&&p[1]==0x10&&!p[2]);return n;}
static int interrupt(libusb_device_handle*h,unsigned char ep,unsigned char*p,int size,int*n,unsigned timeout){
 assert(h&&ep==0x81&&size==64&&timeout==1);*n=0;if(removed)return -4;
 static const unsigned char parts[][3]={{0x0e,3,1},{1,0x10,0}};
 if(event_part<2){*n=event_part?2:3;memcpy(p,parts[event_part++],(size_t)*n);return 0;}return -7;
}
static int bulk(libusb_device_handle*h,unsigned char ep,unsigned char*p,int size,int*n,unsigned timeout){
 assert(h);*n=0;
 if(ep==2){assert(timeout==100&&size==6);*n=size;return 0;}
 assert(ep==0x82&&timeout==1);static const unsigned char parts[][4]={{1,0x20,2,0},{0xaa,0xbb,0,0}};
 if(acl_part<2){*n=acl_part?2:4;assert(size>=*n);memcpy(p,parts[acl_part++],(size_t)*n);return 0;}return -7;
}
int main(void){
 external_usb_api api={init,quit,open_id,close_dev,device,descriptor,config,free_config,claim,release,control,interrupt,bulk};hci_external_test_api(&api);
 hci_t h={0};absent=1;assert(!hci_external_open(&h)&&quits==1&&!closes);absent=0;
 wrong_id=1;assert(!hci_external_open(&h)&&closes==1&&!releases);wrong_id=0;
 claim_fail=1;assert(!hci_external_open(&h)&&closes==2&&!releases);claim_fail=0;
 eps[0].bmAttributes=1;assert(!hci_external_open(&h)&&closes==3);eps[0].bmAttributes=3;
 assert(hci_external_open(&h));assert(!hci_external_open(&h));
 assert(h.ops->cmd(h.ctx,0x1001,NULL,0));assert(!h.ops->cmd(h.ctx,0x1001,NULL,256));
 unsigned char out[HCI_PKT_MAX];assert(h.ops->pump(h.ctx,0)==0);assert(h.ops->pump(h.ctx,0)==1);
 assert(h.ops->next_event(h.ctx,out,sizeof out)==5&&out[0]==0x0e&&out[4]==0x10);
 assert(h.ops->next_acl(h.ctx,out,sizeof out)==6&&out[4]==0xaa&&out[5]==0xbb);
 assert(h.ops->acl_send(h.ctx,out,6));assert(!h.ops->acl_send(h.ctx,out,5));
 removed=1;assert(h.ops->pump(h.ctx,0)==-1);h.ops->close(h.ctx);assert(closes==4&&releases==1);
 h.ops->close(h.ctx);assert(closes==4&&releases==1);
 removed=0;assert(hci_external_open(&h));h.ops->close(h.ctx);assert(closes==5&&releases==2);
 puts("external USB identity, endpoint validation, command/ACL I/O, fragmentation, removal and cleanup passed");
}
