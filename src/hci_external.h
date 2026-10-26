/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MOVE_HCI_EXTERNAL_H
#define MOVE_HCI_EXTERNAL_H
#include "hci.h"
#include <libusb.h>
/* Narrow API surface permits transport tests without opening real hardware. */
typedef struct {
 int (*init)(void); void (*quit)(void);
 libusb_device_handle *(*open_id)(uint16_t,uint16_t);
 void (*close)(libusb_device_handle*);
 libusb_device *(*device)(libusb_device_handle*);
 int (*descriptor)(libusb_device*,struct libusb_device_descriptor*);
 int (*config)(libusb_device*,uint8_t,struct libusb_config_descriptor**);
 void (*free_config)(struct libusb_config_descriptor*);
 int (*claim)(libusb_device_handle*,int);
 int (*release)(libusb_device_handle*,int);
 int (*control)(libusb_device_handle*,uint8_t,uint8_t,uint16_t,uint16_t,unsigned char*,uint16_t,uint32_t);
 int (*interrupt)(libusb_device_handle*,unsigned char,unsigned char*,int,int*,unsigned int);
 int (*bulk)(libusb_device_handle*,unsigned char,unsigned char*,int,int*,unsigned int);
} external_usb_api;
int hci_external_open(hci_t *out);
#ifdef EXTERNAL_BT_TEST
void hci_external_test_api(const external_usb_api *api);
#endif
#endif
