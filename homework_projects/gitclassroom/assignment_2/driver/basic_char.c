/*
 * ece373 assignment 2
 * basic character driver
 *
 * This module implements read() write() open() llseek() and
 * sends data back and forth between kerenel space and user space.
 *
 * written by: James Ross
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define MINOR_STRT 0 /* starting minor number */
#define DEVCNT 1
#define DEV_NAME "basic_char"

#define INT_BYTES (sizeof(int))

#define NO_WR 0 /* no write was done, 0 bytes written */

#define SUCCESS 0

static int basic_char_open(struct inode *inode, struct file *file);
static ssize_t basic_char_read(struct file *file, char __user *buf,
                               size_t len, loff_t *offset);
static ssize_t basic_char_write(struct file *file, const char __user *buf,
                                size_t len, loff_t *offset);

static struct mydev_dev {
	struct cdev my_cdev;
	struct class *myclass;
	struct device *mydevice;
	dev_t devnode;

	int rwdata; /* device data to be read and written to */
} mydev;

static struct file_operations mydev_fops = {
	.owner = THIS_MODULE,
	.open = basic_char_open,
	.read = basic_char_read,
	.write = basic_char_write,
	.llseek = default_llseek
};

/* this shows up under /sys/modules/example5/parameters */
static int data_init = 10;
module_param(data_init, int, S_IRUSR | S_IWUSR);

/*
 * open initialized rwdata with data_init
 *
 * returns: SUCCESS
 */
static int basic_char_open(struct inode *inode, struct file *file)
{
	pr_info(DEV_NAME ": Successfully opened.");

	mydev.rwdata = data_init;

	return SUCCESS;
}

/*
 * read data from the module to the user space handling the offset for the
 * module. data read is a int.
 *
 * returns: on success, returns the number of bytes read from the kernel data.
 *	    on failure -ERRNO is returned.
 */
static ssize_t basic_char_read(struct file *file, char __user *buf,
			       size_t len, loff_t *offset)
{
	ssize_t ret = 0;

	if (unlikely(!buf)) {
		ret = -EINVAL;
		pr_err(DEV_NAME ": NULL buffer passed to read(), error %zd\n",
		       ret);
		return ret;
	}

	return simple_read_from_buffer(buf, len, offset,
				       &mydev.rwdata, INT_BYTES);
}

/*
 * write data to the module and handle the offset. Data that is passed from the
 * user is treated like an integer and changes rwdata on a successful write to
 * the kernel buffer.
 *
 * returns: On success, returns the number of bytes written. If this value is
 *	    smaller than the desired write no data will be changed.
 *	    On failure -ERRNO is returned.
 */
static ssize_t basic_char_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	int *kbuf;
	ssize_t ret;

	if (unlikely(!buf)) {
		ret = -EINVAL;
		pr_err(DEV_NAME ": write failed, user buf was NULL. error %zd\n",
		       ret);
		goto out;
	}

	/*
	 * user trying to write more or less than the data type of the modules
	 * data.
	 */
	if (len != INT_BYTES) {
		ret = -EINVAL;
		pr_err(DEV_NAME ": write failed, len longer/shorter than module "
			        "data type. error %zd\n", ret);
		goto out;
	}

	/* offset is larger than the size of the module data */
	if (unlikely(*ppos > INT_BYTES)) {
		ret = -EFBIG;
		pr_err(DEV_NAME ": write failed, pos is too large. error %zd\n",
		       ret);
		goto out;
	}

	kbuf = kcalloc(len, sizeof(u8), GFP_KERNEL);
	if (unlikely(!kbuf)) {
		ret = -ENOMEM;
		pr_err(DEV_NAME ": write failed, could not kcalloc, error %zd\n",
		       ret);
		goto out;
	}

	ret = simple_write_to_buffer(kbuf, INT_BYTES, ppos, buf, len);
	if (unlikely(ret < NO_WR)) {
		pr_err(DEV_NAME ": write to buffer failed, error %zd\n", ret);
		goto simple_write_to_buffer_out;
	} else if (unlikely(ret < len)) { /* dont change rwdata with incomplete int data */
		pr_warn(DEV_NAME ": failed to write all bytes to buffer, ");
		pr_warn(DEV_NAME ": no module data was changed in write call\n");
	} else { /* read was complete */
		mydev.rwdata = *kbuf;
	}

	pr_info(DEV_NAME ": Module data currently set to %d\n", mydev.rwdata);

simple_write_to_buffer_out:
	kfree(kbuf);
out:
	return ret;
}

static int __init basic_char_init(void)
{
	int ret;

	pr_info(DEV_NAME ": module loading... data_init = %d\n", data_init);

	ret = alloc_chrdev_region(&mydev.devnode, MINOR_STRT, DEVCNT, DEV_NAME);
	if (unlikely(ret)) {
		pr_err(DEV_NAME ": alloc_chrdev_region() failed. error %d\n",
		       ret);
		goto out;
	}

	pr_info(DEV_NAME ": Allocated %d devices at major %d\n",
		DEVCNT, MAJOR(mydev.devnode));

	/* init the character device and add it to the kernel */
	cdev_init(&mydev.my_cdev, &mydev_fops);
	mydev.my_cdev.owner = THIS_MODULE;

	ret = cdev_add(&mydev.my_cdev, mydev.devnode, DEVCNT);
	if (unlikely(ret)) {
		pr_err(DEV_NAME ": cdev_add() failed. error %d\n", ret);
		goto cdev_add_out;
	}

	/* place module in /dev with class and device, requires a GPL license */
	mydev.myclass = class_create(THIS_MODULE, DEV_NAME);
	ret = IS_ERR(mydev.myclass);
	if (unlikely(ret)){
		pr_err(DEV_NAME ": class_create() failed. error %d\n", ret);
		goto class_create_out;
	}

	mydev.mydevice = device_create(mydev.myclass, NULL, mydev.devnode,
				       NULL, DEV_NAME);
	ret = IS_ERR(mydev.mydevice);
	if (unlikely(ret)) {
		pr_err(DEV_NAME ": device_create() failed. error %d\n", ret);
		goto device_create_out;
	}

	return SUCCESS;

device_create_out:
	class_destroy(mydev.myclass);
class_create_out:
	cdev_del(&mydev.my_cdev);
cdev_add_out:
	unregister_chrdev_region(mydev.devnode, DEVCNT);
out:
	return ret;
}

static void __exit basic_char_exit(void)
{
	pr_info(DEV_NAME ": cleaning up...\n");
	device_destroy(mydev.myclass, mydev.devnode);
	class_destroy(mydev.myclass);
	cdev_del(&mydev.my_cdev);
	unregister_chrdev_region(mydev.devnode, DEVCNT);
	pr_info(DEV_NAME ": exiting...\n");
}

MODULE_AUTHOR("James Ross");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");
module_init(basic_char_init);
module_exit(basic_char_exit);
