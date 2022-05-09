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
#include <linux/time.h>

#define SUCCESS 0
#define FAILURE -1

#define MINOR_STRT 0 /* starting minor number */
#define DEVCNT 1
#define DEV_NAME "intel_82540EM"

#define SIZEU32  (sizeof(u32))
#define SIZEU8   (sizeof(u8))
#define SIZE_INT (sizeof(int))

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
#define LED0_MODE_OFF	   0x0000000F
#define LED0_MODE_BLINK_ON ((LED0_MODE_ON) | (LED0_BLINK))
#define LED0_MODE_MASK     0x0000000F

#define LED1_NUM           0x1
#define LED1_MODE_ON       0x00000E00
#define LED1_IVRT          0x00004000
#define LED1_BLINK         0x00008000
#define LED1_MODE_OFF	   0x00000F00
#define LED1_MODE_BLINK_ON ((LED1_MODE_ON) | (LED1_BLINK))
#define LED1_MODE_MASK     0x00000F00

#define LED2_NUM           0x2
#define LED2_MODE_ON       0x000E0000
#define LED2_IVRT          0x00400000
#define LED2_BLINK         0x00800000
#define LED2_MODE_OFF	   0x000F0000
#define LED2_MODE_BLINK_ON ((LED2_MODE_ON) | (LED2_BLINK))
#define LED2_MODE_MASK     0x000F0000

#define LED3_NUM           0x3
#define LED3_MODE_ON       0x0E000000
#define LED3_IVRT	   0x40000000
#define LED3_BLINK	   0x80000000
#define LED3_MODE_OFF	   0x0F000000
#define LED3_MODE_BLINK_ON ((LED3_MODE_ON) | (LED3_BLINK))
#define LED3_MODE_MASK     0x0F000000

#define ALL_LED_MODE_ON							       \
	((LED0_MODE_ON) | (LED1_MODE_ON) | (LED2_MODE_ON) | (LED3_MODE_ON))

#define ALL_LED_IVRT							       \
	((LED0_IVRT_ON) | (LED1_IVRT) | (LED2_IVRT) | (LED3_IVRT))

#define ALL_LED_BLINK						       \
	((LED0_BLINK) | (LED1_BLINK) | (LED2_BLINK) | (LED3_BLINK))

#define ALL_LED_MODE_OFF						       \
	((LED0_MODE_OFF) | (LED1_MODE_OFF) | (LED2_MODE_OFF) | (LED3_MODE_OFF))

#define ALL_LED_MODE_BLINK_ON ((ALL_LED_MODE_ON) | (ALL_LED_BLINK))

/* led definitions that are used on data sent from user */
#define LED_OPT_MODE_ON  0x1
#define LED_OPT_BLINK_ON 0x2

#define UNPACK_LED0(packed) (((packed) << 6) >> 6)
#define UNPACK_LED1(packed) (((packed) << 4) >> 6)
#define UNPACK_LED2(packed) (((packed) << 2) >> 6)
#define UNPACK_LED3(packed) (((packed) << 0) >> 6)

/* timer definitions */
#define SEC_TO_MS(seconds) (seconds * 1000)

#define DFLT_IF_ZERO_TIME 1 /* in seconds, used if blink_rate = 0 */
#define NO_FLAGS 0


			/* file ops */
static int i82540EM_open(struct inode *inode, struct file *file);
static int i82540EM_release(struct inode *inode, struct file *file);
static ssize_t i82540EM_read(struct file *file, char __user *buf,
                               size_t len, loff_t *offset);
static ssize_t i82540EM_write(struct file *file, const char __user *buf,
                                size_t len, loff_t *offset);

			/* pci ops */
static int i82540EM_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void i82540EM_remove(struct pci_dev *pdev);

struct i82540EM_led_states {
	bool led0;
	bool led1;
	bool led2;
	bool led3;
};

struct i82540EM_dev {
	struct cdev my_cdev;
	struct pci_dev *pdev;

	struct class *myclass;
	struct device *mydevice;

	struct timer_list blink_timer;
	unsigned long prev_jiff;
	dev_t devnode;

	struct i82540EM_led_states led_state;

	void __iomem *hwbar;
};


static struct file_operations mydev_fops = {
	.owner   = THIS_MODULE,
	.open    = i82540EM_open,
	.release = i82540EM_release,
	.read    = i82540EM_read,
	.write   = i82540EM_write,
	.llseek  = default_llseek
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

/* this shows up under /sys/modules/<module name>/parameters */
static int blink_rate = 2; /* in seconds */
module_param(blink_rate, int, S_IRUSR | S_IWUSR);

/*
 * i82540EM_set_led - sets an individual led
 *
 * @mydev - i82540EM_dev struct that holds my module information
 * @ledreg - address of led control register
 * @lenum - Which number of led to change. LED[0-3]_NUM
 * @state - Which state to set. true for on false for off.
 * @blink - which state to set blink. true for on false for off
 *
 * returns SUCCESS on completeion
 * returns -ERRNO on failure
 */
static int i82540EM_set_led(struct i82540EM_dev *mydev, int lednum, bool state,
			    bool blink)
{
	u32 regval;
	u32 __iomem *ledreg = HWREG_U32(mydev->hwbar, LEDCTRL);
	int ret;

	regval = ioread32(ledreg);
	switch (lednum) {
	case LED0_NUM:
		if (blink)
			regval |= LED0_BLINK;
		else
			regval &= ~(LED0_BLINK);

		if (state) {
			regval &= ~(LED0_MODE_MASK);
			regval |= LED0_MODE_ON;
		} else {
			regval &= ~(LED0_MODE_MASK);
			regval |= LED0_MODE_OFF;
		}

		mydev->led_state.led0 = state;
		ret = SUCCESS;
	break;

	case LED1_NUM:
		if (blink)
			regval |= LED1_BLINK;
		else
			regval &= ~(LED1_BLINK);

		if (state) {
			regval &= ~(LED1_MODE_MASK);
			regval |= LED1_MODE_ON;
		} else {
			regval &= ~(LED1_MODE_MASK);
			regval |= LED1_MODE_OFF;
		}

		mydev->led_state.led1 = state;
		ret = SUCCESS;
	break;

	case LED2_NUM:
		if (blink)
			regval |= LED2_BLINK;
		else
			regval &= ~(LED2_BLINK);

		if (state) {
			regval &= ~(LED2_MODE_MASK);
			regval |= LED2_MODE_ON;
		} else {
			regval &= ~(LED2_MODE_MASK);
			regval |= LED2_MODE_OFF;
		}

		mydev->led_state.led2 = state;
		ret = SUCCESS;
	break;

	case LED3_NUM:
		if (blink)
			regval |= LED3_BLINK;
		else
			regval &= ~(LED3_BLINK);

		if (state) {
			regval &= ~(LED3_MODE_MASK);
			regval |= LED3_MODE_ON;
		} else {
			regval &= ~(LED3_MODE_MASK);
			regval |= LED3_MODE_OFF;
		}

		mydev->led_state.led3 = state;
		ret = SUCCESS;
	break;

	default:
		ret = -EINVAL;
		dev_err(&mydev->pdev->dev,
		       ": Invalid led number to i82540EM_set_led(). error %d\n", ret);
		return ret;
	}

	/* set the led_reg */
	iowrite32(regval, ledreg);

	regval = ioread32(ledreg);
	dev_info(&mydev->pdev->dev,
		 "led regval after write in set_led: 0x%08X\n", regval);
	return ret;
}

/*
 * i82540EM_set_all_leds - sets all leds to the same state and blink state.
 *
 * @mydev - i82540EM_dev struct that holds my module information
 * @ledreg - address of led control register
 * @state - state to turn the leds to. true for on false for off.
 * @blink - state to turn blink to. true for on false for off.
 *
 * returns SUCCESS on success
 * returns -ERRNO error code on failure.
 */
static int i82540EM_set_all_leds(struct i82540EM_dev *mydev, bool state, bool blink)
{
	int ret;

	ret = i82540EM_set_led(mydev, LED0_NUM, state, blink);
	if (unlikely(ret)) {
		pr_err(DEV_NAME
		       ": LED0 failed to set, i82540EM_set_all_leds, error %d\n",
		       ret);
		return ret;
	}

	ret = i82540EM_set_led(mydev, LED1_NUM, state, blink);
	if (unlikely(ret)) {
		pr_err(DEV_NAME
		       ": LED1 failed to set, i82540EM_set_all_leds, error %d\n",
		       ret);
		return ret;
	}

	ret = i82540EM_set_led(mydev, LED2_NUM, state, blink);
	if (unlikely(ret)) {
		pr_err(DEV_NAME
		       ": LED2 failed to set, i82540EM_set_all_leds, error %d\n",
		       ret);
		return ret;
	}

	ret = i82540EM_set_led(mydev, LED3_NUM, state, blink);
	if (unlikely(ret)) {
		pr_err(DEV_NAME
		       ": LED3 failed to set, i82540EM_set_all_leds, error %d\n",
		       ret);
		return ret;
	}


	return ret;
}

/*
 * i82540EM_blink_timer_cb - timer callback function for blinking LED0
 *
 * turns on or off LED0 based on current state to blink the device at a
 * 50% duty cycle based on the parameter blink_rate
 *
 * sets the timer for half the blink_rate time.
 * Since blink cycle is in seconds, we convert to ms then to jiffies to
 * obtain the 50% duty cycle when rearming the timer.
 */
static void i82540EM_blink_timer_cb(struct timer_list *t)
{
	struct i82540EM_dev *mydev = from_timer(mydev, t, blink_timer);
	unsigned long blink_rate_jiff;
	int blink_rate_ms;

	dev_info(&mydev->pdev->dev,
		 "blink_rate = %d sec, time since last timer in jiff = %lu\n",
		 blink_rate, (jiffies - mydev->prev_jiff));

	if (blink_rate == 0) {
		i82540EM_set_led(mydev, LED0_NUM, LED_OFF, BLINK_OFF);
		mydev->prev_jiff = jiffies;
		mod_timer(&mydev->blink_timer,
			  (jiffies + (DFLT_IF_ZERO_TIME * HZ)));
		return;
	}

	/* set LED0 to opposite state to manually blink */
	i82540EM_set_led(mydev, LED0_NUM, !mydev->led_state.led0, BLINK_OFF);

	/* get blink cycle time in jiffies */
	blink_rate_ms = SEC_TO_MS(blink_rate);
	blink_rate_jiff = msecs_to_jiffies(blink_rate_ms);

	mydev->prev_jiff = jiffies;

	dev_info(&mydev->pdev->dev,
		 "new blink_rate_ms = %d, new blink_rate_jiff = %lu, "
		 "new prev_jiff = %lu, current jiffies = %lu\n",
		 blink_rate_ms, blink_rate_jiff, mydev->prev_jiff, jiffies);

	/*
	 * rearm timer, set for half the blink_rate time to change state for
	 * blinking. 50% duty cycle
	 */
	mod_timer(&mydev->blink_timer, (jiffies + (blink_rate_jiff/2)));
}

/*
 * i82540EM_open - open and set inode private data, arm blink timer
 *
 * returns: SUCCESS
 */
static int i82540EM_open(struct inode *inode, struct file *file)
{
	struct i82540EM_dev *mydev;
	unsigned long blink_rate_jiff;
	int blink_rate_ms;

	mydev = container_of(inode->i_cdev,
			     struct i82540EM_dev,
			     my_cdev);

	if (blink_rate == 0) {
		/*
		 * arm timer to a default time so timer call back can check if
		 * blink cycle turns non zero.
		 */
		mydev->prev_jiff = jiffies;
		mod_timer(&mydev->blink_timer,
			  (jiffies + (DFLT_IF_ZERO_TIME * HZ)));

		file->private_data = mydev;
		return SUCCESS;
	}

	/* get blink cycle time in jiffies */
	blink_rate_ms = SEC_TO_MS(blink_rate);
	blink_rate_jiff = msecs_to_jiffies(blink_rate_ms);

	mydev->prev_jiff = jiffies;

	/* arm timer for 50% duty cycle blink */
	mod_timer(&mydev->blink_timer, (jiffies + (blink_rate_jiff/2)));

	file->private_data = mydev;

	dev_info(&mydev->pdev->dev, "Successfully opened.");

	return SUCCESS;
}

int i82540EM_release(struct inode *inode, struct file *file)
{
	struct i82540EM_dev *mydev = file->private_data;

	/* disarm timer */
	del_timer_sync(&mydev->blink_timer);

	/* clear the private_data member just to be safe */
	file->private_data = NULL;

	return SUCCESS;
}

/*
 * i82540EM_read - return blink_rate parameters current value
 *
 * returns: on success, returns the number of bytes read from the kernel data.
 *	    on failure -ERRNO is returned.
 */
static ssize_t i82540EM_read(struct file *file, char __user *buf,
			     size_t len, loff_t *offset)
{
	struct i82540EM_dev *mydev;
	ssize_t ret = 0;

	mydev = file->private_data;

	if (unlikely(!buf)) {
		ret = -EINVAL;
		dev_err(&mydev->pdev->dev,
			"NULL buffer passed to read(), error %zu\n",
		        ret);
		goto err;
	}

	if (unlikely(len != SIZE_INT)) {
		ret = -EINVAL;
		dev_err(&mydev->pdev->dev,
		       ": Length of passed buffer not u32. error %zu\n", ret);
		goto err;
	}

	ret = copy_to_user(buf, &blink_rate, len);
	if (unlikely(ret == len)) {
		ret = -EFAULT;
		dev_err(&mydev->pdev->dev,
		         ": copy_to_user did not write len bytes, error %zu",
			 ret);
		goto err;
	}

	return len - ret;

err:
	return ret;
}

/*
 * i82540EM_create_led_reg_val - creates a u32 register value and returns it.
 * Register values are based on the led_options struct values.
 *
 * @reg_val - the current value from the register to be altered based on
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
	if (led_opts->led0_opt & LED_OPT_MODE_ON) {
		if (led_opts->led0_opt & LED_OPT_BLINK_ON) {
			reg_val |= LED0_MODE_BLINK_ON;
		} else {
			reg_val |= LED0_MODE_ON;
			reg_val &= ~(LED0_BLINK);
		}
	} else {
		reg_val |= LED0_MODE_OFF;
		if (led_opts->led0_opt & LED_OPT_BLINK_ON)
			reg_val |= LED0_BLINK;
		else
			reg_val |= ~(LED0_BLINK);
	}

	/* set reg_val for LED1 */
	if (led_opts->led1_opt & LED_OPT_MODE_ON) {
		if (led_opts->led1_opt & LED_OPT_BLINK_ON) {
			reg_val |= LED1_MODE_BLINK_ON;
		} else {
			reg_val |= LED1_MODE_ON;
			reg_val &= ~(LED1_BLINK);
		}
	} else {
		reg_val |= LED1_MODE_OFF;
		if (led_opts->led1_opt & LED_OPT_BLINK_ON)
			reg_val |= LED1_BLINK;
		else
			reg_val |= ~(LED1_BLINK);
	}

	/* set reg_val for LED2 */
	if (led_opts->led2_opt & LED_OPT_MODE_ON) {
		if (led_opts->led2_opt & LED_OPT_BLINK_ON) {
			reg_val |= LED2_MODE_BLINK_ON;
		} else {
			reg_val |= LED2_MODE_ON;
			reg_val &= ~(LED2_BLINK);
		}
	} else {
		reg_val |= LED2_MODE_OFF;
		if (led_opts->led2_opt & LED_OPT_BLINK_ON)
			reg_val |= LED2_BLINK;
		else
			reg_val |= ~(LED2_BLINK);
	}

	/* set reg_val for LED3 */
	if (led_opts->led3_opt & LED_OPT_MODE_ON) {
		if (led_opts->led3_opt & LED_OPT_BLINK_ON) {
			reg_val |= LED3_MODE_BLINK_ON;
		} else {
			reg_val |= LED3_MODE_ON;
			reg_val &= ~(LED3_BLINK);
		}
	} else {
		reg_val |= LED3_MODE_OFF;
		if (led_opts->led3_opt & LED_OPT_BLINK_ON)
			reg_val |= LED3_BLINK;
		else
			reg_val |= ~(LED3_BLINK);
	}

	return reg_val;
}

/*
 * i82540EM_write - writes to the paramater variable blink_rate to change
 *		    the leds blink timer.
 *
 * returns: On success, returns the number of bytes written. If this value is
 *	    not a byte, no data is written.
 *	    On failure -ERRNO is returned.
 */
static ssize_t i82540EM_write(struct file *file, const char __user *buf,
			      size_t len, loff_t *ppos)
{
	struct i82540EM_dev *mydev;
	ssize_t ret;
	int *kbuf;

	mydev = file->private_data;

	if (unlikely(!buf)) {
		ret = -EINVAL;
		dev_err(&mydev->pdev->dev,
			"write failed, user buf was NULL. error %zd\n",
		        ret);
		goto err;
	}

	/*
	 * user trying to write more or less than the data type of the modules
	 * data.
	 */
	if (unlikely(len != SIZE_INT)) {
		ret = -EINVAL;
		dev_err(&mydev->pdev->dev,
		       "write failed, len longer/shorter than module "
		       "data type. error %zd\n", ret);
		goto err;
	}

	kbuf = kcalloc(len, SIZEU8, GFP_KERNEL);
	if (unlikely(!kbuf)) {
		ret = -ENOMEM;
		dev_err(&mydev->pdev->dev,
		       "write failed could not kcalloc, error %zd\n",
		       ret);
		goto err;
	}

	/* get data from user buff */
	ret = copy_from_user(kbuf, buf, len);
	if (unlikely(ret == len)) {
		ret = -EFAULT;
		dev_err(&mydev->pdev->dev,
			 "len bytes not written to kbuf in write.\n");
		goto err_copy_from_user;
	}

	if (*kbuf < 0) {
		ret = -EINVAL;
		dev_err(&mydev->pdev->dev,
			"User passed negetive blink rate to write\n");
		goto err_neg_val;
	}

	/* write new value to blink_rate */
	blink_rate = *kbuf;

	dev_info(&mydev->pdev->dev, "new blink_rate value = %d seconds\n",
		 blink_rate);

	kfree(kbuf);

	return len - ret;

err_neg_val:
err_copy_from_user:
	kfree(kbuf);
err:
	return ret;
}

/*
 * i82540EM_reset_device - sets the RST bit and resets the device.
 *			   sleeps after reset to ensure it has time to reset.
 */
static void i82540EM_reset_device(struct i82540EM_dev *mydev)
{
	u32 result;
	u32 __iomem *workreg;
	u32 __iomem *bar = mydev->hwbar;

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
static void i82540EM_set_gen_config(struct i82540EM_dev *mydev)
{
	u32 result;
	u32 __iomem *workreg;
	u32 __iomem *bar = mydev->hwbar;

	/* work on device control reg */
	workreg = HWREG_U32(bar, DEV_CTRL);
	result = ioread32(workreg);

	dev_info(&mydev->pdev->dev,
		 "initial state of device control, 0x%08X\n", result);

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

	dev_info(&mydev->pdev->dev, "latest result sent to DEV_CTRL. 0x%08X",
		 result);
}

static int i82540EM_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct i82540EM_dev *mydev;
	int ret;
	u32 bars;
	resource_size_t mmio_start;
	resource_size_t mmio_len;

	dev_info(&pdev->dev, ": probe starting...");

	mydev = kzalloc(sizeof(struct i82540EM_dev), GFP_KERNEL);
	if (!mydev) {
		ret = -ENOMEM;
		goto err_mydev_alloc;
	}

	ret = alloc_chrdev_region(&mydev->devnode, MINOR_STRT, DEVCNT, DEV_NAME);
	if (unlikely(ret)) {
		dev_err(&pdev->dev, "alloc_chrdev_region() failed. error %d\n",
		       ret);
		goto err_alloc_chrdev_region;
	}

	dev_info(&pdev->dev, "Allocated %d devices at major %d\n",
		DEVCNT, MAJOR(mydev->devnode));

	/* init the character device and add it to the kernel */
	cdev_init(&mydev->my_cdev, &mydev_fops);
	mydev->my_cdev.owner = THIS_MODULE;

	ret = cdev_add(&mydev->my_cdev, mydev->devnode, DEVCNT);
	if (unlikely(ret)) {
		dev_err(&pdev->dev, "cdev_add() failed. error %d\n", ret);
		goto err_cdev_add;
	}

	/* place module in /dev with class and device, requires a GPL license */
	mydev->myclass = class_create(THIS_MODULE, DEV_NAME);
	ret = IS_ERR(mydev->myclass);
	if (unlikely(ret)){
		dev_err(&pdev->dev, "class_create() failed. error %d\n", ret);
		goto err_class_create;
	}

	mydev->mydevice = device_create(mydev->myclass, NULL, mydev->devnode,
				       NULL, DEV_NAME);
	ret = IS_ERR(mydev->mydevice);
	if (unlikely(ret)) {
		dev_err(&pdev->dev,"device_create() failed. error %d\n", ret);
		goto err_device_create;
	}

	dev_info(&pdev->dev, "device created in /dev\n");

	ret = pci_enable_device_mem(pdev);
	if (unlikely(ret)) {
		dev_err(&pdev->dev, "pci_enable_device_mem() failed. error %d",
		       ret);
		goto err_pci_enable_device_mem;
	}

	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	ret = pci_request_selected_regions(pdev, bars, DEV_NAME);
	if (unlikely(ret)) {
		dev_err(&pdev->dev,
		       "pci_requiest_selected_regions() failed, error %d",
		       ret);
		goto err_pci_request_selected_regions;
	}

	pci_set_master(pdev);

	/* save our driver struct */
	mydev->pdev = pdev;
	pci_set_drvdata(pdev, mydev);

	mmio_start = pci_resource_start(pdev, MM_BAR0);
	mmio_len = pci_resource_len(pdev, MM_BAR0);

	mydev->hwbar = ioremap(mmio_start, mmio_len);
	if (unlikely(!mydev->hwbar)) {
		ret = -EIO;
		dev_err(&pdev->dev, "ioremap() failed, error %d", ret);
		goto err_ioremap_bar;
	}

	i82540EM_reset_device(mydev);
	i82540EM_set_gen_config(mydev);

	/* initialize LEDS to off */
	ret = i82540EM_set_all_leds(mydev, LED_OFF, BLINK_OFF);
	if (unlikely(ret))
		goto err_set_all_leds;

	/* set timer for blink */
	timer_setup(&mydev->blink_timer, i82540EM_blink_timer_cb, NO_FLAGS);

	dev_info(&pdev->dev, "probe complete");

	return SUCCESS;

err_set_all_leds:
	iounmap(mydev->hwbar);
err_ioremap_bar:
	/* pci_release_selected_regions(pdev, bars); */
err_pci_request_selected_regions:
	pci_release_mem_regions(pdev);
err_pci_enable_device_mem:
	pci_disable_device(pdev);
	device_destroy(mydev->myclass, mydev->devnode);
err_device_create:
	class_destroy(mydev->myclass);
err_class_create:
	cdev_del(&mydev->my_cdev);
err_cdev_add:
	unregister_chrdev_region(mydev->devnode, DEVCNT);
err_alloc_chrdev_region:
	kfree(mydev);
err_mydev_alloc:
	return ret;
}

static void i82540EM_remove(struct pci_dev *pdev)
{
	struct i82540EM_dev *mydev;
	u32 bars;

	mydev = pci_get_drvdata(pdev);

	/* pci */
	iounmap(mydev->hwbar);
	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	pci_release_selected_regions(pdev, bars);
	/* pci_release_mem_regions(pdev); Frees nonexistant resource */
	pci_disable_device(pdev);

	/* cdev, class, device */
	device_destroy(mydev->myclass, mydev->devnode);
	class_destroy(mydev->myclass);
	cdev_del(&mydev->my_cdev);
	unregister_chrdev_region(mydev->devnode, DEVCNT);
	kfree(mydev);
}

static int __init i82540EM_init(void)
{
	int ret;

	pr_info(DEV_NAME ": module loading... init blink_rate = %d\n",
		blink_rate);

	/* register PCI device for use */
	ret = pci_register_driver(&i82540EM_pci_driver);
	if (unlikely(ret)) {
		pr_err(DEV_NAME ": pci_register_driver() failed. error %d\n",
		       ret);
		return ret;
	}

	pr_info(DEV_NAME ": init complete\n");

	return SUCCESS;
}

static void __exit i82540EM_exit(void)
{
	pr_info(DEV_NAME ": cleaning up...\n");
	pci_unregister_driver(&i82540EM_pci_driver);
	pr_info(DEV_NAME ": exiting...\n");
}

MODULE_AUTHOR("James Ross");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.4");
module_init(i82540EM_init);
module_exit(i82540EM_exit);
