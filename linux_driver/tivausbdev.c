#include <linux/init.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>

/* device specific defines */
#define PRODUCT_ID	0x11bb
#define VENDOR_ID	0xccbb
/* set interval for interrupt urb in milliseconds */
#define INT_URB_INTERVAL	16

struct tivausbdev_dev {
	struct usb_device *usb_dev; /* usb device descriptor */
	wait_queue_head_t read_wq;
	int rx_done;
	int tx_done;
};

struct tivausbdev_dev *tivausbdev;

void tx_complete(struct urb *req)
{
	struct tivausbdev_dev *dev;
	printk("Data sent \r\n");

	dev = (struct tivausbdev_dev*)req->context;
	dev->tx_done = 1;
}

void rx_complete(struct urb *req)
{
	struct tivausbdev_dev *dev;

	/* check URB status */
	printk("Data received \r\n");
	dev = (struct tivausbdev_dev*)req->context;

	dev->rx_done = 1;
}

int tivausbdev_open(struct inode *i, struct file *f)
{
	return 0;
}

int tivausbdev_close(struct inode *i, struct file *f)
{
	return 0;
}

ssize_t tivausbdev_read(struct file *f, char __user *buf, size_t len, loff_t *offset)
{
	struct urb *rx_req;
	struct urb *tx_req;
	char *rx_buf;
	char *tx_buf;
	int ret;

	int i;

	tx_buf = kzalloc(64, GFP_KERNEL);
	if (!tx_buf) {
		printk("Unable to create TX buffer \r\n");
		return -ENOMEM;		
	}

	for (i = 0; i < 64; i++) {
		tx_buf[i] = i;
	}

	tx_req = usb_alloc_urb(0, GFP_KERNEL);
	if (!tx_req) {
		printk(KERN_ERR "Failed to create URB for Tx \r\n");
		return -ENOMEM;
	}
	
	tivausbdev->tx_done = 0;

        /* fill up urb using helper function for interrupt endpoint */
        usb_fill_int_urb(tx_req, tivausbdev->usb_dev, usb_sndintpipe(tivausbdev->usb_dev, 1), 
                        tx_buf, 64, tx_complete, tivausbdev, INT_URB_INTERVAL);

        ret = usb_submit_urb(tx_req, GFP_KERNEL);
        if(ret < 0) {
                printk("Failed to submit Tx URB. Ret: %d \r\n", ret);
                return ret;
        }

        /*wait the request complete, if something wrong, just dequeue it*/
        ret = wait_event_timeout(tivausbdev->read_wq, tivausbdev->tx_done, 20);
        if (ret <= 0) {
                printk("Timed-out sending data \r\n");
                return -EIO;
        }

	rx_buf = kzalloc(64, GFP_KERNEL);
	if (!rx_buf) {
		printk(KERN_ERR "Unable to create RX buffer \r\n");
		return -ENOMEM;
	}

	rx_req = usb_alloc_urb(0, GFP_KERNEL);
	if (!rx_req) {
		printk(KERN_ERR "Failed to create URB for Rx \r\n");
		return -ENOMEM;
	}

	tivausbdev->rx_done = 0;

	/* fill up urb using helper function for interrupt endpoint */
	usb_fill_int_urb(rx_req, tivausbdev->usb_dev, usb_rcvintpipe(tivausbdev->usb_dev, 1), 
			rx_buf, 64, rx_complete, tivausbdev, INT_URB_INTERVAL);

	ret = usb_submit_urb(rx_req, GFP_KERNEL);
	if(ret < 0) {
		printk("Failed to submit Rx URB. Ret: %d \r\n", ret);
		return ret;
	}

	/*wait the request complete, if something wrong, just dequeue it*/
        ret = wait_event_timeout(tivausbdev->read_wq, tivausbdev->rx_done, 20);
	if (ret <= 0) {
		printk("Timed-out waiting for data \r\n");
		return -EIO;
	}

	/* use copy_to_user to return USB data from kernel to userspace */
	copy_to_user(buf, rx_buf, 64);
	
	return 64;

}

ssize_t tivausbdev_write(struct file *f, const char __user *buf, size_t len, loff_t *offset)
{
	/* use copy_from_user to copy data from userspace and send over USB */
}

static const struct file_operations tivausbdev_fops = {
	.open = tivausbdev_open,
	.release = tivausbdev_close,
	.read = tivausbdev_read,
	.write = tivausbdev_write,
};

static struct usb_class_driver tivausbdev_class = {
	.name = "tivausbdev_class",
	.fops = &tivausbdev_fops
};

static struct usb_device_id tivausbdev_id[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{} /* terminating entry */
};

MODULE_DEVICE_TABLE(usb, tivausbdev_id);

int tivausbdev_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_host_interface *host_interface = interface->cur_altsetting;
	struct usb_device device;
 
	printk("Tiva USB device detected: VendorID: %x ProductID: %x \r\n", id->idVendor, id->idProduct);
	tivausbdev->usb_dev = interface_to_usbdev(interface);

	/* register usb class driver which uses USB MAJOR number */
	usb_register_dev(interface, &tivausbdev_class);

	return 0;
}

void tivausbdev_disconnect(struct usb_interface *interface)
{
	printk("Tiva USB device disconnected\r\n");
	usb_deregister_dev(interface, &tivausbdev_class);
}

static struct usb_driver tivausbdev_driver = {
	.name 		= "tivausbdev_driver",
	.id_table 	= tivausbdev_id,
	.probe 		= tivausbdev_probe,
	.disconnect 	= tivausbdev_disconnect
	
};

static int __init tivausbdev_init(void)
{
	struct tivausbdev_dev *device;

	printk("Init called for Tiva USB driver\r\n");

	device = kzalloc(sizeof(*device), GFP_KERNEL);
	if (!device) {
		printk(KERN_ERR "Failed to initialize device: no mem \r\n");
		return -ENOMEM;
	}
	/* initialize waitqueue heads */
	init_waitqueue_head(&device->read_wq);
	device->rx_done = 0;

	tivausbdev = device;

	return usb_register(&tivausbdev_driver);
}

static void __exit tivausbdev_exit(void)
{
	printk("Unloading Tiva USB device driver\r\n");
	usb_deregister(&tivausbdev_driver);
}
;

module_init(tivausbdev_init);
module_exit(tivausbdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yasir Khan <yasir_electronics@yahoo.com>");
MODULE_DESCRIPTION("A USB driver for custom Tiva C TM4C123G based USB device");
