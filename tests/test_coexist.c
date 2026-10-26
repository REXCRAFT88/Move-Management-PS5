#define MOVE_STUDIO_PASSIVE_SETUP
#define ANYPAD_RAW_DIAGNOSTIC
#include "../src/host.c"
#include "sim.h"
#include <assert.h>
static int (*original_cmd)(void*,unsigned,const void*,int);
static unsigned commands;
static int checked(void*c,unsigned op,const void*p,int n){assert(op==OP_READ_BUFFER_SIZE||op==OP_READ_BD_ADDR);commands++;return original_cmd(c,op,p,n);}
int main(void){sim_pad pad={0};sim_t*s=sim_new(&pad);host_t h={0};h.hci=sim_hci(s);hci_ops ops=*h.hci.ops;original_cmd=ops.cmd;ops.cmd=checked;h.hci.ops=&ops;assert(setup(&h));assert(commands==2);assert(h.scan_set==-1&&!h.le_ok);restore(&h);assert(commands==2);sim_free(s);puts("Move/Nav startup and shutdown preserve native Bluetooth configuration");}
