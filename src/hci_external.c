/* SPDX-License-Identifier: GPL-3.0-or-later
 * External-only ASUS USB-BT500 transport. Never opens the internal radio.
 * Synchronous bounded transfers avoid pending callbacks at close/removal.
 * No reset, firmware load, driver detach or pairing is performed here. */
#include "hci_external.h"
#include "evstream.h"
#include "log.h"
#include "util.h"
#include "external_adapters.h"
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#define QUEUE 32
typedef struct {unsigned char packet[QUEUE][HCI_PKT_MAX];int length[QUEUE],head,count;} packets;
typedef struct {
 external_usb_api usb;libusb_device_handle *handle;
 int initialized,claimed,iface,dead;
 unsigned char events_ep,in_ep,out_ep;
 evstream events; packets event_queue,acl_queue;
 unsigned char acl[HCI_PKT_MAX];int acl_used;long acl_time;
} external_context;
static external_context context;
static external_usb_api api;
#ifdef EXTERNAL_BT_TEST
void hci_external_test_api(const external_usb_api *value){api=*value;}
#else
static int load_api(void){
 static void *module;
 if(!module)module=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);
 if(!module)return 0;
#define LOAD(field,name) do { *(void **)(&api.field)=dlsym(module,name);if(!api.field)return 0; } while(0)
 LOAD(init,"sceUsbdInit");LOAD(quit,"sceUsbdExit");
 LOAD(open_id,"sceUsbdOpenDeviceWithVidPid");LOAD(close,"sceUsbdClose");
 LOAD(device,"sceUsbdGetDevice");LOAD(descriptor,"sceUsbdGetDeviceDescriptor");
 LOAD(config,"sceUsbdGetConfigDescriptor");LOAD(free_config,"sceUsbdFreeConfigDescriptor");
 LOAD(claim,"sceUsbdClaimInterface");LOAD(release,"sceUsbdReleaseInterface");
 LOAD(control,"sceUsbdControlTransfer");LOAD(interrupt,"sceUsbdInterruptTransfer");LOAD(bulk,"sceUsbdBulkTransfer");
#undef LOAD
 return 1;
}
#endif
static int put(packets*q,const unsigned char*p,int n){
 if(n<=0||n>HCI_PKT_MAX||q->count==QUEUE)return 0;
 int at=(q->head+q->count)%QUEUE;memcpy(q->packet[at],p,(size_t)n);q->length[at]=n;q->count++;return 1;
}
static int take(packets*q,unsigned char*p,int max){
 if(!q->count)return 0;
 int n=q->length[q->head];if(max<n)return 0;
 memcpy(p,q->packet[q->head],(size_t)n);q->head=(q->head+1)%QUEUE;q->count--;return n;
}
static void event(void*opaque,const uint8_t*p,int n){external_context*c=opaque;if(!put(&c->event_queue,p,n))c->dead=1;}
static void acl_feed(external_context*c,const unsigned char*p,int n){
 long now=now_ms();if(c->acl_used&&now-c->acl_time>100)c->acl_used=0;c->acl_time=now;
 for(int i=0;i<n&&!c->dead;i++){
  c->acl[c->acl_used++]=p[i];
  if(c->acl_used>=4){int expected=4+c->acl[2]+256*c->acl[3];
   if(expected>HCI_PKT_MAX){c->dead=1;break;}
   if(c->acl_used==expected){if(!put(&c->acl_queue,c->acl,expected))c->dead=1;c->acl_used=0;}
  }
 }
}
static int error(external_context*c,int rc){
 if(rc==LIBUSB_ERROR_NO_DEVICE||rc==LIBUSB_ERROR_IO||rc==LIBUSB_ERROR_PIPE)c->dead=1;
 return c->dead;
}
static int command(void*opaque,unsigned op,const void*p,int n){
 external_context*c=opaque;unsigned char b[258];if(c->dead||n<0||n>255||(n&&!p)||op>65535)return 0;
 b[0]=(unsigned char)op;b[1]=(unsigned char)(op>>8);b[2]=(unsigned char)n;if(n)memcpy(b+3,p,(size_t)n);
 int rc=c->usb.control(c->handle,0x20,0,0,(uint16_t)c->iface,b,(uint16_t)(n+3),100);
 error(c,rc);
 if(rc!=n+3)log_line("external command %#x transfer result=%d expected=%d",op,rc,n+3);
 return rc==n+3;
}
static int acl_send(void*opaque,const unsigned char*p,int n){
 external_context*c=opaque;if(c->dead||!p||n<4||n>HCI_PKT_MAX||n!=4+p[2]+256*p[3])return 0;
 int sent=0,rc=c->usb.bulk(c->handle,c->out_ep,(unsigned char*)p,n,&sent,100);
 error(c,rc);if(rc||sent!=n){c->dead=1;return 0;}return 1;
}
static int pump(void*opaque,int timeout){
 external_context*c=opaque;long end=now_ms()+(timeout>0?timeout:0);
 do{
  if(c->dead)return -1;
  if(c->event_queue.count||c->acl_queue.count)return 1;
  unsigned char b[HCI_PKT_MAX];int n=0;
  int rc=c->usb.interrupt(c->handle,c->events_ep,b,64,&n,1);
  if(n<0||n>64){c->dead=1;return -1;}
  if(n)evstream_feed(&c->events,b,n,now_ms(),event,c);
  if(error(c,rc))return -1;
  n=0;rc=c->usb.bulk(c->handle,c->in_ep,b,sizeof b,&n,1);
  if(n<0||n>(int)sizeof b){c->dead=1;return -1;}
  if(n)acl_feed(c,b,n);
  if(error(c,rc)||c->dead)return -1;
  if(c->event_queue.count||c->acl_queue.count)return 1;
 }while(now_ms()<end);
 return 0;
}
static int next_event(void*c,unsigned char*p,int n){return take(&((external_context*)c)->event_queue,p,n);}
static int next_acl(void*c,unsigned char*p,int n){return take(&((external_context*)c)->acl_queue,p,n);}
static void close_transport(void*opaque){
 external_context*c=opaque;
 if(c->handle){if(c->claimed)c->usb.release(c->handle,c->iface);c->usb.close(c->handle);}
 if(c->initialized)c->usb.quit();
 c->handle=NULL;c->claimed=c->initialized=0;c->dead=1;
 /* All transfers are synchronous: no asynchronous callback outlives this. */
}
static void diagnostic(void*opaque){external_context*c=opaque;log_line("external ASUS only: iface=%d events=%02x ACL=%02x/%02x dead=%d",c->iface,c->events_ep,c->in_ep,c->out_ep,c->dead);}
static const hci_ops ops={command,acl_send,pump,next_event,next_acl,close_transport,diagnostic};
int hci_external_open(hci_t*out){
 external_context*c=&context;if(!out||c->handle)return 0;
#ifndef EXTERNAL_BT_TEST
 if(!load_api())return 0;
#endif
 memset(c,0,sizeof *c);c->usb=api;
 if(!api.init||api.init())return 0;
 c->initialized=1;
 unsigned selected_vid=0,selected_pid=0;
 for(unsigned i=0;i<sizeof external_adapters/sizeof external_adapters[0];i++){
  selected_vid=external_adapters[i].vid;selected_pid=external_adapters[i].pid;
  c->handle=api.open_id((uint16_t)selected_vid,(uint16_t)selected_pid);
  if(c->handle){log_line("external adapter: %s",external_adapters[i].name);break;}
 }
 /* Explicit generic opt-in for an unlisted external HCI VID/PID. Endpoint
  * validation below remains mandatory; never try arbitrary internal nodes. */
 if(!c->handle){
  FILE*f=fopen("/data/move-studio/external-adapter-id","r");char extra;
  int valid=f&&fscanf(f,"%x:%x %c",&selected_vid,&selected_pid,&extra)==2;
  if(f)fclose(f);
  if(valid&&external_identity_allowed(selected_vid,selected_pid))c->handle=api.open_id((uint16_t)selected_vid,(uint16_t)selected_pid);
 }
 if(!c->handle)goto fail;
 libusb_device*d=api.device(c->handle);struct libusb_device_descriptor dd;
 if(!d||api.descriptor(d,&dd)||dd.idVendor!=selected_vid||dd.idProduct!=selected_pid||!external_identity_allowed(dd.idVendor,dd.idProduct))goto fail;
 struct libusb_config_descriptor*cfg=NULL;
 if(api.config(d,0,&cfg)||!cfg)goto fail;
 for(int i=0;i<cfg->bNumInterfaces&&!c->events_ep;i++){
  for(int j=0;j<cfg->interface[i].num_altsetting&&!c->events_ep;j++){
   const struct libusb_interface_descriptor*a=&cfg->interface[i].altsetting[j];
   if(a->bAlternateSetting||a->bInterfaceClass!=0xe0||a->bInterfaceSubClass!=1||a->bInterfaceProtocol!=1)continue;
   unsigned char e=0,in=0,o=0;
   for(int k=0;k<a->bNumEndpoints;k++){const struct libusb_endpoint_descriptor*p=&a->endpoint[k];
    if((p->bmAttributes&3)==3&&(p->bEndpointAddress&0x80))e=p->bEndpointAddress;
    if((p->bmAttributes&3)==2){if(p->bEndpointAddress&0x80)in=p->bEndpointAddress;else o=p->bEndpointAddress;}
   }
   if(e&&in&&o){c->iface=a->bInterfaceNumber;c->events_ep=e;c->in_ep=in;c->out_ep=o;}
  }
 }
 api.free_config(cfg);if(!c->events_ep||api.claim(c->handle,c->iface))goto fail;
 c->claimed=1;evstream_init(&c->events);out->ops=&ops;out->ctx=c;diagnostic(c);return 1;
fail:close_transport(c);return 0;
}
