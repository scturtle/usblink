#include "usb.h"

#include <string.h>

extern "C" {
#include <switch/result.h>
#include <switch/services/usbds.h>
#include <switch/types.h>
}

#define TOTAL_INTERFACES 4

typedef struct {
  bool initialized;
  UsbDsInterface *interface;
  UsbDsEndpoint *endpoint_in, *endpoint_out;
} usbCommsInterface;

static bool g_usbCommsInitialized = false;

static usbCommsInterface g_usbCommsInterfaces[TOTAL_INTERFACES];

static Result _usbCommsInterfaceInit5x(u32 intf_ind);

static Result usbCommsInitializeEx(u32 num_interfaces) {
  Result rc = 0;

  rc = usbDsInitialize();

  if (R_SUCCEEDED(rc)) {
    u8 iManufacturer, iProduct, iSerialNumber;
    static const u16 supported_langs[1] = {0x0409};
    // Send language descriptor
    rc = usbDsAddUsbLanguageStringDescriptor(
        NULL, supported_langs, sizeof(supported_langs) / sizeof(u16));
    // Send manufacturer
    if (R_SUCCEEDED(rc))
      rc = usbDsAddUsbStringDescriptor(&iManufacturer, "Nintendo");
    // Send product
    if (R_SUCCEEDED(rc))
      rc = usbDsAddUsbStringDescriptor(&iProduct, "Nintendo Switch");
    // Send serial number
    if (R_SUCCEEDED(rc))
      rc = usbDsAddUsbStringDescriptor(&iSerialNumber, "SerialNumber");

    // Send device descriptors
    struct usb_device_descriptor device_descriptor = {
        .bLength = USB_DT_DEVICE_SIZE,
        .bDescriptorType = USB_DT_DEVICE,
        .bcdUSB = 0x0110,
        .bDeviceClass = 0x00,
        .bDeviceSubClass = 0x00,
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize0 = 0x40,
        .idVendor = 0x057e,
        .idProduct = 0x3000,
        .bcdDevice = 0x0100,
        .iManufacturer = iManufacturer,
        .iProduct = iProduct,
        .iSerialNumber = iSerialNumber,
        .bNumConfigurations = 0x01};
    // Full Speed is USB 1.1
    if (R_SUCCEEDED(rc))
      rc = usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_Full, &device_descriptor);

    // High Speed is USB 2.0
    device_descriptor.bcdUSB = 0x0200;
    if (R_SUCCEEDED(rc))
      rc = usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_High, &device_descriptor);

    // Super Speed is USB 3.0
    device_descriptor.bcdUSB = 0x0300;
    // Upgrade packet size to 512
    device_descriptor.bMaxPacketSize0 = 0x09;
    if (R_SUCCEEDED(rc))
      rc =
          usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_Super, &device_descriptor);

    // Define Binary Object Store
    u8 bos[0x16] = {0x05,       // .bLength
                    USB_DT_BOS, // .bDescriptorType
                    0x16, 0x00, // .wTotalLength
                    0x02,       // .bNumDeviceCaps

                    // USB 2.0
                    0x07,                     // .bLength
                    USB_DT_DEVICE_CAPABILITY, // .bDescriptorType
                    0x02,                     // .bDevCapabilityType
                    0x02, 0x00, 0x00, 0x00,   // dev_capability_data

                    // USB 3.0
                    0x0A,                     // .bLength
                    USB_DT_DEVICE_CAPABILITY, // .bDescriptorType
                    0x03,                     // .bDevCapabilityType
                    0x00, 0x0E, 0x00, 0x03, 0x00, 0x00, 0x00};
    if (R_SUCCEEDED(rc))
      rc = usbDsSetBinaryObjectStore(bos, sizeof(bos));

    if (R_SUCCEEDED(rc)) {
      for (u32 i = 0; i < num_interfaces; i++) {
        usbCommsInterface *intf = &g_usbCommsInterfaces[i];
        rc = _usbCommsInterfaceInit5x(i);
        if (R_FAILED(rc)) {
          break;
        }
      }
    }
  }

  if (R_SUCCEEDED(rc)) {
    rc = usbDsEnable();
  }

  if (R_SUCCEEDED(rc)) {
    g_usbCommsInitialized = true;
  }

  if (R_FAILED(rc)) {
    custom_usbCommsExit();
  }

  return rc;
}

Result custom_usbCommsInitialize() { return usbCommsInitializeEx(1); }

static void _usbCommsInterfaceFree(usbCommsInterface *interface) {
  if (!interface->initialized)
    return;
  interface->initialized = 0;
  interface->endpoint_in = NULL;
  interface->endpoint_out = NULL;
  interface->interface = NULL;
}

void custom_usbCommsExit() {
  usbDsExit();
  g_usbCommsInitialized = false;
  for (u32 i = 0; i < TOTAL_INTERFACES; i++)
    _usbCommsInterfaceFree(&g_usbCommsInterfaces[i]);
}

static Result _usbCommsInterfaceInit5x(u32 intf_ind) {
  Result rc = 0;
  usbCommsInterface *interface = &g_usbCommsInterfaces[intf_ind];

  struct usb_interface_descriptor interface_descriptor = {
      .bLength = USB_DT_INTERFACE_SIZE,
      .bDescriptorType = USB_DT_INTERFACE,
      .bInterfaceNumber = 4,
      .bNumEndpoints = 2,
      .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
      .bInterfaceSubClass = USB_CLASS_VENDOR_SPEC,
      .bInterfaceProtocol = USB_CLASS_VENDOR_SPEC,
  };

  struct usb_endpoint_descriptor endpoint_descriptor_in = {
      .bLength = USB_DT_ENDPOINT_SIZE,
      .bDescriptorType = USB_DT_ENDPOINT,
      .bEndpointAddress = USB_ENDPOINT_IN,
      .bmAttributes = USB_TRANSFER_TYPE_BULK,
      .wMaxPacketSize = 0x40,
  };

  struct usb_endpoint_descriptor endpoint_descriptor_out = {
      .bLength = USB_DT_ENDPOINT_SIZE,
      .bDescriptorType = USB_DT_ENDPOINT,
      .bEndpointAddress = USB_ENDPOINT_OUT,
      .bmAttributes = USB_TRANSFER_TYPE_BULK,
      .wMaxPacketSize = 0x40,
  };

  struct usb_ss_endpoint_companion_descriptor endpoint_companion = {
      .bLength = sizeof(struct usb_ss_endpoint_companion_descriptor),
      .bDescriptorType = USB_DT_SS_ENDPOINT_COMPANION,
      .bMaxBurst = 0x0F,
      .bmAttributes = 0x00,
      .wBytesPerInterval = 0x00,
  };

  interface->initialized = 1;

  if (R_FAILED(rc))
    return rc;

  rc = usbDsRegisterInterface(&interface->interface);
  if (R_FAILED(rc))
    return rc;

  interface_descriptor.bInterfaceNumber = interface->interface->interface_index;
  endpoint_descriptor_in.bEndpointAddress +=
      interface_descriptor.bInterfaceNumber + 1;
  endpoint_descriptor_out.bEndpointAddress +=
      interface_descriptor.bInterfaceNumber + 1;

  // Full Speed Config
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_Full, &interface_descriptor,
      USB_DT_INTERFACE_SIZE);
  if (R_FAILED(rc))
    return rc;
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_Full, &endpoint_descriptor_in,
      USB_DT_ENDPOINT_SIZE);
  if (R_FAILED(rc))
    return rc;
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_Full, &endpoint_descriptor_out,
      USB_DT_ENDPOINT_SIZE);
  if (R_FAILED(rc))
    return rc;

  // High Speed Config
  endpoint_descriptor_in.wMaxPacketSize = 0x200;
  endpoint_descriptor_out.wMaxPacketSize = 0x200;
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_High, &interface_descriptor,
      USB_DT_INTERFACE_SIZE);
  if (R_FAILED(rc))
    return rc;
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_High, &endpoint_descriptor_in,
      USB_DT_ENDPOINT_SIZE);
  if (R_FAILED(rc))
    return rc;
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_High, &endpoint_descriptor_out,
      USB_DT_ENDPOINT_SIZE);
  if (R_FAILED(rc))
    return rc;

  // Super Speed Config
  endpoint_descriptor_in.wMaxPacketSize = 0x400;
  endpoint_descriptor_out.wMaxPacketSize = 0x400;
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_Super, &interface_descriptor,
      USB_DT_INTERFACE_SIZE);
  if (R_FAILED(rc))
    return rc;
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_Super, &endpoint_descriptor_in,
      USB_DT_ENDPOINT_SIZE);
  if (R_FAILED(rc))
    return rc;
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_Super, &endpoint_companion,
      USB_DT_SS_ENDPOINT_COMPANION_SIZE);
  if (R_FAILED(rc))
    return rc;
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_Super, &endpoint_descriptor_out,
      USB_DT_ENDPOINT_SIZE);
  if (R_FAILED(rc))
    return rc;
  rc = usbDsInterface_AppendConfigurationData(
      interface->interface, UsbDeviceSpeed_Super, &endpoint_companion,
      USB_DT_SS_ENDPOINT_COMPANION_SIZE);
  if (R_FAILED(rc))
    return rc;

  // Setup endpoints.
  rc = usbDsInterface_RegisterEndpoint(interface->interface,
                                       &interface->endpoint_in,
                                       endpoint_descriptor_in.bEndpointAddress);
  if (R_FAILED(rc))
    return rc;

  rc = usbDsInterface_RegisterEndpoint(
      interface->interface, &interface->endpoint_out,
      endpoint_descriptor_out.bEndpointAddress);
  if (R_FAILED(rc))
    return rc;

  rc = usbDsInterface_EnableInterface(interface->interface);
  if (R_FAILED(rc))
    return rc;

  return rc;
}

enum UsbCommsDirection {
  USB_COMMS_READ,
  USB_COMMS_WRITE,
};

static Result _usbCommsTransfer(UsbDsEndpoint *ep, void *buffer, size_t size,
                                size_t *transferredSize, u64 timeout) {
  Result rc = 0;
  u32 urbId = 0;
  u8 *bufptr = (u8 *)buffer;
  u32 tmp_size = 0;
  size_t total_transferredSize = 0;
  UsbDsReportData reportdata;

  if (((u64)bufptr) & 0xfff)
    return LibnxError_ShouldNotHappen;

  // Makes sure endpoints are ready for data-transfer / wait for init if needed.
  rc = usbDsWaitReady(timeout);
  if (R_FAILED(rc))
    return rc;

  while (size) {
    // Start a host->device transfer.
    rc = usbDsEndpoint_PostBufferAsync(ep, bufptr, size, &urbId);
    if (R_FAILED(rc))
      return rc;

    // Wait for the transfer to finish.
    rc = eventWait(&ep->CompletionEvent, timeout);
    // timeout
    if (R_FAILED(rc)) {
      usbDsEndpoint_Cancel(ep);
      eventWait(&ep->CompletionEvent, UINT64_MAX);
      eventClear(&ep->CompletionEvent);
      return rc;
    }
    eventClear(&ep->CompletionEvent);

    rc = usbDsEndpoint_GetReportData(ep, &reportdata);
    if (R_FAILED(rc))
      return rc;

    rc = usbDsParseReportData(&reportdata, urbId, nullptr, &tmp_size);
    if (R_FAILED(rc))
      return rc;

    if (tmp_size > size)
      tmp_size = size;
    total_transferredSize += (size_t)tmp_size;

    bufptr += tmp_size;
    size -= tmp_size;

    if (tmp_size < size)
      break;
  }

  if (transferredSize)
    *transferredSize = total_transferredSize;

  return rc;
}

static size_t usbCommsTransferEx(void *buffer, size_t size, u32 interface,
                                 UsbCommsDirection dir, u64 timeout) {
  size_t transferredSize = 0;
  UsbState state;
  Result rc, rc2;
  usbCommsInterface *inter = &g_usbCommsInterfaces[interface];
  UsbDsEndpoint *ep =
      dir == USB_COMMS_READ ? inter->endpoint_out : inter->endpoint_in;
  bool initialized = inter->initialized;
  if (!initialized)
    return 0;
  rc = _usbCommsTransfer(ep, buffer, size, &transferredSize, timeout);
  if (R_FAILED(rc))
    return 0;
  return transferredSize;
}

size_t custom_usbCommsRead(void *buffer, size_t size, u64 timeout) {
  return usbCommsTransferEx(buffer, size, 0, USB_COMMS_READ, timeout);
}

size_t custom_usbCommsWrite(void *buffer, size_t size, u64 timeout) {
  return usbCommsTransferEx(buffer, size, 0, USB_COMMS_WRITE, timeout);
}
