/* Drive real incoming HCI events through the host to check bounded multiplayer admission. */
#include "../src/host.c"
#include <assert.h>
static int accepts,rejects;
static int command(void*ctx,unsigned opcode,const void*params,int len){(void)ctx;(void)params;(void)len;if(opcode==OP_ACCEPT_CONNECTION)accepts++;if(opcode==OP_REJECT_CONNECTION)rejects++;return 1;}
int main(void){host_t h={0};hci_ops ops={.cmd=command};h.hci.ops=&ops;
 unsigned char addr[HOST_MAX_PADS+1][6];
 for(int i=0;i<=HOST_MAX_PADS;i++){memset(addr[i],0,6);addr[i][0]=(unsigned char)(i+1);assert(i%2?host_seed_usb_nav(&h,addr[i]):host_seed_usb_move(&h,addr[i]));}
 for(int i=0;i<=HOST_MAX_PADS;i++){unsigned char event[12]={4,10};memcpy(event+2,addr[i],6);event[11]=1;on_event(&h,event,12);}
 assert(accepts==HOST_MAX_PADS&&rejects==1);
 for(int i=0;i<HOST_MAX_PADS;i++){assert(h.links[i].used&&!memcmp(h.links[i].addr,addr[i],6));assert(h.links[i].pid==(i%2?0x042f:0x03d5));}
 assert(!link_by_addr(&h,addr[HOST_MAX_PADS]));
 link_free(&h,&h.links[1]);unsigned char event[12]={4,10};memcpy(event+2,addr[HOST_MAX_PADS],6);event[11]=1;on_event(&h,event,12);assert(accepts==HOST_MAX_PADS+1&&h.links[1].pid==0x03d5);assert(!memcmp(h.links[1].addr,addr[HOST_MAX_PADS],6));
 unsigned saved_count=h.usb_seed_count;
 h.links[0].connected=1;h.links[0].handle=0x41;assert(host_disconnect(&h,addr[0]));assert(h.links[0].disconnecting);assert(!h.links[2].disconnecting&&h.usb_seed_count==saved_count);assert(host_disconnect(&h,addr[0]));
 unsigned char unknown[6]={99,1,2,3,4,5};assert(!host_disconnect(&h,unknown));
 puts("Eight simultaneous physical links, mixed device types and ninth-device capacity checks passed");return 0;
}
