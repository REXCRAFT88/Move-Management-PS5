#ifndef STUDIO_USB_REGISTER_H
#define STUDIO_USB_REGISTER_H
#include "host.h"
void usb_register_poll(host_t *host);
int usb_register_start(void);
void usb_register_stop(void);
#endif
