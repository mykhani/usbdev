/*
 * main.c
 */

#include <inc/hw_usb.h>
#include <inc/hw_memmap.h>
#include <inc/hw_sysctl.h>
#include <inc/hw_gpio.h>
#include <inc/hw_nvic.h>
#include "usb.h"
#include <string.h>

char ep1_data[64];
char ep1_tx_data[64] = {
		'1', '2', '3', '4', '5', '6', '7', '\n',
		'A', 'B', 'C', 'D', 'E', 'F', 'G', '\n',
		'1', '2', '3', '4', '5', '6', '7', '\n',
		'H', 'I', 'J', 'K', 'L', 'M', 'N', '\n',
		'1', '2', '3', '4', '5', '6', '7', '\n',
		'O', 'P', 'Q', 'R', 'S', 'T', 'U', '\n',
		'1', '2', '3', '4', '5', '6', '7', '\n',
		'V', 'W', 'X', 'Y', 'Z', 'A', 'B', '\n',
};

volatile int tx_done = 0;
volatile int rx_done = 0;

volatile struct usb_device usb_dev;

#define REQUEST_IN 0x1

#define reg32(addr) ((volatile unsigned long *)((unsigned long)(addr)))
#define reg16(addr) ((volatile unsigned short *)((unsigned long)(addr)))
#define reg8(addr) ((volatile unsigned char *)((unsigned long)(addr)))

#define write32(addr, val) (*((volatile unsigned long *) ((unsigned long) (addr))) = (val))
#define write16(addr, val) (*((volatile unsigned short *) ((unsigned long) (addr))) = (val))
#define write8(addr, val) (*((volatile unsigned char *) ((unsigned long) (addr))) = (val))

#define read32(addr) (*((volatile unsigned long *) (unsigned long) (addr)))
#define read16(addr) (*((volatile unsigned short *) (unsigned long) (addr)))
#define read8(addr) (*((volatile unsigned char *) (unsigned long) (addr)))

#define STRING_MANUFACTURER_IDX         1
#define STRING_PRODUCT_IDX              2
#define STRING_SERIAL_IDX               3

int desc_index = 0;

/* TODO
 * create string descriptors dynamically
 */
/*
 * string descriptors containing unicode encoded strings
 */
char manufacturer_string[] = {
		(9 + 1) * 2,
		DESC_TYPE_STRING,
		'Y', 0, 'a', 0, 's', 0, 'i', 0, 'r', 0,
		'K', 0, 'h', 0, 'a', 0, 'n', 0
};

char product_string[] = {
		(11 + 1) * 2,
		DESC_TYPE_STRING,
		'M', 0, 'y', 0, 'U', 0, 'S', 0, 'B', 0,
		'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0
};

char serial_string[] = {
		(11 + 1) * 2,
		DESC_TYPE_STRING,
		'0', 0, '3', 0, '1', 0, '3', 0, '5', 0,
		'2', 0, '8', 0, '4', 0, '3', 0, '2', 0, '1', 0
};

/* device specific defines */
#define PRODUCT_ID	0x11bb
#define VENDOR_ID	0xccbb

const struct device_descriptor dev_desc = {
	sizeof (struct device_descriptor),
	DESC_TYPE_DEVICE,
	USB_VER_2,
	USB_CLASS_VENDOR,
	0x00,
	0x00,
	EP0_SIZE,
	VENDOR_ID,
	PRODUCT_ID,
	0x0000, // bcd device
	STRING_MANUFACTURER_IDX,	// index for manufacturer string desc.
	STRING_PRODUCT_IDX,	// index for product string desc.
	STRING_SERIAL_IDX,	// index for serial string desc.
	1	// number of configurations
};

const struct device_qualifier dev_qualifier = {
		sizeof (struct device_qualifier),
		DESC_TYPE_DEV_QUALIFIER,
		USB_VER_2,
		USB_CLASS_VENDOR,
		0x00,
		0x00,
		EP0_SIZE,
		0x01,
		0x00
};

/* device configuration for this device
 * Note that it consists of 1 interface
 * and interface consists of 2 endpoints
 * TODO: Make this structure flexible so that it can support
 * any configuration consisting of any number of interfaces
 * and endpoints
 */
struct usb_configuration {
	struct config_descriptor config_desc;
	struct interface_descriptor interface_desc;
	struct endpoint_descriptor ep1_in_desc;
	struct endpoint_descriptor ep1_out_desc;
};

struct usb_configuration dev_config;

struct config_descriptor dev_config_desc = {
		.bLength 				= sizeof(struct config_descriptor),
		.bDescriptorType 		= DESC_TYPE_CONFIG,
		.wTotalLength			= sizeof(dev_config),
		.bNumInterfaces			= 0x01,
		.bConfigurationValue	= 0x01,
		.iConfiguration			= 0x00,
		.bmAttributes			= USB_SELF_POWERED,
		.bMaxPower				= 250 /* 500mA */
};

struct interface_descriptor dev_interface_desc = {
		.bLength				= sizeof(struct interface_descriptor),
		.bDescriptorType		= DESC_TYPE_INTERFACE,
		.bInterfaceNumber		= 0x00,
		.bAlternateSetting		= 0x00,
		.bNumEndpoints			= 0x02,
		.bInterfaceClass		= 0x00,
		.bInterfaceSubClass		= 0x00,
		.bInterfaceProtocol		= 0x00,
		.iInterface				= 0x00
};

struct endpoint_descriptor dev_ep1_in_desc = {
		.bLength				= sizeof(struct endpoint_descriptor),
		.bDescriptorType		= DESC_TYPE_ENDPOINT,
		.bEndpointAddress		= (0x01 & EPT_ADDRESS_MASK) | EPT_DIR_IN,
		.bmAttributes			= EPT_XFER_INT,
		.wMaxPacketSize			= 64,
		.bInterval				= 16
};

struct endpoint_descriptor dev_ep1_out_desc = {
		.bLength				= sizeof(struct endpoint_descriptor),
		.bDescriptorType		= DESC_TYPE_ENDPOINT,
		.bEndpointAddress		= (0x01 & EPT_ADDRESS_MASK) | EPT_DIR_OUT,
		.bmAttributes			= EPT_XFER_INT,
		.wMaxPacketSize			= 64,
		.bInterval				= 16
};

struct string_descriptor string_desc = {
		.bLength = sizeof (string_desc),
		.bDescriptorType = DESC_TYPE_STRING,
		.wLANGID[0] = LANGID_ENGLISH
};

void usb_configure_ept(unsigned char epnum , unsigned int fifo_sz)
{
	epnum &= 0x0F; /* mask out reserved bits */

	if (epnum != 0) {
		/* write ep number to EPIDX */
		write32(USB0_BASE + USB_O_EPIDX, epnum);
		/* write fifo start address */
		write16(USB0_BASE + USB_O_TXFIFOADD, ~USB_TXFIFOADD_ADDR_M); // zero out address
		write16(USB0_BASE + USB_O_TXFIFOADD, (8 & USB_TXFIFOADD_ADDR_M));
		write8(USB0_BASE + USB_O_TXFIFOSZ, USB_TXFIFOSZ_SIZE_64);
		write16(USB0_BASE + USB_O_TXMAXP1, (USB_TXMAXP1_MAXLOAD_M & 64));

		write16(USB0_BASE + USB_O_RXFIFOADD, ~USB_RXFIFOADD_ADDR_M); // zero out address
		write16(USB0_BASE + USB_O_RXFIFOADD, (16 & USB_RXFIFOADD_ADDR_M));
		write8(USB0_BASE + USB_O_RXFIFOSZ, USB_RXFIFOSZ_SIZE_64);
		write16(USB0_BASE + USB_O_RXMAXP1, (USB_RXMAXP1_MAXLOAD_M & 64));
	}
}

void usb_stall_ep0(void)
{
	*reg8(USB0_BASE + USB_O_CSRL0) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
	//usb_dev.ep0_state = STATE_STALL;
}

static void set_address_handler(short address)
{
	usb_dev.dev_addr = address;
	/* raise the flag for setting device address */
	usb_dev.flags |= USB_SET_DEVICE_ADDRESS;
	/* reset RX ready, mark data end as no data phase */
	*reg8(USB0_BASE + USB_O_CSRL0) = (USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);

	usb_dev.ep0_state = STATE_STATUS;
}

static void usb_get_status_handler(unsigned char type)
{
	while (1);
}

static void usb_tx(int bytes)
{
	int to_send;
	unsigned char val8;
	int i;

	if (bytes > 0) {
		to_send = bytes;
	} else {
		to_send = usb_dev.ep0_tx_remaining;
	}

	to_send = to_send < EP0_SIZE ? to_send : EP0_SIZE;

	usb_dev.ep0_state = STATE_TX;

	for (i = 0; to_send > 0; i++, to_send--) {
		write8(USB0_BASE + USB_O_FIFO0, *usb_dev.ep0_tx_data);
		usb_dev.ep0_tx_data++;
		usb_dev.ep0_tx_remaining--;
	}

	val8 = read8(USB0_BASE + USB_O_CSRL0);
	if (val8 & USB_CSRL0_TXRDY) {
		while (1);
	}

	if (to_send == EP0_SIZE) {
		/* more data to come, do not set dataend */
		val8 |= (USB_CSRL0_TXRDY);
	} else {
		/* no more data to come */
		val8 |= (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
		usb_dev.ep0_state = STATE_STATUS;
	}

	write8(USB0_BASE + USB_O_CSRL0, val8);
}

static char *usb_send_config_desc(unsigned int *size)
{
	unsigned int config_size;

	config_size = sizeof(dev_config);

	memcpy(&dev_config.config_desc, &dev_config_desc, sizeof(struct config_descriptor));
	memcpy(&dev_config.interface_desc, &dev_interface_desc, sizeof(struct interface_descriptor));
	memcpy(&dev_config.ep1_in_desc, &dev_ep1_in_desc, sizeof(struct endpoint_descriptor));
	memcpy(&dev_config.ep1_out_desc, &dev_ep1_out_desc, sizeof(struct endpoint_descriptor));

	*size = config_size;

	return (char *)&dev_config;
}

static char *usb_send_string_desc(struct setup_packet *pkt, unsigned int *length)
{
	char *data = NULL;

	/* string descriptor index is in wValue low byte */
	switch (pkt->wValue & 0x00FF) {
	case 0:
		desc_index = 0;
		*length = sizeof(string_desc);
		data = (char *)&string_desc;
		break;
	case STRING_MANUFACTURER_IDX:
		desc_index = 1;
		*length = manufacturer_string[0];
		data = manufacturer_string;
		break;
	case STRING_PRODUCT_IDX:
		desc_index = 2;
		*length = product_string[0];
		data = product_string;
		break;
	case STRING_SERIAL_IDX:
		desc_index = 3;
		*length = serial_string[0];
		data = serial_string;
		break;
	}

	return data;
}

static void usb_get_desc_handler(struct setup_packet *request)
{
	char *tx_data;
	unsigned int length;
	unsigned char val8;

	/* ack the reception of request */
	val8 = read8(USB0_BASE + USB_O_CSRL0);
	val8 |= USB_CSRL0_RXRDYC;
	write8(USB0_BASE + USB_O_CSRL0, val8);

	/*
	 * Descriptor Type (H) and Descriptor Index (L)
	 */
	switch (request->wValue >> 8) {
	/* check descriptor type */
	case DESC_TYPE_DEVICE:
		tx_data = (char *)&dev_desc;
		length = dev_desc.bLength; // descriptor length
		break;
	case DESC_TYPE_CONFIG:
		tx_data = usb_send_config_desc(&length);
		break;
	case DESC_TYPE_STRING:
		tx_data = usb_send_string_desc(request, &length);
		break;
	case DESC_TYPE_INTERFACE:
		break;
	case DESC_TYPE_ENDPOINT:
		break;
	case DESC_TYPE_DEV_QUALIFIER:
		tx_data = (char *)&dev_qualifier;
		length = dev_qualifier.bLength;
		break;
	default:
		usb_stall_ep0();
		return;
	}

	usb_dev.ep0_tx_data = tx_data;
	usb_dev.ep0_tx_remaining = length;

	/* if requested data is less than actual descriptor size,
	 * just send the data size requested
	 */
	if (request->wLength < length) {
		usb_tx(request->wLength);
	} else {
		usb_tx(0);
	}
}

static void usb_set_configuration(struct setup_packet *req) {

	unsigned char config_value;

	/* lower byte contains configuration value */
	config_value = req->wValue & 0x00FF;
	/* In this example there is only one configuration which
	 * is active by default
	 */

	*reg8(USB0_BASE + USB_O_CSRL0) = (USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
	usb_dev.ep0_state = STATE_STATUS;

	usb_configure_ept(1, 64);
	usb_dev.flags |= USB_DEVICE_CONFIGURED;

}

static void usb_clear_feature_handler(unsigned char type)
{
	while (1);
}

static void usb_parse_setup_pkt(struct setup_packet *pkt)
{
	//if (pkt->bmRequestType == 0x00) {
	switch (pkt->bRequest) {
	/* handle USB device request */
	case GET_STATUS:
		usb_get_status_handler(pkt->bmRequestType);
		break;
	case CLEAR_FEATURE:
		usb_clear_feature_handler(pkt->wValue);
		break;
	case SET_FEATURE:
		break;
	case SET_ADDRESS:
		set_address_handler(pkt->wValue);
		break;
	case GET_DESCRIPTOR:
		usb_get_desc_handler(pkt);
		break;
	case SET_DESCRIPTOR:
		break;
	case GET_CONFIGURATION:
		break;
	case SET_CONFIGURATION:
		usb_set_configuration(pkt);
		break;
	default:
		while (1);
		;
	}
}

ep0_get_data(unsigned char *buffer, unsigned long *size) {

	unsigned char count;
	int i;

	if (!(*reg8(USB0_BASE + USB_O_CSRL0) & USB_CSRL0_RXRDY)) {
			/* no data packet received */
			*size = 0;
			return;
	}
	count = read8(USB0_BASE + USB_O_COUNT0);
	*size = count;

	for (i = 0; i < count; i++) {
			buffer[i] = read8(USB0_BASE + USB_O_FIFO0);
	}
}

usb_read_request(void)
{

	unsigned char buffer[EP0_SIZE];

	struct setup_packet *setup;
	unsigned long size;

	setup = (struct setup_packet *)buffer;
	size = EP0_SIZE;

	ep0_get_data(buffer, &size);

	if (!size)
		return;

	usb_parse_setup_pkt(setup);
}

void usb_setup_handler(void)
{
	volatile unsigned char bytes_available;
	volatile unsigned long val32;
	unsigned volatile char val8, status, power;


	status = read8(USB0_BASE + USB_O_CSRL0);
	power = read8(USB0_BASE + USB_O_POWER);

	/* check if controller is suspended */
	if (power & USB_POWER_SUSPEND) {
		/* stall ep0 */
		//return;
	}
	/* check if endpoint is stalled */
	if (status & USB_CSRL0_STALLED) {
		usb_dev.ep0_state = STATE_IDLE;
		/* clear STALL state */
		*reg8(USB0_BASE + USB_O_CSRL0) &= ~USB_CSRL0_STALLED;
	}
	/* check if setup ended prematurely */
	if (status & USB_CSRL0_ERROR) {
		usb_dev.ep0_state = STATE_IDLE;
		*reg8(USB0_BASE + USB_O_CSRL0) |= USB_CSRL0_SETENDC;
		val8 = read8(USB0_BASE + USB_O_CSRL0);
	}

	switch (usb_dev.ep0_state) {

	case STATE_IDLE:
		if (status & REQUEST_IN) {
			usb_read_request();
		}
		break;
	case STATE_STATUS: /* indicate the status stage */
		/* set the state to idle again */
		usb_dev.ep0_state = STATE_IDLE;
		/* check if address set is pending */
		if (usb_dev.flags & USB_SET_DEVICE_ADDRESS) {
			/* set device address */
			write8(USB0_BASE + USB_O_FADDR, (char)usb_dev.dev_addr);
			/* de-assert address set flag*/
			usb_dev.flags &= ~USB_SET_DEVICE_ADDRESS;
		}
		break;
	case STATE_RX:
		break;
	case STATE_TX:
		usb_tx(0);
		break;
	default:
		while (1);
	}
}

/* handle being set in suspended state */
static void usb_suspend_handler(void)
{
	/* configure the device in suspend state
	 * like current consumption etc
	 */
	unsigned long val32;
	/* enable resume interrupt through other interface */
	val32 = read32(USB0_BASE + USB_O_DRIM);
	val32 |= USB_DRIM_RESUME;
	write32(USB0_BASE + USB_O_DRIM, val32);
}

/* handle resume signal from host */
static void usb_resume_handler(void)
{
	/* TODO */
	unsigned long val32;

	/* check if resume interrupt asserted */
	val32 = read32(USB0_BASE + USB_O_DRISC);
	if (val32 & USB_DRISC_RESUME) {
		val32 |= USB_DRISC_RESUME;
		write32(USB0_BASE + USB_O_DRISC, val32);
		//resumes += 1;
	}
}

/* Keep resume signal asserted for 15ms by device
 * controller
 */
usb_remote_wakeup_delay()
{
	volatile unsigned val8;

	if (usb_dev.flags & USB_REMOTE_WAKEUP_PENDING) {
		usb_dev.resume_ticks_1ms++;
		/* check if 15ms elapsed */
		if (usb_dev.resume_ticks_1ms == 15)
			/* de-assert resume signal */
			val8 = read8(USB0_BASE +USB_O_POWER);
			val8 &= ~USB_POWER_RESUME;
			write8(USB0_BASE +USB_O_POWER, val8);
			/* de-assert remote wake-up flag */
			usb_dev.flags &= ~USB_REMOTE_WAKEUP_PENDING;
	}
}

/* assert remote wake-up */
static void usb_remote_wakeup(void)
{
	unsigned char val8;

	usb_dev.flags |= USB_REMOTE_WAKEUP_PENDING;
	usb_dev.resume_ticks_1ms = 0;
	/* assert resume signal */
	val8 = read32(USB0_BASE +USB_O_POWER);
	val8 |= USB_POWER_RESUME;
	write8(USB0_BASE + USB_O_POWER, val8);
	val8 &= ~USB_POWER_RESUME;
	write8(USB0_BASE + USB_O_POWER, val8);
}

/* SOF handler: called every 1ms */
static void usb_sof_handler(void)
{
	/* use sof ticks to watch time for resume
	 *  signal duration asserted by device controller
	 *  (Remote wake-up functionality)
	 */
	//usb_remote_wakeup_delay();
}

/* handle reset signal from host */
static void usb_reset_handler(void)
{
	/* TODo: Implement usb reset  */
	usb_dev.flags &= ~USB_DEVICE_CONFIGURED;
	usb_dev.ep0_state = STATE_IDLE;
}

static void usb_ep1_tx_handler(void)
{
	tx_done = 1;
}

static void usb_ep1_rx_handler(void)
{
	unsigned short count;
	int i;

	count = read16(USB0_BASE + USB_O_RXCOUNT1);

	if (count == 0)
		return;
	for (i = 0; i < count; i++) {
		ep1_data[i] = read8(USB0_BASE + USB_O_FIFO1);
	}
	/* flush fifo */
	*reg8(USB0_BASE + USB_O_RXCSRL1) |= USB_RXCSRL1_FLUSH;
	/* clear RXRDY */
	*reg8(USB0_BASE + USB_O_RXCSRL1) |= USB_RXCSRL1_RXRDY;

	rx_done = 1;

}

void usb_interrupt_handler(void)
{
	unsigned short val16;
	volatile unsigned long val32;


	/* read USB general interrupt status */
	val32 = read32(USB0_BASE + USB_O_IS);
	/* check sof interrupt status */
	if (val32 & USB_IS_SOF) {
		usb_sof_handler();
	}
	/* handle being reset by host */
	if (val32 & USB_IS_RESET) {
		usb_reset_handler();
	}
	/* check if suspend interrupt asserted */
	if (val32 & USB_IS_SUSPEND) {
		usb_suspend_handler();
	}
	/* check if resume interrupt asserted */
	val32 = read32(USB0_BASE + USB_O_DRISC);
	if (val32 & USB_DRISC_RESUME) {
		usb_resume_handler();
	}

	/* check type of tx interrupt */
	val16 = read16(USB0_BASE + USB_O_TXIS);
	/* ignore the MSB (reserved bits) */
	val16 &= 0x00FF;

	/* check for ep0 interrupt */
	if (val16 & USB_TXIS_EP0) {
		usb_setup_handler();
	}
	if (val16 & USB_TXIS_EP1) {
		usb_ep1_tx_handler();
	}
	/* check type of rx interrupt */
	val16 = read16(USB0_BASE + USB_O_RXIS);
	/* ignore the MSB (reserved bits) */
	val16 &= 0x00FF;
	/* check for ep1 interrupt */
	if (val16 & USB_TXIS_EP1) {
		usb_ep1_rx_handler();
	}
}

/* handler for device-to-host transaction complete event */
void tx_complete(void)
{

}

static inline void usb_connect(void)
{
	unsigned long temp;
	/* set SOFTCONN bit */
	temp = read32(USB0_BASE + USB_O_POWER);
	temp |= (USB_POWER_SOFTCONN);
	write32(USB0_BASE + USB_O_POWER, temp);
}

static inline void usb_disconnect(void)
{
	unsigned long temp;
	/* reset SOFTCONN bit */
	temp = read32(USB0_BASE + USB_O_POWER);
	temp &= ~(USB_POWER_SOFTCONN);
	write32(USB0_BASE + USB_O_POWER, temp);
}

void usb_enable_interrupts(void)
{
	unsigned short val16;
	unsigned long val32;

	/* enable sof, reset, suspend interrupts */
	val32 = read32(USB0_BASE + USB_O_IE);
	val32 |= (USB_IE_SOF | USB_IE_RESET | USB_IE_SUSPND);
	write32(USB0_BASE + USB_O_IE, val32);
	/* enable ep0 and ep1 interrupts */
	val16 = read16(USB0_BASE + USB_O_TXIE);
	val16 |= USB_TXIE_EP0 | USB_TXIE_EP1;
	write16(USB0_BASE +  USB_O_TXIE, val16);

	/* enable USB interrupt: irq 44 */
	val32 = read32(NVIC_EN1);
	val32 |= (1 << 12);
	write32(NVIC_EN1, val32);

}

static void inline usb_device_mode(void)
{
	unsigned long temp;
	char delay;

	/* Before changing mode, reset USB controller */
	temp = read32(SYSCTL_SRCR2);
	temp |= SYSCTL_SRCR2_USB0;
	write32(SYSCTL_SRCR2, temp);
	/* add short delay */
	for(delay = 0; delay < 16; delay++) {
		__asm("    nop");/* delay loop */
	}
	/* de-assert USB controller reset */
	temp = read32(SYSCTL_SRCR2);
	temp &= ~SYSCTL_SRCR2_USB0;
	write32(SYSCTL_SRCR2, temp);
	/* Enable USB controller clock */
	temp = read32(SYSCTL_RCGCUSB);
	temp |= SYSCTL_RCGCUSB_R0;
	write32(SYSCTL_RCGCUSB, temp);
	/* Enable clock for USB PHY (USB PLL) */
	/* setup USB PHY clock */
	temp = read32(SYSCTL_RCC);
	/* set osc source to main oscillator */
	temp &= ~(SYSCTL_RCC_OSCSRC_M); // zero out osc bits
	temp |= SYSCTL_RCC_OSCSRC_MAIN;
	/* enable PLL */
	temp &= ~SYSCTL_RCC_PWRDN; //zero out
	/* disable bypass */
	temp &= ~SYSCTL_RCC_BYPASS;
	/* enable sysclk divider */
	temp |= SYSCTL_RCC_USESYSDIV;
	/* select syclk divisor */
	temp &= ~SYSCTL_RCC_SYSDIV_M; //zero out
	temp |= (0x3 << SYSCTL_RCC_SYSDIV_S);
	/* select xtal source 16MHz */
	temp &= ~(0x1f << SYSCTL_RCC_XTAL_S); //zero out xtal field
	temp |= SYSCTL_RCC_XTAL_16MHZ;
	write32(SYSCTL_RCC, temp);
	/* configure RCC2 */
	temp = read32(SYSCTL_RCC2);
	/* disable RCC2 overriding RCC */
	temp &= ~SYSCTL_RCC2_USERCC2;
	temp &= ~SYSCTL_RCC2_DIV400;
	temp &= ~SYSCTL_RCC2_SYSDIV2_M;
	temp |= (0x3 << SYSCTL_RCC2_SYSDIV2_S);
	temp &= ~SYSCTL_RCC2_USBPWRDN;
	temp &= ~SYSCTL_RCC2_PWRDN2;
	temp &= ~SYSCTL_RCC2_BYPASS2;
	temp &= ~SYSCTL_RCC2_OSCSRC2_M;
	temp |= SYSCTL_RCC2_OSCSRC2_MO;
	write32(SYSCTL_RCC2, temp);

	//temp = read32(SYSCTL_RCC);
	/* use RCC2 */
	//temp |= SYSCTL_RCC2_USERCC2;
	//temp &= ~(SYSCTL_RCC_PWRDN);
	//write32(SYSCTL_RCC, temp);
	/* Fix the USBVBUS and USBID pins to device mode values */
	temp = read32(USB0_BASE + USB_O_GPCS);
	temp |= USB_GPCS_DEVMOD_DEV;
	write32(USB0_BASE + USB_O_GPCS, temp);
	/* Clear all pending USB interrupts */
	temp = read8(USB0_BASE + USB_O_IS);
	/* check and clear power fault interrupt */
	if (read32(USB0_BASE + USB_O_EPCISC) & USB_EPCISC_PF) {
		/* power fault interrupt raised */
		/* clear the PF interrupt */
		write32(USB0_BASE + USB_O_EPCISC, USB_EPCISC_PF);
	}
	if (read32(USB0_BASE + USB_O_IDVISC) & USB_IDVRIS_ID) {
		write32(USB0_BASE + USB_O_IDVISC, USB_IDVRIS_ID);
	}

	temp = read16(USB0_BASE + USB_O_TXIS);
	temp = read16(USB0_BASE + USB_O_RXIS);
	/* Verify that USB controller is in device mode */
	temp = read32(USB0_BASE + USB_O_DEVCTL);

	//usb_set_string_desc(&string_desc);
	//usb_set_config_desc(&config_desc);

	usb_dev.ep0_state = STATE_IDLE;

}

int main(void) {

	unsigned long temp;
	unsigned char val8;
	int i;

	/* system clock is set at 50MHz */
	/* Main oscillator driving  PLL
	 * PLL -> /2 PRE SCALAR ->  SYSDIV -> SYSTEM CLOCK
	 * PLL -> 400MHz, after prescaling -> 200MHz
	 * SYSDIV = 3 (/4 scale)
	 * 200MHz /4 = 50MHz
	 */

	/* enable USB data pins
	 * USB0DM - PD4
	 * USB0DP - PD5
	 */

	/* enable Port D */
	temp = read32(SYSCTL_RCGCGPIO);
	temp |= SYSCTL_RCGCGPIO_R3;
	write32(SYSCTL_RCGCGPIO, temp);
	/* associate GPIO pins to USB peripheral
	 * by selecting analog function on them */
	temp = read32(GPIO_PORTD_BASE + GPIO_O_AMSEL);
	temp |= ((1 << 4) | (1 << 5));
	write32(GPIO_PORTD_BASE + GPIO_O_AMSEL, temp);

	usb_device_mode();
	//usb_disconnect();
	//usb_remote_wakeup(); /* resume device controller */
	usb_enable_interrupts();
	usb_connect();

	while (1) {

		while(!usb_dev.flags & USB_DEVICE_CONFIGURED)
			; /* wait for configuration to be complete */

		while(!rx_done)
			; /* wait for data to be received first */

		rx_done = 0;
		/* flush TX fifo */
		*reg8(USB0_BASE + USB_O_TXCSRL1) |= USB_TXCSRL1_FLUSH;
		/* enable autoset of TXRDY */
		*reg8(USB0_BASE + USB_O_TXCSRH1) |= USB_TXCSRH1_AUTOSET;
		tx_done = 0;

		for (i = 0; i < 64; i++) {
			write8(USB0_BASE + USB_O_FIFO1, ep1_tx_data[i]);
		}
		/* set Tx ready */
		val8 = read8(USB0_BASE + USB_O_TXCSRL1);
		val8 |= USB_TXCSRL1_TXRDY;
		write8(USB0_BASE + USB_O_TXCSRL1, val8);

		while (!tx_done)
			;/* wait for tx to complete */
	}
}
