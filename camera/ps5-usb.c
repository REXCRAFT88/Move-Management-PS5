/* PS5 ABI adapter for libuvc. Uses the SDK's FreeBSD libusb ABI. */
#include <libusb.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include "ps5-usb.h"
static void *module;
static char problem[128];
static int available;
const char *camera_usb_error(void){return problem;}
int camera_usb_load(void){
 if(module)return available;
 module=dlopen("/system/common/lib/libSceUsbd.sprx",RTLD_LAZY);
 if(!module){snprintf(problem,sizeof problem,"USB library unavailable");return 0;}
 const char*required[]={"sceUsbdInit","sceUsbdExit","sceUsbdGetDeviceList","sceUsbdGetConfigDescriptor","sceUsbdGetDeviceDescriptor","sceUsbdRefDevice","sceUsbdUnrefDevice","sceUsbdHandleEventsTimeout","sceUsbdAllocTransfer","sceUsbdFreeTransfer","sceUsbdSubmitTransfer","sceUsbdCancelTransfer","sceUsbdFillIsoTransfer","sceUsbdFillBulkTransfer","sceUsbdFillInterruptTransfer","sceUsbdFreeConfigDescriptor","sceUsbdFreeDeviceList","sceUsbdOpen","sceUsbdClose","sceUsbdGetDevice","sceUsbdClaimInterface","sceUsbdReleaseInterface","sceUsbdSetInterfaceAltSetting","sceUsbdControlTransfer"};
 for(unsigned i=0;i<sizeof required/sizeof required[0];i++)if(!dlsym(module,required[i])){snprintf(problem,sizeof problem,"Missing USB export: %s",required[i]);return 0;}
 available=1;return 1;
}
static void *symbol(const char *name){void *p=module?dlsym(module,name):NULL;if(!p)snprintf(problem,sizeof problem,"Missing USB export: %s",name);return p;}
int libusb_init(libusb_context **context){if(!camera_usb_load())return LIBUSB_ERROR_NOT_SUPPORTED;int(*f)(void)=symbol("sceUsbdInit");if(!f)return LIBUSB_ERROR_NOT_SUPPORTED;int rc=f();if(!rc&&context)*context=(libusb_context*)1;return rc;}
void libusb_exit(libusb_context *context){(void)context;void(*f)(void)=symbol("sceUsbdExit");if(f)f();}
ssize_t libusb_get_device_list(libusb_context *context,libusb_device ***list){(void)context;ssize_t(*f)(libusb_device***)=symbol("sceUsbdGetDeviceList");return f?f(list):LIBUSB_ERROR_NOT_SUPPORTED;}
int libusb_handle_events_completed(libusb_context *context,int *completed){(void)context;if(completed&&*completed)return 0;int(*f)(struct timeval*)=symbol("sceUsbdHandleEventsTimeout");struct timeval tv={0,50000};return f?f(&tv):LIBUSB_ERROR_NOT_SUPPORTED;}
/* Never detach the console's USB drivers. Upstream accepts NOT_SUPPORTED. */
int libusb_detach_kernel_driver(libusb_device_handle*d,int i){(void)d;(void)i;return LIBUSB_ERROR_NOT_SUPPORTED;}
int libusb_attach_kernel_driver(libusb_device_handle*d,int i){(void)d;(void)i;return LIBUSB_ERROR_NOT_SUPPORTED;}
int libusb_get_ss_endpoint_companion_descriptor(libusb_context*c,const struct libusb_endpoint_descriptor*e,struct libusb_ss_endpoint_companion_descriptor**s){(void)c;(void)e;*s=NULL;return LIBUSB_ERROR_NOT_SUPPORTED;}
void libusb_free_ss_endpoint_companion_descriptor(struct libusb_ss_endpoint_companion_descriptor*s){(void)s;}
const char *libusb_error_name(int e){(void)e;return "PS5 USB error";}
struct libusb_transfer *libusb_alloc_transfer(int iso_packets){struct libusb_transfer *(*f)(int iso_packets)=symbol("sceUsbdAllocTransfer");return f?f(iso_packets):NULL;}
int libusb_cancel_transfer(struct libusb_transfer *transfer){int (*f)(struct libusb_transfer *transfer)=symbol("sceUsbdCancelTransfer");return f?f(transfer):LIBUSB_ERROR_NOT_SUPPORTED;}
int libusb_claim_interface(libusb_device_handle * devh, int interface_number){int (*f)(libusb_device_handle * devh, int interface_number)=symbol("sceUsbdClaimInterface");return f?f(devh,interface_number):LIBUSB_ERROR_NOT_SUPPORTED;}
void libusb_close(libusb_device_handle * devh){void (*f)(libusb_device_handle * devh)=symbol("sceUsbdClose");if(f)f(devh);}
int libusb_control_transfer(libusb_device_handle * devh, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t wLength, uint32_t timeout){int (*f)(libusb_device_handle * devh, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t wLength, uint32_t timeout)=symbol("sceUsbdControlTransfer");return f?f(devh,bmRequestType,bRequest,wValue,wIndex,data,wLength,timeout):LIBUSB_ERROR_NOT_SUPPORTED;}
void libusb_fill_bulk_transfer(struct libusb_transfer *transfer, libusb_device_handle *devh, uint8_t endpoint, uint8_t *buf, int length, libusb_transfer_cb_fn callback, void *user_data, uint32_t timeout){void (*f)(struct libusb_transfer *transfer, libusb_device_handle *devh, uint8_t endpoint, uint8_t *buf, int length, libusb_transfer_cb_fn callback, void *user_data, uint32_t timeout)=symbol("sceUsbdFillBulkTransfer");if(f)f(transfer,devh,endpoint,buf,length,callback,user_data,timeout);}
void libusb_fill_interrupt_transfer(struct libusb_transfer *transfer, libusb_device_handle *devh, uint8_t endpoint, uint8_t *buf, int length, libusb_transfer_cb_fn callback, void *user_data, uint32_t timeout){void (*f)(struct libusb_transfer *transfer, libusb_device_handle *devh, uint8_t endpoint, uint8_t *buf, int length, libusb_transfer_cb_fn callback, void *user_data, uint32_t timeout)=symbol("sceUsbdFillInterruptTransfer");if(f)f(transfer,devh,endpoint,buf,length,callback,user_data,timeout);}
void libusb_fill_iso_transfer(struct libusb_transfer *transfer, libusb_device_handle *devh, uint8_t endpoint, uint8_t *buf, int length, int npacket, libusb_transfer_cb_fn callback, void *user_data, uint32_t timeout){void (*f)(struct libusb_transfer *transfer, libusb_device_handle *devh, uint8_t endpoint, uint8_t *buf, int length, int npacket, libusb_transfer_cb_fn callback, void *user_data, uint32_t timeout)=symbol("sceUsbdFillIsoTransfer");if(f)f(transfer,devh,endpoint,buf,length,npacket,callback,user_data,timeout);}
void libusb_free_config_descriptor(struct libusb_config_descriptor *config){void (*f)(struct libusb_config_descriptor *config)=symbol("sceUsbdFreeConfigDescriptor");if(f)f(config);}
void libusb_free_device_list(libusb_device ** list, int unref_devices){void (*f)(libusb_device ** list, int unref_devices)=symbol("sceUsbdFreeDeviceList");if(f)f(list,unref_devices);}
void libusb_free_transfer(struct libusb_transfer *transfer){void (*f)(struct libusb_transfer *transfer)=symbol("sceUsbdFreeTransfer");if(f)f(transfer);}
uint8_t libusb_get_bus_number(libusb_device * dev){uint8_t (*f)(libusb_device * dev)=symbol("sceUsbdGetBusNumber");return f?f(dev):LIBUSB_ERROR_NOT_SUPPORTED;}
int libusb_get_config_descriptor(libusb_device * dev, uint8_t config_index, struct libusb_config_descriptor **config){int (*f)(libusb_device * dev, uint8_t config_index, struct libusb_config_descriptor **config)=symbol("sceUsbdGetConfigDescriptor");return f?f(dev,config_index,config):LIBUSB_ERROR_NOT_SUPPORTED;}
libusb_device *libusb_get_device(libusb_device_handle * devh){libusb_device *(*f)(libusb_device_handle * devh)=symbol("sceUsbdGetDevice");return f?f(devh):NULL;}
uint8_t libusb_get_device_address(libusb_device * dev){uint8_t (*f)(libusb_device * dev)=symbol("sceUsbdGetDeviceAddress");return f?f(dev):LIBUSB_ERROR_NOT_SUPPORTED;}
int libusb_get_device_descriptor(libusb_device * dev, struct libusb_device_descriptor *desc){int (*f)(libusb_device * dev, struct libusb_device_descriptor *desc)=symbol("sceUsbdGetDeviceDescriptor");return f?f(dev,desc):LIBUSB_ERROR_NOT_SUPPORTED;}
int libusb_get_string_descriptor_ascii(libusb_device_handle * devh, uint8_t desc_index, uint8_t *data, int length){int (*f)(libusb_device_handle * devh, uint8_t desc_index, uint8_t *data, int length)=symbol("sceUsbdGetStringDescriptorAscii");return f?f(devh,desc_index,data,length):LIBUSB_ERROR_NOT_SUPPORTED;}
int libusb_open(libusb_device * dev, libusb_device_handle ** devh){int (*f)(libusb_device * dev, libusb_device_handle ** devh)=symbol("sceUsbdOpen");return f?f(dev,devh):LIBUSB_ERROR_NOT_SUPPORTED;}
libusb_device *libusb_ref_device(libusb_device * dev){libusb_device *(*f)(libusb_device * dev)=symbol("sceUsbdRefDevice");return f?f(dev):NULL;}
int libusb_release_interface(libusb_device_handle * devh, int interface_number){int (*f)(libusb_device_handle * devh, int interface_number)=symbol("sceUsbdReleaseInterface");return f?f(devh,interface_number):LIBUSB_ERROR_NOT_SUPPORTED;}
int libusb_set_interface_alt_setting(libusb_device_handle * devh, int interface_number, int alternate_setting){int (*f)(libusb_device_handle * devh, int interface_number, int alternate_setting)=symbol("sceUsbdSetInterfaceAltSetting");return f?f(devh,interface_number,alternate_setting):LIBUSB_ERROR_NOT_SUPPORTED;}
int libusb_submit_transfer(struct libusb_transfer *transfer){int (*f)(struct libusb_transfer *transfer)=symbol("sceUsbdSubmitTransfer");return f?f(transfer):LIBUSB_ERROR_NOT_SUPPORTED;}
void libusb_unref_device(libusb_device * dev){void (*f)(libusb_device * dev)=symbol("sceUsbdUnrefDevice");if(f)f(dev);}
uint8_t *libusb_get_iso_packet_buffer_simple(struct libusb_transfer *t,uint32_t index){
 if(!t||index>=(uint32_t)t->num_iso_packets)return NULL;
 size_t offset=(size_t)t->iso_packet_desc[0].length*index;
 if(offset>(size_t)t->length)return NULL;
 return t->buffer+offset;
}
void libusb_set_iso_packet_lengths(struct libusb_transfer *t,uint32_t length){if(t)for(int i=0;i<t->num_iso_packets;i++)t->iso_packet_desc[i].length=length;}
