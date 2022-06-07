/*
 * ece373 assignment 4
 *
 *
 * This module implements read() write() open() llseek() and
 * sends data back and forth between kerenel space and user space.
 *
 * this module implements probe() remove() and sets up for the pci device 82540EM
 * intel ethernet card.
 *
 * Uses a timer to blink LED0 at a 50% duty cycle given by a paramater or a
 * write from the user.
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
#include <linux/workqueue.h>
#include <linux/interrupt.h>

#define SUCCESS 0
#define FAILURE -1

#define NO_FLAGS 0

#define MINOR_STRT 0 /* starting minor number */
#define DEVCNT 1
#define DEV_NAME "intel_82540EM"

#define SIZEU32  (sizeof(u32))
#define SIZEU8   (sizeof(u8))
#define SIZE_INT (sizeof(int))
#define BUFF_INFO_SIZE (sizeof(struct i82540EM_rx_buff_info))

#define NO_WR 0 /* no write was done, 0 bytes written */

#define i82540EM_VEND_ID 0x8086
#define i82540EM_DEV_ID  0x100e

#define HWREG_U32(bar, offset) ((u32*)(((char*)bar)+(offset)))

#define MM_BAR0 0 /* Memory maps IO on the 82540EM is bar 0 */

/* device control registers and mask definitions */
#define DEV_CTRL     0x00000000 /* device control register offset */
#define CTRL_RST     0x04000000
#define CTRL_ASDE    0x00000020 /* auto speed detection enable */
#define CTRL_SLU     0x00000040 /* set link up */
#define CTRL_PHY_RST 0x80000000
#define CTRL_ILOS    0x00000080 /* invert loss of signal */
#define CTRL_VME     0x40000000 /* VLAN mode enable */
#define CTRL_SPEED   0x00000300
#define TEN_MBS      0x00000000
#define HUND_MBS     0x00000100
#define THOUS_MBS    0x00000200

#define RST_SLEEP_DELAY 100 /* delay while polling for RST, in ms */

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

#define ALL_LED_BLINK							       \
	((LED0_BLINK) | (LED1_BLINK) | (LED2_BLINK) | (LED3_BLINK))

#define ALL_LED_MODE_OFF						       \
	((LED0_MODE_OFF) | (LED1_MODE_OFF) | (LED2_MODE_OFF) | (LED3_MODE_OFF))

#define ALL_LED_MODE_BLINK_ON ((ALL_LED_MODE_ON) | (ALL_LED_BLINK))

/* Receive registers */
#define RCTRL  0x00100 /* Receive control register */
#define RCTRL_EN   0x00000001 /* enable bit */
#define LPE	   0x00000020 /* Long Packet Enable */
#define LBM        0x000000C0 /* loopback mode */
#define LBM_INIT   0x00000000 /* no loopback */
#define RDMTS      0x00000300 /* Receive descriptor minimum threshhold size */
#define RDMTS_INIT 0x00000200 /* Free buffer threshhold is set to 1/8 RDLEN */
#define MO         0x00030000 /* multi-cast offset */
#define MO_INIT    0x00000000 /* bits [47:36] of received destination mo addr */
#define BAM	   0x00008000 /* Broadcast accept mode */
#define BSIZE_BITS 0x00030000 /* Receive buffer size */
#define BSIZE_INIT 0x00000000 /* 2048 bytes */
#define SECRC      0x04000000 /* Strip ethernet CTC from incomming packet */
#define UPE 0x00000008 /* unicast promiscuous enable bit */
#define MPE 0x00000010 /* multicast promiscuous enable bit */

#define RDBAL  0x02800 /* Receive descriptor base addr low */
#define RDBAH  0x02804 /* Receive descriptor base addr high */
#define RDLEN  0x02808 /* Receive descriptor length */
#define RDLEN_VAL 0x00002000 /* 16 decriptors shifted to len feild */
#define RDH    0x02810 /* Receive descriptor head */
#define RDT    0x02818 /* Receive descriptor tail */
#define RXDCTL 0x02828 /* Receive descriptor control */

/* filter registers */
#define MTA 0x5200 /* multicast table array, 0x5200-0x53FC, 128bits total */
#define MTA_VECT_INIT 0x0
#define MTA_NUM_VECTS 4

#define RAL0 0x05400 /* reg0 of read address low, holds MAC addr low 32bit */
#define RAH0 0x05404 /* reg0 of read address high, holds MAC addr high 16bit */

/* interrupt registers */
#define ICR 0x000C0 /* interrupt cause read register */
#define ICS 0x000C8 /* interrupt cause set register */
#define IMS 0x000D0 /* interrupt mask set/read register */
#define IMC 0x000D8 /* interrupt mask clear register */
#define RXDMT0 0x00000008 /* set min desc threshold hit interrupt */
#define DISABLE_ALL_INTS 0xFFFFFFFF /* used in the TMC clearing interrupts */

/* status registers */
#define GPRC 0x04074 /* good packets received register */

/* EEPROM definitions */
#define EERD	     0x00014	/* EEPROM read register */
#define EE_STRT	     0x00000001 /* EEPROM start bit used to initiate read */
#define EE_DONE	     0x00000010 /* EEPROM done, set to 1 when no read in prog */
#define EE_MAC_ADDRL 0x00000000 /* MAC addr location for first 2 bytes */
#define EE_MAC_ADDRH 0x00000200 /* MAC addr location for last byte shifted */
#define EE_ADDR(addr) ((addr) << 8) /* shifts addr to addr cell location */

/* reads the GPRC to flush any writes pending */
#define WRITE_FLUSH(adaptor_ptr) ioread32((adaptor_ptr)->hwbar + GPRC)

/* timer definitions */
#define SEC_TO_MS(seconds) ((seconds) * 1000)

#define DFLT_IF_ZERO_TIME 1 /* in seconds, used if blink_rate = 0 */

/* DMA definitions */
#define DESC_CNT 16
#define DESC_RING_SIZE (DESC_CNT * sizeof(struct i82540EM_rx_desc))
#define RX_BUF_SIZE 2048 /* 2k buffer size */

#define DESC_STAT    0xFF
#define DESC_STAT_DD 0x01

#define GET_RX_DESC(rx_ring, i)						       \
	(&(((struct i82540EM_rx_desc*)rx_ring->desc)[i]))

#define GET_LOWER_32(addr)  (((addr) << 32) >> 32)
#define GET_HIGHER_32(addr) ((addr) >> 32)

#define WORK_RX_DELAY 500 /* rx workqueue service delay ms */

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

struct i82540EM_rx_desc {
	__le64 buf_addr; /* address of the descriptors data buffer */
	union {
		__le32 data;
		struct {
			__le16 len;  /* length of data DMAed into data buffer */
			__le16 csum; /* packet check sum */
		} fields;
	} lower;

	union {
		__le32 data;
		struct {
			u8 status; /* descriptor status */
			u8 error;  /* descriptor errors */
			__le16 special;
		} fields;
	} upper;
};

struct i82540EM_rx_buff_info {
	dma_addr_t dma; /* pinned address for buffer used in desc */
	size_t size;    /* size of desc buffer */
};

struct i82540EM_rx_ring {
	void *desc;	/* pointer to desc ring */
	dma_addr_t dma; /* phys address of ring */

	size_t size;	    /* length of ring in bytes */
	unsigned int count; /* number of desc in ring */

	u16 next_to_use;   /* index of next desc to use */
	u16 next_to_clean; /* index of next desc to clean */

	void *buffer[DESC_CNT]; /* receive buffer which descriptors point to */
	struct i82540EM_rx_buff_info *buff_info; /* rx buff info of desc */
};

struct i82540EM_led_states {
	bool led0;
	bool led1;
	bool led2;
	bool led3;

	bool blink_led0;
	bool blink_led1;
	bool blink_led2;
	bool blink_led3;
};

struct i82540EM_dev {
	struct cdev cdev;
	struct pci_dev *pdev;

	struct class *class;
	struct device *dev;

	struct timer_list blink_timer;
	unsigned long prev_jiff;

	dev_t devnode;

	struct i82540EM_led_states led_state;

	void __iomem *hwbar;

	struct i82540EM_rx_ring rx_ring;
	struct work_struct rx_work;
};

static struct file_operations dev_fops = {
	.owner   = THIS_MODULE,
	.open    = i82540EM_open,
	.release = i82540EM_release,
	.read    = i82540EM_read,
	.write   = i82540EM_write,
	.llseek  = no_llseek
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

/* this shows up under /sys/modules/<module name>/parameters */
static int blink_rate = 2; /* in seconds */
module_param(blink_rate, int, S_IRUSR | S_IWUSR);

/*
 * i82540EM_set_led - sets an individual led
 *
 * @dev - i82540EM_dev struct that holds my module information
 * @ledreg - address of led control register
 * @lenum - Which number of led to change. LED[0-3]_NUM
 * @state - Which state to set. true for on false for off.
 * @blink - which state to set blink. true for on false for off
 *
 * returns SUCCESS on completeion
 * returns -ERRNO on failure
 */
static int i82540EM_set_led(struct i82540EM_dev *dev, int lednum, bool state,
			    bool blink)
{
	u32 regval;
	u32 __iomem *ledreg = HWREG_U32(dev->hwbar, LEDCTRL);
	int ret;

	regval = ioread32(ledreg);
	switch (lednum) {
	case LED0_NUM:
		if (blink)
			regval |= LED0_BLINK;
		else
			regval &= ~(LED0_BLINK);

		regval &= ~(LED0_MODE_MASK);
		if (state)
			regval |= LED0_MODE_ON;
		else
			regval |= LED0_MODE_OFF;

		dev->led_state.led0 = state;
		dev->led_state.blink_led0 = blink;
		ret = SUCCESS;
	break;

	case LED1_NUM:
		if (blink)
			regval |= LED1_BLINK;
		else
			regval &= ~(LED1_BLINK);

		regval &= ~(LED1_MODE_MASK);
		if (state)
			regval |= LED1_MODE_ON;
		else
			regval |= LED1_MODE_OFF;

		dev->led_state.led1 = state;
		dev->led_state.blink_led1 = blink;
		ret = SUCCESS;
	break;

	case LED2_NUM:
		if (blink)
			regval |= LED2_BLINK;
		else
			regval &= ~(LED2_BLINK);

		regval &= ~(LED2_MODE_MASK);
		if (state)
			regval |= LED2_MODE_ON;
		else
			regval |= LED2_MODE_OFF;

		dev->led_state.led2 = state;
		dev->led_state.blink_led2 = blink;
		ret = SUCCESS;
	break;

	case LED3_NUM:
		if (blink)
			regval |= LED3_BLINK;
		else
			regval &= ~(LED3_BLINK);

		regval &= ~(LED3_MODE_MASK);
		if (state)
			regval |= LED3_MODE_ON;
		else
			regval |= LED3_MODE_OFF;

		dev->led_state.led3 = state;
		dev->led_state.blink_led3 = blink;
		ret = SUCCESS;
	break;

	default:
		ret = -EINVAL;
		dev_err(&dev->pdev->dev,
		       ": Invalid led number to i82540EM_set_led(). error %d\n", ret);
		return ret;
	}

	/* set the led_reg */
	iowrite32(regval, ledreg);

	regval = ioread32(ledreg);
	dev_info(&dev->pdev->dev,
		 "led number being changed: %d\n"
		 "led regval after write in set_led: 0x%08X\n",
		 lednum, regval);
	return ret;
}

/*
 * i82540EM_set_all_leds - sets all leds to the same state and blink state.
 *
 * @dev - i82540EM_dev struct that holds my module information
 * @ledreg - address of led control register
 * @state - state to turn the leds to. true for on false for off.
 * @blink - state to turn blink to. true for on false for off.
 *
 * returns SUCCESS on success
 * returns -ERRNO error code on failure.
 */
static void i82540EM_set_all_leds(struct i82540EM_dev *dev, bool state, bool blink)
{
	i82540EM_set_led(dev, LED0_NUM, state, blink);
	i82540EM_set_led(dev, LED1_NUM, state, blink);
	i82540EM_set_led(dev, LED2_NUM, state, blink);
	i82540EM_set_led(dev, LED3_NUM, state, blink);
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
	struct i82540EM_dev *dev = from_timer(dev, t, blink_timer);
	unsigned long blink_rate_jiff;
	int blink_rate_ms;

	if (blink_rate == 0) {
		i82540EM_set_led(dev, LED0_NUM, LED_OFF, BLINK_OFF);
		dev->prev_jiff = jiffies;
		mod_timer(&dev->blink_timer,
			 (jiffies + (DFLT_IF_ZERO_TIME * HZ)));
		return;
	}

	/* set LED0 to opposite state to manually blink */
	i82540EM_set_led(dev, LED0_NUM, !dev->led_state.led0, BLINK_OFF);

	/* get blink cycle time in jiffies */
	blink_rate_ms = SEC_TO_MS(blink_rate);
	blink_rate_jiff = msecs_to_jiffies(blink_rate_ms);

	dev->prev_jiff = jiffies;

	/*
	 * rearm timer, set for half the blink_rate time to change state for
	 * blinking. 50% duty cycle
	 */
	mod_timer(&dev->blink_timer, (jiffies + (blink_rate_jiff/2)));
}

/*
 * i82540EM_open - open and set inode private data, arm blink timer
 *
 * returns: SUCCESS
 */
static int i82540EM_open(struct inode *inode, struct file *file)
{
	struct i82540EM_dev *dev;
	unsigned long blink_rate_jiff;
	int blink_rate_ms;

	dev = container_of(inode->i_cdev,
			     struct i82540EM_dev,
			     cdev);

	if (blink_rate == 0) {
		/*
		 * arm timer to a default time so timer call back can check if
		 * blink cycle turns non zero.
		 */
		dev->prev_jiff = jiffies;
		mod_timer(&dev->blink_timer,
			 (jiffies + (DFLT_IF_ZERO_TIME * HZ)));

		file->private_data = dev;
		return SUCCESS;
	}

	/* get blink cycle time in jiffies */
	blink_rate_ms = SEC_TO_MS(blink_rate);
	blink_rate_jiff = msecs_to_jiffies(blink_rate_ms);

	dev->prev_jiff = jiffies;

	/* arm timer for 50% duty cycle blink */
	mod_timer(&dev->blink_timer, (jiffies + (blink_rate_jiff/2)));

	file->private_data = dev;

	dev_info(&dev->pdev->dev, "Successfully opened.\n");

	return SUCCESS;
}

/* i82540EM_release - release current file
 *
 * disarms the blink timer and sets file's private data to NULL;
 *
 * returns SUCCESS;
 */
static int i82540EM_release(struct inode *inode, struct file *file)
{
	struct i82540EM_dev *dev = file->private_data;

	/* disarm timer */
	del_timer_sync(&dev->blink_timer);

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
	struct i82540EM_dev *dev;
	ssize_t ret;
	u32 packed;
	u32 __iomem *workreg;

	dev = file->private_data;

	if (unlikely(!buf)) {
		ret = -EINVAL;
		dev_err(&dev->pdev->dev,
			"NULL buffer passed to read(), error %zu\n",
		        ret);
		goto err;
	}

	if (unlikely(len != SIZEU32)) {
		ret = -EINVAL;
		dev_err(&dev->pdev->dev,
		       ": Length of passed buffer not u32. error %zu\n", ret);
		goto err;
	}

	/* pack head and tail into a u32, upper head lower tail */
	workreg = HWREG_U32(dev->hwbar, RDH);
	packed = ioread32(workreg);
	packed <<= 16;
	packed |= dev->rx_ring.next_to_clean;

	ret = copy_to_user(buf, &packed, len);
	if (unlikely(ret == len)) {
		ret = -EFAULT;
		dev_err(&dev->pdev->dev,
		         ": copy_to_user did not write len bytes, error %zu\n",
			 ret);
		goto err;
	}

	return len - ret;

err:
	return ret;
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
	struct i82540EM_dev *dev;
	ssize_t ret;
	int *kbuf;

	dev = file->private_data;

	if (unlikely(!buf)) {
		ret = -EINVAL;
		dev_err(&dev->pdev->dev,
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
		dev_err(&dev->pdev->dev,
		       "write failed, len longer/shorter than module "
		       "data type. error %zd\n", ret);
		goto err;
	}

	kbuf = kcalloc(len, SIZEU8, GFP_KERNEL);
	if (unlikely(!kbuf)) {
		ret = -ENOMEM;
		dev_err(&dev->pdev->dev,
		       "write failed could not kcalloc, error %zd\n",
		       ret);
		goto err;
	}

	/* get data from user buff */
	ret = copy_from_user(kbuf, buf, len);
	if (unlikely(ret == len)) {
		ret = -EFAULT;
		dev_err(&dev->pdev->dev,
			 "len bytes not written to kbuf in write.\n");
		goto err_copy_from_user;
	}

	if (*kbuf < 0) {
		ret = -EINVAL;
		dev_err(&dev->pdev->dev,
			"User passed negetive blink rate to write\n");
		goto err_neg_val;
	}

	/* write new value to blink_rate */
	blink_rate = *kbuf;

	dev_info(&dev->pdev->dev, "new blink_rate value = %d seconds\n",
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
 * i82540EM_init_rx_ring - initializes the variables, allocates and pins desc
 *			   and buffers for i82540EM_rx_ring
 */
static int i82540EM_init_rx_ring(struct i82540EM_dev *dev)
{
	struct i82540EM_rx_ring *rx_ring = &dev->rx_ring;
	struct pci_dev *pdev = dev->pdev;
	struct i82540EM_rx_buff_info *buf_info;
	struct i82540EM_rx_desc *desc;
	int i, k;
	int ret;

	rx_ring->count = DESC_CNT;

	rx_ring->size = DESC_RING_SIZE;

	/* alloc descriptors */
	rx_ring->desc = dma_alloc_coherent(&pdev->dev, rx_ring->size,
					   &rx_ring->dma, GFP_KERNEL);
	if (unlikely(!rx_ring->desc)) {
		dev_err(&pdev->dev, "failed to dma alloc desc ring\n");
		ret = -ENOMEM;
		goto err;
	}


	/* alloc buffer_info */
	rx_ring->buff_info = vmalloc(BUFF_INFO_SIZE * DESC_CNT);
	if (unlikely(!rx_ring->buff_info)) {
		dev_err(&pdev->dev, "failed to alloc rx buff info %d\n", i);
		ret = -ENOMEM;
		goto err_buff_info;
	}

	/* alloc buffers */
	for (i=0, k=0; i < DESC_CNT; ++i, ++k) {
		rx_ring->buffer[i] = kzalloc(RX_BUF_SIZE, GFP_KERNEL);
		if (unlikely(!rx_ring->buffer[i])) {
			dev_err(&pdev->dev, "failed to alloc rx buff %d\n", i);
			ret = -ENOMEM;
			goto err_buff_alloc;
		}

		rx_ring->buff_info[i].size = RX_BUF_SIZE;
	}

	/* pin buffers */
	for (i=0, k=0; i < DESC_CNT; ++i, ++k) {
		buf_info = &rx_ring->buff_info[i];
		buf_info->dma = dma_map_single(&pdev->dev, rx_ring->buffer[i],
					       RX_BUF_SIZE, DMA_FROM_DEVICE);
		if (unlikely(!buf_info->dma)) {
			dev_err(&pdev->dev, "failed to pin buffer %d\n", i);
			ret = -ENOMEM; /* TODO: find a better errno code */
			goto err_buffer_pin;
		}
	}

	rx_ring->next_to_use = 0;
	rx_ring->next_to_clean = 0;

	/* fill descriptor ring with buffer addresses */
	for (i=0; i < DESC_CNT; ++i) {
		desc = GET_RX_DESC(rx_ring, i);
		buf_info = &rx_ring->buff_info[i];

		desc->buf_addr = cpu_to_le64(buf_info->dma);
	}

	return SUCCESS;

err_buffer_pin:
	for (i=0; i < k; ++i) {
		buf_info = &rx_ring->buff_info[i];
		dma_unmap_single(&pdev->dev, buf_info->dma, buf_info->size,
				 DMA_FROM_DEVICE);
	}

	k = DESC_CNT;

err_buff_alloc:
	for (i=0; i < k; ++i)
		kfree(rx_ring->buffer[i]);

	vfree(rx_ring->buff_info);

err_buff_info:
	dma_free_coherent(&dev->pdev->dev, rx_ring->size, rx_ring->desc,
			  rx_ring->dma);
err:
	return ret;
}

/*
 * i82540EM_clear_rx_ring - unpins and frees contents in descriptor ring and
 *			    sets variables to 0.
 */
static void i82540EM_clear_rx_ring(struct i82540EM_dev *dev)
{
	int i;
	struct pci_dev *pdev = dev->pdev;
	struct i82540EM_rx_ring *rx_ring = &dev->rx_ring;
	struct i82540EM_rx_buff_info *buff_info;

	for (i=0; i < DESC_CNT; ++i) {
		buff_info = &rx_ring->buff_info[i];
		dma_unmap_single(&pdev->dev, buff_info->dma, buff_info->size,
				 DMA_FROM_DEVICE);
		kfree(rx_ring->buffer[i]);
	}


	dma_free_coherent(&pdev->dev, rx_ring->size, rx_ring->desc,
			  rx_ring->dma);

	vfree(rx_ring->buff_info);

	rx_ring->dma  = 0;
	rx_ring->size = 0;
	rx_ring->next_to_use   = 0;
	rx_ring->next_to_clean = 0;
}

/*
 * i82540EM_reset_device - sets the RST bit and resets the device.
 *			   sleeps after reset to ensure it has time to reset.
 */
static void i82540EM_reset_device(struct i82540EM_dev *dev)
{
	u32 result;
	u32 __iomem *workreg;
	u32 __iomem *bar = dev->hwbar;

	workreg = HWREG_U32(bar, DEV_CTRL);
	result = ioread32(workreg);

	result |= CTRL_RST;
	iowrite32(result, workreg);

	/* wait until RST bit is reset */
	while (ioread32(workreg) & CTRL_RST)
		msleep(RST_SLEEP_DELAY);

	dev_info(&dev->pdev->dev, "device reset complete.\n");
}


/*
 * i82540EM_set_gen_config - sets the configuation of the NIC to the general
 *			     config (page 385 in manual)
 */
static void i82540EM_init_gen_config(struct i82540EM_dev *dev)
{
	u32 result;
	u32 __iomem *workreg;
	u32 __iomem *bar = dev->hwbar;

	/* work on device control reg */
	workreg = HWREG_U32(bar, DEV_CTRL);
	result = ioread32(workreg);

	dev_info(&dev->pdev->dev,
		 "initial state of device control, 0x%08X\n", result);

	result |= CTRL_SLU;	   /* enable set link up */
	result &= ~(CTRL_PHY_RST); /* disable PHY_RST */
	result &= ~(CTRL_ILOS);    /* disable invert loss of signal */
	result &= ~(CTRL_SPEED);   /* clear speed bits */
	result |= THOUS_MBS;	   /* set speed to 1000 MB/s */
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

	WRITE_FLUSH(dev);

	dev_info(&dev->pdev->dev, "latest result sent to DEV_CTRL. 0x%08X\n",
		 result);
}

/*
 * i82540EM_init_mult_table - initializes the multicast table array to 0 in
 *			      each vector.
 */
static void i82540EM_init_mult_table(struct i82540EM_dev *dev)
{
	u32 __iomem *mult_table = HWREG_U32(dev->hwbar, MTA);
	int i;

	/* TODO: make sure indexing like this works as intended */
	for (i=0; i < MTA_NUM_VECTS; ++i)
		iowrite32(MTA_NUM_VECTS, &mult_table[i]);
}

/*
 * i82540EM_init_rxctrl_config - sets up the initial state for the reveive
 *				 control register.
 *
 * Should be called after software has set up the descriptor rings and software
 * is ready to process.
 */
static void i82540EM_init_rxctrl_config(struct i82540EM_dev *dev)
{
	u32 __iomem *workreg = HWREG_U32(dev->hwbar, RCTRL);
	u32 reg_val = ioread32(workreg);

	reg_val &= ~(LPE); /* Disable long packet enable */
	reg_val &= ~(LBM); /* disable loopback mode */

	reg_val &= ~(RDMTS);
	reg_val |= RDMTS_INIT; /* set receive desc min thresh */

	reg_val &= ~(MO); /* disable  multicast offset */
	reg_val |= BAM;   /* enable broadcast accept mode */

	reg_val &= ~(BSIZE_BITS);
	reg_val |= BSIZE_INIT; /* set receive buffer size */

	reg_val &= ~(SECRC); /* Disbale stripping CTC */

	reg_val |= UPE; /* unicast promiscuous mode */
	reg_val |= MPE; /* multicast promiscuous mode */

	iowrite32(reg_val, workreg);

	reg_val |= RCTRL_EN; /* Rx enabled */

	iowrite32(reg_val, workreg);

	WRITE_FLUSH(dev);
}

/*
 * i82540EM_IMC_diable_ints - sets the interrupt mask clear register to disable
 *			      all interrupts on device
 */
static void i82540EM_IMC_disable_ints(struct i82540EM_dev *dev)
{
	u32 __iomem *workreg = HWREG_U32(dev->hwbar, IMC);
	iowrite32(DISABLE_ALL_INTS, workreg);
	WRITE_FLUSH(dev);
}

/*
 * i82540EM_enable_dev_ints - enables the interrupts on the device via its
 *			      interrupt mask set register
 */
static void i82540EM_enable_dev_ints(struct i82540EM_dev *dev)
{
	u32 __iomem *workreg = HWREG_U32(dev->hwbar, IMS);
	u32 reg_val = ioread32(workreg);

	reg_val |= RXDMT0;
	iowrite32(reg_val, workreg);
	WRITE_FLUSH(dev);
}

/*
 * i82540EM_rx_irq_handler - handles the interupts set for RX
 */
static irqreturn_t i82540EM_rx_irq_handler(int irq, void *data)
{
	struct i82540EM_dev *dev = data;
	u32 __iomem *workreg = HWREG_U32(dev->hwbar, ICR);
	u32 cause;

	cause = ioread32(workreg);
	if (cause & RXDMT0) {
		i82540EM_IMC_disable_ints(dev);
		i82540EM_set_led(dev, LED2_NUM, LED_ON, BLINK_OFF);
		i82540EM_set_led(dev, LED3_NUM, LED_ON, BLINK_OFF);
		schedule_work(&dev->rx_work);
	} else {
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

#ifdef _REMOVE
/* EEPROM definitions */
#define EERD	     0x00014	/* EEPROM read register */
#define EE_STRT	     0x00000001 /* EEPROM start bit used to initiate read */
#define EE_DONE	     0x00000010 /* EEPROM done, set to 1 when no read in prog */
#define EE_MAC_ADDRL 0x00000000 /* MAC addr location for first 2 bytes */
#define EE_MAC_ADDRH 0x00000200 /* MAC addr location for last byte shifted */
#define EE_ADDR(addr) ((addr) << 8) /* shifts addr to addr cell location */

#define RAL0 0x05400 /* reg0 of read address low, holds MAC addr low 32bit */
#define RAH0 0x05404 /* reg0 of read address high, holds MAC addr high 16bit */
#endif

static u32 i82540EM_get_eeprom_data(u32 addr)
{
	u32 data = 0;

	return data;
}

static void i82540EM_set_mac_addr(struct i82540EM_dev *dev)
{
	u32 __iomem *workreg;
	u32 reg_val;
	u32 eeprom_data;

	/* setup address in EEPROM read register */

	/* enable read */

	/* poll till read is complete */

	/* get the two 16bit words from data */

	/* place first two words in RAL0 */

	/* setup next address in EEPROM read register */

	/* enable read */

	/* poll till read is complete */

	/* get the first 16bit word from the data received */

	/* place 16bit word into RAH0 16bit feild for last 16bits of mac addr */

	/* write flush */
}


/* i82540_set_rx_init_config - sets the configuration for receiving based on
 *			       the user manual chapter 14.4.
 */
static void i82540EM_set_rx_init_config(struct i82540EM_dev *dev)
{
	void __iomem *hwbar = dev->hwbar;
	struct i82540EM_rx_ring *rx_ring = &dev->rx_ring;
	u32 __iomem *workreg;
	u32 reg_val;

	/* program receive address registers with desired ethernet addresses */



	/* Initialize multicast table array */
	i82540EM_init_mult_table(dev);

	/* program interrupt masks */
	i82540EM_enable_dev_ints(dev);

	/* set descriptor base addresses */
	workreg = HWREG_U32(hwbar, RDBAL);
	reg_val = GET_LOWER_32(rx_ring->dma);
	iowrite32(reg_val, workreg);

	workreg = HWREG_U32(hwbar, RDBAH);
	reg_val = GET_HIGHER_32(rx_ring->dma);
	iowrite32(reg_val, workreg);

	/* set descriptor length */
	workreg = HWREG_U32(hwbar, RDLEN);
	iowrite32(RDLEN_VAL, workreg);

	/* initialize head and tail of descriptor ring */
	workreg = HWREG_U32(hwbar, RDH);
	iowrite32(rx_ring->next_to_use, workreg);

	workreg = HWREG_U32(hwbar, RDT);
	iowrite32(rx_ring->next_to_clean, workreg);

	/* init the control register and enable rx, does WRITE_FLUSH at end */
	i82540EM_init_rxctrl_config(dev);
}

static void i82540EM_rx_work_service(struct work_struct *work)
{
	struct i82540EM_dev *dev = container_of(work,
					        struct i82540EM_dev,
						rx_work);
	struct i82540EM_rx_ring *rx_ring = &dev->rx_ring;
	struct i82540EM_rx_desc *desc;
	u32 __iomem *workreg = HWREG_U32(dev->hwbar, RDT);
	struct mutex mlock;

	mutex_init(&mlock);
	mutex_lock(&mlock);

	msleep(WORK_RX_DELAY);

	/* turn off LED[2:3] */
	i82540EM_set_led(dev, LED2_NUM, LED_ON, BLINK_OFF);
	i82540EM_set_led(dev, LED3_NUM, LED_ON, BLINK_OFF);

	do {
		desc = GET_RX_DESC(rx_ring, rx_ring->next_to_clean);
		if (desc->upper.fields.status & DESC_STAT_DD) {
			/* clean descriptor */
			desc->upper.fields.status = 0;

			++(rx_ring->next_to_clean);
			if (rx_ring->next_to_clean > DESC_CNT)
				rx_ring->next_to_clean = 0;
		}

		/*
		 * keeping track of head without register since register says
		 * its not reliable to use the head register for head
		 * placement
		 */
		++(rx_ring->next_to_use);
		if (rx_ring->next_to_use > DESC_CNT)
			rx_ring->next_to_use = 0;

		dev_info(&dev->pdev->dev, "tail after cleaning: %d\n",
			 rx_ring->next_to_clean);
	} while (rx_ring->next_to_clean <= rx_ring->next_to_use);

	--(rx_ring->next_to_use); /* taila/head should be equal post clean */

	iowrite32(rx_ring->next_to_clean, workreg); /* bump tail */

	mutex_unlock(&mlock);

	i82540EM_enable_dev_ints(dev);
}

static int i82540EM_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct i82540EM_dev *dev;
	int ret;
	u32 bars;
	resource_size_t mmio_start;
	resource_size_t mmio_len;

	dev_info(&pdev->dev, ": probe starting...\n");

	dev = kzalloc(sizeof(struct i82540EM_dev), GFP_KERNEL);
	if (unlikely(!dev)) {
		ret = -ENOMEM;
		goto err_dev_alloc;
	}

	ret = alloc_chrdev_region(&dev->devnode, MINOR_STRT, DEVCNT, DEV_NAME);
	if (unlikely(ret)) {
		dev_err(&pdev->dev, "alloc_chrdev_region() failed. error %d\n",
		        ret);
		goto err_alloc_chrdev_region;
	}

	dev_info(&pdev->dev, "Allocated %d devices at major %d\n",
		 DEVCNT, MAJOR(dev->devnode));

	/* init the character device and add it to the kernel */
	cdev_init(&dev->cdev, &dev_fops);
	dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&dev->cdev, dev->devnode, DEVCNT);
	if (unlikely(ret)) {
		dev_err(&pdev->dev, "cdev_add() failed. error %d\n", ret);
		goto err_cdev_add;
	}

	/* place module in /dev with class and device, requires a GPL license */
	dev->class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(dev->class)) {
		dev_err(&pdev->dev, "class_create() failed. error %d\n", ret);
		ret = PTR_ERR(dev->class);
		goto err_class_create;
	}

	dev->dev = device_create(dev->class, NULL, dev->devnode,
				       NULL, DEV_NAME);
	if (IS_ERR(dev->dev)) {
		dev_err(&pdev->dev,"device_create() failed. error %d\n", ret);
		ret = PTR_ERR(dev->dev);
		goto err_device_create;
	}

	dev_info(&pdev->dev, "device created in /dev\n");

	ret = pci_enable_device_mem(pdev);
	if (unlikely(ret)) {
		dev_err(&pdev->dev, "pci_enable_device_mem failed. error %d\n",
		        ret);
		goto err_pci_enable_device_mem;
	}

	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	ret = pci_request_selected_regions(pdev, bars, DEV_NAME);
	if (unlikely(ret)) {
		dev_err(&pdev->dev,
		        "pci_requiest_selected_regions() failed, error %d\n",
			ret);
		goto err_pci_request_selected_regions;
	}

	pci_set_master(pdev);

	/* save our driver struct */
	dev->pdev = pdev;
	pci_set_drvdata(pdev, dev);

	mmio_start = pci_resource_start(pdev, MM_BAR0);
	mmio_len = pci_resource_len(pdev, MM_BAR0);

	dev->hwbar = ioremap(mmio_start, mmio_len);
	if (unlikely(!dev->hwbar)) {
		ret = -EIO;
		dev_err(&pdev->dev, "ioremap() failed, error %d\n", ret);
		goto err_ioremap_bar;
	}

	i82540EM_reset_device(dev);
	i82540EM_init_gen_config(dev);

	/* initialize LEDS to off */
	i82540EM_set_all_leds(dev, LED_OFF, BLINK_OFF);

	/* set timer for blink */
	timer_setup(&dev->blink_timer, i82540EM_blink_timer_cb, NO_FLAGS);

	/* setup rx ring */
	ret = i82540EM_init_rx_ring(dev);
	if (unlikely(ret)) {
		dev_err(&pdev->dev, "Failed to init rx ring\n");
		goto err_init_rx_ring;
	}

	INIT_WORK(&dev->rx_work, i82540EM_rx_work_service);

	/* initialize interrupts as disabled */
	i82540EM_IMC_disable_ints(dev);

	/* set interrupts in the kernel */
	ret = request_irq(pdev->irq, i82540EM_rx_irq_handler,
			  IRQF_SHARED, "i82540EM_rx", dev);
	if (unlikely(ret)) {
		dev_err(&pdev->dev, "failed to request rx irq\n");
		goto err_request_irq;
	}

	/* set rx config, enables dev register ints, enables RX at the end */
	i82540EM_set_rx_init_config(dev);

	dev_info(&pdev->dev, "probe complete\n");

	return SUCCESS;

err_request_irq:
	i82540EM_IMC_disable_ints(dev);
	i82540EM_clear_rx_ring(dev);
	cancel_work_sync(&dev->rx_work);
err_init_rx_ring:
	del_timer_sync(&dev->blink_timer);
	iounmap(dev->hwbar);
err_ioremap_bar:
	/* pci_release_selected_regions(pdev, bars); */
err_pci_request_selected_regions:
	pci_release_mem_regions(pdev);
err_pci_enable_device_mem:
	pci_disable_device(pdev);
	device_destroy(dev->class, dev->devnode);
err_device_create:
	class_destroy(dev->class);
err_class_create:
	cdev_del(&dev->cdev);
err_cdev_add:
	unregister_chrdev_region(dev->devnode, DEVCNT);
err_alloc_chrdev_region:
	kfree(dev);
err_dev_alloc:
	return ret;
}

static void i82540EM_remove(struct pci_dev *pdev)
{
	struct i82540EM_dev *dev;
	u32 bars;

	dev = pci_get_drvdata(pdev);

	/* disable IRQ */
	i82540EM_IMC_disable_ints(dev);
	free_irq(pdev->irq, dev);

	cancel_work_sync(&dev->rx_work);

	i82540EM_clear_rx_ring(dev);

	del_timer_sync(&dev->blink_timer);

	/* pci */
	iounmap(dev->hwbar);
	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	pci_release_selected_regions(pdev, bars);
	/* pci_release_mem_regions(pdev); Frees nonexistant resource */
	pci_disable_device(pdev);

	/* cdev, class, device */
	device_destroy(dev->class, dev->devnode);
	class_destroy(dev->class);
	cdev_del(&dev->cdev);
	unregister_chrdev_region(dev->devnode, DEVCNT);
	kfree(dev);
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
MODULE_VERSION("0.5");
module_init(i82540EM_init);
module_exit(i82540EM_exit);
