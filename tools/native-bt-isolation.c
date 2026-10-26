/* Finite transport-only diagnostic: no HCI commands, resets, pairing or outputs.
 * Build twice: OPEN_ONLY=1 claims transfers but never queues reads; =0 pumps.
 * Must be run with the bridge stopped. Camera need not be stopped separately. */
#include "hci_usb.h"
#include "log.h"
#include "util.h"
#include <stdio.h>
#include <unistd.h>
#ifndef OPEN_ONLY
#define OPEN_ONLY 1
#endif
int main(void){
 log_open("/data/move-studio/native-bt-isolation.log");
 hci_t h={0};log_line("native pairing isolation: open_only=%d; duration=45 seconds; no HCI commands",OPEN_ONLY);
 if(!hci_usb_open(&h)){log_line("transport open failed");return 1;}
 long end=now_ms()+45000;
 while(now_ms()<end){
  if(OPEN_ONLY){usleep(10000);continue;}
  if(h.ops->pump(h.ctx,10)<0)break;
  unsigned char event[HCI_PKT_MAX];int n;
  while((n=h.ops->next_event(h.ctx,event,sizeof event))>0)log_line("event=%02x length=%d",event[0],n);
  while(h.ops->next_acl(h.ctx,event,sizeof event)>0){}
 }
 h.ops->close(h.ctx);log_line("transport closed; diagnostic complete");log_close();return 0;
}

