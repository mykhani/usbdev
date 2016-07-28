/*
 * usb.h
 *
 *  Created on: Jun 22, 2016
 *      Author: ykhan
 */

#ifndef USB_H_
#define USB_H_

/* TODO:
 * rename EPO_SIZE to EP0_MAX_PKT_SZ
 */
#define EP0_SIZE 64
#define SETUP_PKT_SIZE 8

/* Defines for descriptor values */
#define USB_VER_2			0x0200
#define USB_CLASS_VENDOR	0xFF


/* defines for bmAttributes bitmap */
#define USB_SELF_POWERED	(1 << 6)


/* define for string descriptor */
#define LANGUAGES_SUPPORTED		0x01
#define LANGID_ENGLISH			0x0409	/* USB LANGID ver 1.0 */

/* defines for descriptor types */
#define DESC_TYPE_DEVICE			0x01
#define DESC_TYPE_CONFIG			0x02
#define DESC_TYPE_STRING			0x03
#define DESC_TYPE_INTERFACE			0x04
#define DESC_TYPE_ENDPOINT			0x05
#define DESC_TYPE_DEV_QUALIFIER		0x06 /*for USB 2.0 devices */

/* defines for endpoint descriptor */

/* Bitmask for bEndpointAddress field of endpoint descriptor
 *
 * Bits 0..3b Endpoint Number.
 * Bits 4..6b Reserved. Set to Zero
 * Bits 7 Direction 0 = Out, 1 = In (Ignored for Control Endpoints)
 */
#define EPT_ADDRESS_MASK	(0x0F)
#define EPT_DIR_OUT			(0 << 7)
#define EPT_DIR_IN			(1 << 7)

/* Bitmask for bmAttributes field of endpoint descriptor
 *
 * Bits 0..1 Transfer Type
 * 00 = Control
 * 01 = Isochronous
 * 10 = Bulk
 * 11 = Interrupt
 * Bits 2..7 are reserved. If Isochronous endpoint,
 * Bits 3..2 = Synchronisation Type (Iso Mode)
 * 00 = No Synchonisation
 * 01 = Asynchronous
 * 10 = Adaptive
 * 11 = Synchronous
 * Bits 5..4 = Usage Type (Iso Mode)
 * 00 = Data Endpoint
 * 01 = Feedback Endpoint
 * 10 = Explicit Feedback Data Endpoint
 * 11 = Reserved
 */
#define EPT_XFER_CONTROL	(0 << 0)
#define EPT_XFER_ISO		(1 << 0)
#define EPT_XFER_BULK		(2 << 0)
#define EPT_XFER_INT		(3 << 0)

enum control_state {
	STATE_IDLE,
	STATE_STATUS,
	STATE_RX,
	STATE_TX,
};

enum device_request {
	GET_STATUS = 0x00,
	CLEAR_FEATURE = 0x01,
	SET_FEATURE = 0x03,
	SET_ADDRESS = 0x05,
	GET_DESCRIPTOR = 0x06,
	SET_DESCRIPTOR = 0x07,
	GET_CONFIGURATION = 0x08,
	SET_CONFIGURATION = 0x09
};

/* TODO
 * rename setup_packet to usb_request
 */
struct setup_packet {
	char bmRequestType;
	char bRequest;
	unsigned short wValue;
	unsigned short wIndex;
	unsigned short wLength;
};

/* Descriptors */

struct device_qualifier {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned short bcdUSB;
	unsigned char bDeviceClass;
	unsigned char bDeviceSubClass;
	unsigned char bDeviceProtocol;
	unsigned char bMaxPacketSize0;
	unsigned char bNumConfigurations;
	unsigned char bReserved;
} __attribute__((packed));

struct device_descriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned short bcdUSB;
	unsigned char bDeviceClass;
	unsigned char bDeviceSubClass;
	unsigned char bDeviceProtocol;
	unsigned char bMaxPacketSize;
	unsigned short idVendor;
	unsigned short idProduct;
	unsigned short bcdDevice;
	unsigned char iManufacturer;
	unsigned char iProduct;
	unsigned char iSerialNumber;
	unsigned char bNumConfigurations;
} __attribute__((packed));

struct config_descriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned short wTotalLength;
	unsigned char bNumInterfaces;
	unsigned char bConfigurationValue;
	unsigned char iConfiguration;
	unsigned char bmAttributes;
	unsigned char bMaxPower;
} __attribute__((packed));

struct interface_descriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned char bInterfaceNumber;
	unsigned char bAlternateSetting;
	unsigned char bNumEndpoints;
	unsigned char bInterfaceClass;
	unsigned char bInterfaceSubClass;
	unsigned char bInterfaceProtocol;
	unsigned char iInterface;
} __attribute__((packed));

struct endpoint_descriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned char bEndpointAddress;
	unsigned char bmAttributes;
	unsigned short wMaxPacketSize;
	unsigned char bInterval;
} __attribute__((packed));

struct string_descriptor {
	unsigned char bLength;
	unsigned char bDescriptorType;
	unsigned short wLANGID[LANGUAGES_SUPPORTED];
} __attribute__((packed));

/* defines for usb controller specific flags */
#define USB_SET_DEVICE_ADDRESS 		(1 << 0)
#define USB_REMOTE_WAKEUP_PENDING 	(1 << 1)
#define USB_DEVICE_CONFIGURED		(1 << 2)

struct usb_device {
	volatile unsigned short dev_addr;
	volatile enum control_state ep0_state;
	volatile unsigned long flags; /* various usb controller specific flags */
	volatile unsigned long resume_ticks_1ms; /* sof based ticks for resume duration */
	char *ep0_tx_data;
	unsigned char ep0_tx_remaining;
};

#endif /* USB_H_ */
