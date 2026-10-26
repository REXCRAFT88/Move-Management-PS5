/* Standard HCI transport candidates, not hardware compatibility claims.
 * IDs researched against Linux v6.12 drivers/bluetooth/btusb.c.
 * Firmware/clone quirks remain unimplemented. Only ASUS has PS5 validation. */
#ifndef EXTERNAL_ADAPTERS_H
#define EXTERNAL_ADAPTERS_H
#include <stdint.h>
typedef struct {uint16_t vid,pid;const char*name;} external_adapter;
static const external_adapter external_adapters[]={
 {0x0b05,0x190e,"ASUS USB-BT500 (tested)"},
 {0x2357,0x0604,"TP-Link / RTL8761BU (untested)"},
 {0x0bda,0x8771,"Realtek RTL8761BU (untested)"},
 {0x2550,0x8761,"Realtek 8761 family (untested)"},
 {0x6655,0x8771,"Realtek 8761 family (untested)"},
 {0x7392,0xc611,"Realtek 8761 family (untested)"},
 {0x2b89,0x8761,"Realtek 8761 family (untested)"},
 {0x0a12,0x0001,"CSR USB HCI (untested; clone quirks possible)"}
};
static inline int external_identity_allowed(unsigned vid,unsigned pid){
 /* Never permit Sony, Marvell internal radio or Intel integrated adapters. */
 return vid&&pid&&vid<=65535&&pid<=65535&&vid!=0x054c&&vid!=0x1286&&vid!=0x8087;
}
#endif
