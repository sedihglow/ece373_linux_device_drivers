/*
 * PJ Waskiewicz
 * 5/11/2020
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>

#define DEVNAME "create_device"

/* Our private driver struct */
struct create_device {
        dev_t create_device_node;
        struct cdev create_device_cdev;
};

/* Class for our auto-generated /dev entry */
static struct class *create_device_driver_class;
static struct create_device create_device_driver;

int create_device_open(struct inode *inode, struct file *file);

/* File operations for our system calls */
static struct file_operations create_device_driver_fops = {
	.owner = THIS_MODULE,
	.open = create_device_open,
};

int create_device_open(struct inode *inode, struct file *file)
{
	pr_info("made it to open...yahoo\n");

	return 0;
}

static int __init create_device_driver_init(void)
{
	int err;

	/* chardev creation and setup */
	err = alloc_chrdev_region(&create_device_driver.create_device_node, 0, 1, DEVNAME);
	if (err) {
		pr_err("alloc_chrdev_region() failed: %d\n", err);
		goto err_alloc_chardev;
	}

	cdev_init(&create_device_driver.create_device_cdev, &create_device_driver_fops);
	create_device_driver.create_device_cdev.owner = THIS_MODULE;
	err = cdev_add(&create_device_driver.create_device_cdev, create_device_driver.create_device_node, 1);
	if (err) {
		pr_err("cdev_init() failed: %d\n", err);
		goto err_cdev_init;
	}

	/* device class setup and device creation */
	create_device_driver_class = class_create(THIS_MODULE, "create_device");
	device_create(create_device_driver_class, NULL, create_device_driver.create_device_node,
	              NULL, DEVNAME);
	return 0;

err_cdev_init:
	unregister_chrdev_region(create_device_driver.create_device_node, 1);
err_alloc_chardev:
	return err;
}

static void __exit create_device_driver_exit(void)
{
	/* destroy the dev entry */
	device_destroy(create_device_driver_class, create_device_driver.create_device_node);
	class_destroy(create_device_driver_class);

	/* tear down the cdev */
	cdev_del(&create_device_driver.create_device_cdev);
	unregister_chrdev_region(create_device_driver.create_device_node, 1);
}

module_init(create_device_driver_init);
module_exit(create_device_driver_exit);

MODULE_AUTHOR("PJ Waskiewicz");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
