#ifndef TEST_USB_TYPES_H
#define TEST_USB_TYPES_H
#include <stdint.h>
typedef struct libusb_device libusb_device;typedef struct libusb_device_handle libusb_device_handle;
struct libusb_device_descriptor{uint8_t bLength,bDescriptorType;uint16_t bcdUSB;uint8_t bDeviceClass,bDeviceSubClass,bDeviceProtocol,bMaxPacketSize0;uint16_t idVendor,idProduct,bcdDevice;uint8_t iManufacturer,iProduct,iSerialNumber,bNumConfigurations;};
struct libusb_endpoint_descriptor{uint8_t bEndpointAddress,bmAttributes;};
struct libusb_interface_descriptor{uint8_t bAlternateSetting,bInterfaceClass,bInterfaceSubClass,bInterfaceProtocol,bInterfaceNumber,bNumEndpoints;const struct libusb_endpoint_descriptor*endpoint;};
struct libusb_interface{int num_altsetting;const struct libusb_interface_descriptor*altsetting;};
struct libusb_config_descriptor{uint8_t bNumInterfaces;const struct libusb_interface*interface;};
#define LIBUSB_ERROR_IO -1
#define LIBUSB_ERROR_NO_DEVICE -4
#define LIBUSB_ERROR_TIMEOUT -7
#define LIBUSB_ERROR_PIPE -9
#endif
