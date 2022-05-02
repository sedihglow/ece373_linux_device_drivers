/*
 * ece373 assignment 2
 *
 *
 * This module implements read() write() open() llseek() and
 * sends data back and forth between kerenel space and user space.
 *
 * this module implements probe() remove() and sets up for the pci device 82540EM
 * intel ethernet card.
 *
 * takes a bytes from the user and sets LEDs based on that.
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
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/delay.h>

#define SUCCESS 0
#define FAILURE -1

#define MINOR_STRT 0 /* starting minor number */
#define DEVCNT 1
#define DEV_NAME "intel_82540EM"

#define SIZEU32 (sizeof(u32))
#define SIZEU8  (sizeof(u8))

#define NO_WR 0 /* no write was done, 0 bytes written */

#define i82540EM_VEND_ID 0x8086
#define i82540EM_DEV_ID  0x100e

#define HWREG_U32(bar, offset) ((u32*)((bar)+(offset)))

#define MM_BAR0 0 /* Memory maps IO on the 82540EM is bar 0 */

/* device control registers and mask definitions */
#define DEV_CTRL     0x00000000 /* device control register offset */
#define CTRL_RST     0x04000000
#define CTRL_ASDE    0x00000020 /* auto speed detection enable */
#define CTRL_SLU     0x00000040 /* set link up */
#define CTRL_PHY_RST 0x80000000
#define CTRL_ILOS    0x00000080 /* invert loss of signal */
#define CTRL_VME     0x40000000 /* VLAN mode enable */

#define CTRL_ASDE_SLU ((CTRL_ASDE) | (CTRL_SLU))

#define RST_SLEEP_DELAY 400 /* delay after RST is signaled in ms */

/* Flow control register offsets */
#define FCAH  0x0002C
#define FCAL  0x00028
#define FCT   0x00030
#define FCTTV 0x00170
#define FLOW_REGS_INIT 0x0

/* LED register and mask definitions */
#define LEDCTRL 0x0E00 /* LED CTRL offset */

#define LED_ON    true
#define LED_OFF   false
#define BLINK_ON  true
#define BLINK_OFF false

#define LED0_NUM           0x0
#define LED0_MODE_ON       0x0000000E
#define LED0_IVRT          0x00000040
#define LED0_BLINK         0x00000080
#define LED0_MODE_OFF	   ((LED0_MODE_ON) | (LED0_IVRT))
#define LED0_MODE_BLINK_ON ((LED0_MODE_ON) | (LED0_BLINK))

#define LED1_NUM           0x1
#define LED1_MODE_ON       0x00000E00
#define LED1_IVRT          0x00004000
#define LED1_BLINK         0x00008000
#define LED1_MODE_OFF	   ((LED1_MODE_ON) | (LED1_IVRT))
#define LED1_MODE_BLINK_ON ((LED1_MODE_ON) | (LED1_BLINK))

#define LED2_NUM           0x2
#define LED2_MODE_ON       0x000E0000
#define LED2_IVRT          0x00400000
#define LED2_BLINK         0x00800000
#define LED2_MODE_OFF	   ((LED2_MODE_ON) | (LED2_IVRT))
#define LED2_MODE_BLINK_ON ((LED2_MODE_ON) | (LED2_BLINK))

#define LED3_NUM           0x3
#define LED3_MODE_ON       0x0E000000
#define LED3_IVRT	   0x40000000
#define LED3_BLINK	   0x80000000
#define LED3_MODE_OFF	   ((LED3_MODE_ON) | (LED3_IVRT))
#define LED3_MODE_BLINK_ON ((LED3_MODE_ON) | (LED3_BLINK))

#define ALL_LED_MODE_ON							       \
	((LED0_MODE_ON) | (LED1_MODE_ON) | (LED2_MODE_ON) | (LED3_MODE_ON))

#define ALL_LED_IVRT							       \
	((LED0_IVRT_ON) | (LED1_IVRT) | (LED2_IVRT) | (LED3_IVRT))

#define ALL_LED_BLINK						       \
	((LED0_BLINK_ON) | (LED1_BLINK) | (LED2_BLINK) | (LED3_BLINK))

#define ALL_LED_MODE_OFF						       \
	((LED0_MODE_OFF) | (LED1_MODE_OFF) | (LED2_MODE_OFF) | (LED3_MODE_OFF))

#define ALL_LED_MODE_BLINK_ON ((ALL_LED_MODE_ON) | (ALL_LED_BLINK))

/* led definitions that are used on data sent from user */
#define LED_USER_MODE_ON  0x1
#define LED_USER_BLINK_ON 0x2

#define UNPACK_LED0(packed) (((packed) << 6) >> 6)
#define UNPACK_LED1(packed) (((packed) << 4) >> 6)
#define UNPACK_LED2(packed) (((packed) << 2) >> 6)
#define UNPACK_LED3(packed) (((packed) << 0) >> 6)

			/* file ops */
static int i82540EM_open(struct inode *inode, struct file *file);
static ssize_t i82540EM_read(struct file *file, char __user *buf,
                               size_t len, loff_t *offset);
static ssize_t i82540EM_write(struct file *file, const char __user *buf,
                                size_t len, loff_t *offset);

			/* pci ops */
static int i82540EM_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void i82540EM_remove(struct pci_dev *pdev);

static struct i82540EM_dev {
	void __iomem *hwbar;
	struct cdev my_cdev;
	struct class *myclass;
	struct device *mydevice;
	dev_t devnode;

	int rwdata; /* device data to be read and written to */
} mydev;

static struct file_operations mydev_fops = {
	.owner  = THIS_MODULE,
	.open   = i82540EM_open,
	.read   = i82540EM_read,
	.write  = i82540EM_write,
	.llseek = default_llseek
};

static struct pci_device_id i82540EM_pci_ids[] = {
	{ PCI_DEVICE(i82540EM_VEND_ID, i82540EM_DEV_ID) },
	{0,0} /* terminate list */
};
MODULE_DEVICE_TABLE(pci, i82540EM_pci_ids);

static struct pci_driver i82540EM_pci_driver = {
	.name     = DEV_NAME,
	.id_table = i82540EM_pci_ids,
	.probe    = i82540EM_probe,
	.remove   = i82540EM_remove
};

/* struct for unpacked user led options */
struct led_options {
	u8 led0_opt;
	u8 led1_opt;
	u8 led2_opt;
	u8 led3_opt;
};

/* this shows up under /sys/modules/example5/parameters */
static int data_init = 10;
module_param(data_init, int, S_IRUSR | S_IWUSR);

/*
 * i82540EM_set_led - sets an individual led
 *
 * @lenum - Which number of led to change. LED[0-3]_NUM
 * @state - Which state to set. true for on false for off.
 * @blink - which state to set blink. true for on false for off
 *
 * returns success on completeion
 * returns -ERRNO on failure
 */
static int i82540EM_set_led(int lednum, bool state, bool blink)
{
	u32 __iomem *led_reg = HWREG_U32(mydev.hwbar, LEDCTRL);
	int ret;
	u32 regval;

	regval = ioread32(led_reg);
	switch (lednum) {
	case LED0_NUM:
		if (state) {
			if (blink) {
				regval |= LED0_MODE_BLINK_ON;
				iowrite32(regval, led_reg);
			} else {
				regval |= LED0_MODE_ON;
				iowrite32(regval, led_reg);
			}
		} else {
			regval |= LED0_MODE_OFF;
			iowrite32(regval, led_reg);
		}
		ret = SUCCESS;
	break;

	case LED1_NUM:
		if (state) {
			if (blink) {
				regval |= LED1_MODE_BLINK_ON;
				iowrite32(regval, led_reg);
			} else {
				regval |= LED1_MODE_ON;
				iowrite32(regval, led_reg);
			}
		} else {
			regval |= LED1_MODE_OFF;
			iowrite32(regval, led_reg);
		}
		ret = SUCCESS;
	break;

	case LED2_NUM:
		if (state) {
			if (blink) {
				regval |= LED2_MODE_BLINK_ON;
				iowrite32(regval, led_reg);
			} else {
				regval |= LED2_MODE_ON;
				iowrite32(regval, led_reg);
			}
		} else {
			regval |= LED2_MODE_OFF;
			iowrite32(regval, led_reg);
		}
		ret = SUCCESS;
	break;

	case LED3_NUM:
		if (state) {
			if (blink) {
				regval |= LED3_MODE_BLINK_ON;
				iowrite32(regval, led_reg);
			} else {
				regval |= LED3_MODE_ON;
				iowrite32(regval, led_reg);
			}
		} else {
			regval |= LED3_MODE_OFF;
			iowrite32(regval, led_reg);
		}
		ret = SUCCESS;
	break;

	default:
		ret = -EINVAL;
		pr_err(DEV_NAME
		       ": Invalid led number to i82540EM_set_led(). error %d\n", ret);
	}

	return ret;
}

/*
 * i82540EM_set_all_leds - sets all leds to the same state and blink state.
 *
 * @state - state to turn the leds to. true for on false for off.
 * @blink - state to turn blink to. true for on false for off.
 *
 * returns SUCCESS on success
 * returns -ERRNO error code on failure.
 */
static int i82540EM_set_all_leds(bool state, bool blink)
{
	int ret;

	/* below two declarations are for testing currently */
	u32 __iomem *ledreg = HWREG_U32(mydev.hwbar, LEDCTRL);
	u32 val = ioread32(ledreg);

	/*
	 * BUG CHECKING: regval returns a value of 0 for some reason when it
	 *		 should be its init value.
	 */
	pr_info(DEV_NAME ": ledreg val in set_all_leds start, %X\n", val);

	ret = i82540EM_set_led(LED0_NUM, state, blink);
	if (ret) {
		pr_err(DEV_NAME
		       ": LED0 failed to set, i82540EM_set_all_leds(), error %d\n", ret);
		return ret;
	}

	ret = i82540EM_set_led(LED1_NUM, state, blink);
	if (ret) {
		pr_err(DEV_NAME
		       ": LED1 failed to set, i82540EM_set_all_leds(), error %d\n", ret);
		return ret;
	}

	ret = i82540EM_set_led(LED2_NUM, state, blink);
	if (ret) {
		pr_err(DEV_NAME
		       ": LED2 failed to set, i82540EM_set_all_leds(), error %d\n", ret);
		return ret;
	}

	ret = i82540EM_set_led(LED3_NUM, state, blink);
	if (ret) {
		pr_err(DEV_NAME
		       ": LED3 failed to set, i82540EM_set_all_leds(), error %d\n", ret);
		return ret;
	}


	return ret;
}

/*
 * i82540EM_open - open initialized rwdata with data_init
 *
 * returns: SUCCESS
 */
static int i82540EM_open(struct inode *inode, struct file *file)
{
	pr_info(DEV_NAME ": Successfully opened.");

	mydev.rwdata = data_init;

	return SUCCESS;
}

/*
 * i82540EM_read - reads the LED register and returns its value
 *
 * returns: on success, returns the number of bytes read from the kernel data.
 *	    on failure -ERRNO is returned.
 */
static ssize_t i82540EM_read(struct file *file, char __user *buf,
			     size_t len, loff_t *offset)
{
	ssize_t ret = 0;
	u32 __iomem *ledreg = HWREG_U32(mydev.hwbar, LEDCTRL);
	u32 ledreg_val = ioread32(ledreg);

	if (unlikely(!buf)) {
		ret = -EINVAL;
		pr_err(DEV_NAME ": NULL buffer passed to read(), error %zu\n",
		       ret);
		goto err;
	}

	if (len != SIZEU32) {
		ret = -EINVAL;
		pr_err(DEV_NAME
		       ": Length of passed buffer not u32. error %zu\n", ret);
		goto err;
	}

	/*
	 * BUG: ledreg returns 0 when its suppoed to be the init value. Does not
	 * print out anything after copy_to_user. I do not think anything after
	 * gets called but the program does not crash.
	 *
	 */
	pr_info(DEV_NAME ": ledreg_val before copy. %X, len = %zu\n", ledreg_val, len);

	ret = copy_to_user(buf, &ledreg_val, len);
	if (ret == len) {
		ret = -EFAULT;
		pr_err(DEV_NAME
		       ": copy_to_user did not write len bytes, error %zu", ret);
		goto err;
	}

	/* BUG: DOES NOT PRINT when read() is called */
	pr_info(DEV_NAME ": led reg after read, %X", ioread32(ledreg));

	return len - ret;

err:
	return len - ret;
}

/*
 * i82540EM_create_led_reg_val - creates a u32 register value and returns it.
 * Register values are based on the led_options struct values.
 *
 * @reg_val - the initial value from the register to be altered based on
 *	      the led_opts.
 * @led_opts - a struct that holds flag values for which settings to set on
 *	       reg_val.
 *
 * returns: returns reg_val after its been altered based on led_opts
 */
static u32 i82540EM_create_led_reg_val(u32 reg_val,
				       struct led_options *led_opts)
{
	/* set reg_val for LED0 */
	if (led_opts->led0_opt & LED_USER_MODE_ON) {
		if (led_opts->led0_opt & LED_USER_BLINK_ON) {
			reg_val |= LED0_MODE_BLINK_ON;
		} else {
			reg_val |= LED0_MODE_ON;
			reg_val &= ~(LED0_BLINK);
		}
	} else {
		reg_val |= LED0_MODE_OFF;
		if (led_opts->led0_opt & LED_USER_BLINK_ON)
			reg_val |= LED0_BLINK;
		else
			reg_val |= ~(LED0_BLINK);
	} /* end led0 */

	/* set reg_val for LED1 */
	if (led_opts->led1_opt & LED_USER_MODE_ON) {
		if (led_opts->led1_opt & LED_USER_BLINK_ON) {
			reg_val |= LED1_MODE_BLINK_ON;
		} else {
			reg_val |= LED1_MODE_ON;
			reg_val &= ~(LED1_BLINK);
		}
	} else {
		reg_val |= LED1_MODE_OFF;
		if (led_opts->led1_opt & LED_USER_BLINK_ON)
			reg_val |= LED1_BLINK;
		else
			reg_val |= ~(LED1_BLINK);
	} /* end led1 */

	/* set reg_val for LED2 */
	if (led_opts->led2_opt & LED_USER_MODE_ON) {
		if (led_opts->led2_opt & LED_USER_BLINK_ON) {
			reg_val |= LED2_MODE_BLINK_ON;
		} else {
			reg_val |= LED2_MODE_ON;
			reg_val &= ~(LED2_BLINK);
		}
	} else {
		reg_val |= LED2_MODE_OFF;
		if (led_opts->led2_opt & LED_USER_BLINK_ON)
			reg_val |= LED2_BLINK;
		else
			reg_val |= ~(LED2_BLINK);
	} /* end led2 */

	/* set reg_val for LED3 */
	if (led_opts->led3_opt & LED_USER_MODE_ON) {
		if (led_opts->led3_opt & LED_USER_BLINK_ON) {
			reg_val |= LED3_MODE_BLINK_ON;
		} else {
			reg_val |= LED3_MODE_ON;
			reg_val &= ~(LED3_BLINK);
		}
	} else {
		reg_val |= LED3_MODE_OFF;
		if (led_opts->led3_opt & LED_USER_BLINK_ON)
			reg_val |= LED3_BLINK;
		else
			reg_val |= ~(LED3_BLINK);
	} /* end led3 */

	return reg_val;
}

/*
 * i82540EM_write - writes to the led register based on a packed u8 passed
 *		    from the user that represents the LEDS state for on/off and
 *	            blink. The u8 holds 2 bits per LED. This is unpacked and
 *	            then used to build a u32 to write to the LED register.
 *
 * returns: On success, returns the number of bytes written. If this value is
 *	    not a byte, no data is written.
 *	    On failure -ERRNO is returned.
 */
static ssize_t i82540EM_write(struct file *file, const char __user *buf,
			      size_t len, loff_t *ppos)
{
	u32 __iomem *led_reg;
	u32 reg_val;
	ssize_t ret;
	struct led_options led_opts; /* unpacked user led options */
	u8 *kbuf;

	if (unlikely(!buf)) {
		ret = -EINVAL;
		pr_err(DEV_NAME ": write failed, user buf was NULL. error %zd\n",
		       ret);
		goto err;
	}

	/*
	 * user trying to write more or less than the data type of the modules
	 * data.
	 */
	if (unlikely(len != SIZEU8)) {
		ret = -EINVAL;
		pr_err(DEV_NAME ": write failed, len longer/shorter than module "
			        "data type. error %zd\n", ret);
		goto err;
	}

	kbuf = kcalloc(len, SIZEU8, GFP_KERNEL);
	if (unlikely(!kbuf)) {
		ret = -ENOMEM;
		pr_err(DEV_NAME ": write failed could not kcalloc, error %zd\n",
		       ret);
		goto err;
	}

	/* get data from user buff */
	ret = copy_from_user(kbuf, buf, len);
	if (unlikely(ret)) {
		pr_warn(DEV_NAME ": Not all bytes written to kbuf in write.\n");
		goto err_copy_from_user;
	}

	/* unpack data */
	led_opts.led0_opt = UNPACK_LED0(*kbuf);
	led_opts.led1_opt = UNPACK_LED1(*kbuf);
	led_opts.led2_opt = UNPACK_LED2(*kbuf);
	led_opts.led3_opt = UNPACK_LED3(*kbuf);

	/* read led reg to get current values */
	led_reg = HWREG_U32(mydev.hwbar, LEDCTRL);
	reg_val = ioread32(led_reg);

	/*
	 * set new led reg value variable to desired state based on unpacked
	 * data
	 */
	reg_val = i82540EM_create_led_reg_val(reg_val, &led_opts);

	/* write to led reg */
	iowrite32(reg_val, led_reg);

	/* read back led reg and print to check results */
	reg_val = ioread32(led_reg);

	pr_info(DEV_NAME ": new LEDCTRL register value - %X\n", reg_val);

	kfree(kbuf);

	return len - ret;

err_copy_from_user:
	kfree(kbuf);
err:
	return len - ret;
}

/*
 * i82540EM_reset_device - sets the RST bit and resets the device.
 *			   sleeps after reset to ensure it has time to reset.
 */
static void i82540EM_reset_device(void)
{
	u32 result;
	u32 __iomem *workreg;
	u32 __iomem *bar = mydev.hwbar;

	workreg = HWREG_U32(bar, DEV_CTRL);
	result = ioread32(workreg);

	result |= CTRL_RST;
	iowrite32(result, workreg);

	/* TODO: not sure if this is required for the CTRL_RST, but RST# does */
	msleep(RST_SLEEP_DELAY);
}

/*
 * i82540EM_set_gen_config - sets the configuation of the NIC to the general
 *			     config (page 385 in manual)
 */
static void i82540EM_set_gen_config(void)
{
	u32 result;
	u32 __iomem *workreg;
	u32 __iomem *bar = mydev.hwbar;

	/* work on device control reg */
	workreg = HWREG_U32(bar, DEV_CTRL);
	result = ioread32(workreg);

	result |= CTRL_ASDE_SLU;   /* enable ASDE and SLU */
	iowrite32(result, workreg);

	result &= ~(CTRL_PHY_RST); /* disable PHY_RST */
	iowrite32(result, workreg);

	result &= ~(CTRL_ILOS);    /* disable ILOS */
	iowrite32(result, workreg);

	/* init/disable flow control registers */
	workreg = HWREG_U32(bar, FCAH);
	iowrite32(FLOW_REGS_INIT, workreg);

	workreg = HWREG_U32(bar, FCAL);
	iowrite32(FLOW_REGS_INIT, workreg);

	workreg = HWREG_U32(bar, FCT);
	iowrite32(FLOW_REGS_INIT, workreg);

	workreg = HWREG_U32(bar, FCTTV);
	iowrite32(FLOW_REGS_INIT, workreg);

	/* no VLAN */
	workreg = HWREG_U32(bar, DEV_CTRL);
	result = ioread32(workreg);

	result &= ~(CTRL_VME);
	iowrite32(result, workreg);

	pr_info(DEV_NAME ": latest result sent to DEV_CTRL. %X", result);
}

static int i82540EM_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int ret;
	u32 bars;
	resource_size_t mmio_start;
	resource_size_t mmio_len;

	pr_info(DEV_NAME ": probe starting...");

	ret = pci_enable_device_mem(pdev);
	if (unlikely(ret)) {
		pr_err(DEV_NAME ": pci_enable_device_mem() failed. error %d",
		       ret);
		goto err;
	}

	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	ret = pci_request_selected_regions(pdev, bars, DEV_NAME);
	if (unlikely(ret)) {
		pr_err(DEV_NAME
		       ": pci_requiest_selected_regions() failed, error %d",
		       ret);
		goto err_pci_request_selected_regions;
	}

	pci_set_master(pdev);

	mmio_start = pci_resource_start(pdev, MM_BAR0);
	mmio_len = pci_resource_len(pdev, MM_BAR0);

	mydev.hwbar = ioremap(mmio_start, mmio_len);
	if (unlikely(!mydev.hwbar)) {
		ret = -EIO;
		pr_err(DEV_NAME ": ioremap() failed, error %d", ret);
		goto err_ioremap_bar;
	}

	i82540EM_reset_device();
	i82540EM_set_gen_config();

	/* initialize LEDS to off */
	ret = i82540EM_set_all_leds(LED_OFF, BLINK_OFF);
	if (ret)
		goto err_set_all_leds;

	pr_info(DEV_NAME ": probe complete");

	return SUCCESS;

err_set_all_leds:
	iounmap(mydev.hwbar);
err_ioremap_bar:
	/*pci_release_selected_regions(pdev, bars);*/
err_pci_request_selected_regions:
	pci_release_mem_regions(pdev);
err:
	pci_disable_device(pdev);
	return ret;
}

static void i82540EM_remove(struct pci_dev *pdev)
{
	u32 bars = pci_select_bars(pdev, IORESOURCE_MEM);
	iounmap(mydev.hwbar);
	pci_release_selected_regions(pdev, bars);
	/* pci_release_mem_regions(pdev); BUG: Frees non existant resource */
	pci_disable_device(pdev);
}

static int __init i82540EM_init(void)
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

	/* register PCI device for use */
	ret = pci_register_driver(&i82540EM_pci_driver);
	if (unlikely(ret)) {
		pr_err(DEV_NAME ": pci_register_driver() failed. error %d\n",
		       ret);
		goto pci_register_driver_out;
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

	pr_info(DEV_NAME ": init complete");

	return SUCCESS;

device_create_out:
	class_destroy(mydev.myclass);
class_create_out:
	pci_unregister_driver(&i82540EM_pci_driver);
pci_register_driver_out:
	cdev_del(&mydev.my_cdev);
cdev_add_out:
	unregister_chrdev_region(mydev.devnode, DEVCNT);
out:
	return ret;
}

static void __exit i82540EM_exit(void)
{
	pr_info(DEV_NAME ": cleaning up...\n");
	device_destroy(mydev.myclass, mydev.devnode);
	class_destroy(mydev.myclass);
	pci_unregister_driver(&i82540EM_pci_driver);
	cdev_del(&mydev.my_cdev);
	unregister_chrdev_region(mydev.devnode, DEVCNT);
	pr_info(DEV_NAME ": exiting...\n");
}

MODULE_AUTHOR("James Ross");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.3");
module_init(i82540EM_init);
module_exit(i82540EM_exit);
