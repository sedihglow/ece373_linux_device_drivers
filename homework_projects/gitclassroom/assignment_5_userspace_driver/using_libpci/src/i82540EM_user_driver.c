#include "i82540EM_user_driver.h"

#define HWREGU32(base, off) ((u32*)((char*)(base) + (off)))

#define SUCCESS 0
#define FAILURE -1

#define BUF_SIZE 128
#define ADDR_STR_SIZE 10

/* definitions for user input in get_user_exit() */
#define USER_RERUN_LED_SEQ 1
#define USER_EXIT_PROG     2

/* delays for sleep */
#define ONE_SEC 1
#define TWO_SEC 2

#define BLINK_SEQ_LOOP 5 /* loop count in part of led_blink_sequence() */

static bool verbose = false;

void set_verbose(bool state)
{
	verbose = state;
}

bool get_verbose(void)
{
	return verbose;
}

void print_verbose(char *fmt, ...)
{
	va_list varg;

	va_start(varg, fmt);

	if (get_verbose())
		vprintf(fmt, varg);

	va_end(varg);
}

u32 mem_read32(volatile void *mem_addr, u32 reg)
{
	volatile u32 *mem_reg = HWREGU32(mem_addr, reg);
	u32 val = *mem_reg;
	return val;
}

void mem_write32(volatile void *mem_addr, u32 reg, u32 val)
{
	volatile u32 *mem_reg = HWREGU32(mem_addr, reg);
	*mem_reg = val;
}


int open_dev(struct pci_info *pci_info)
{
	int fd;
	struct pci_dev *dev = pci_info->dev;

	fd = open("/dev/mem", O_RDWR);
	if (errno) {
		err_msg("failed to open file descriptor.");
		return FAILURE;
	}

	pci_info->mem_addr = mmap(NULL, dev->size[BAR0],
				  PROT_READ | PROT_WRITE,
				  MAP_SHARED,
				  fd,
				  (dev->base_addr[BAR0] & PCI_ADDR_MEM_MASK));
	if (pci_info->mem_addr == MAP_FAILED) {
		err_msg("mmap failed - try rebooting with iomem=relaxed");
		close(fd);
		return FAILURE;
	}

	return fd;
}

void clear_pci_info(struct pci_info *pci_info, bool unmap)
{
	if (unmap)
		munmap((void *)pci_info->mem_addr, pci_info->dev->size[BAR0]);

	pci_cleanup(pci_info->pacc);

	pci_info->mem_addr = NULL;
	pci_info->dev	   = NULL;
}

void close_dev(struct pci_info *pci_info, int fd)
{
	clear_pci_info(pci_info, MEM_UNMAP);
	close(fd);
}

int setup_pci_data(struct pci_info *pci_info)
{
	/* setup pci structures to find our device */
	pci_info->pacc = pci_alloc();
	if (errno)
		return FAILURE;

	pci_filter_init(pci_info->pacc, &pci_info->filter);

	pci_init(pci_info->pacc); /* get underlying PCI accessors ready */
	pci_scan_bus(pci_info->pacc); /* get list of all devices on bus */

	/* 0x8086 - pci.h include chain */
	pci_info->filter.vendor = PCI_VENDOR_ID_INTEL;
	pci_info->filter.device = DEV_ID; /* Intel 82540EM - e1000 device */

	return SUCCESS;
}

int find_pci_dev(struct pci_info *pci_info)
{
	struct pci_dev *dev;

	/* iterate to try and find the pci device */
	for (pci_info->dev = pci_info->pacc->devices;
	     pci_info->dev; pci_info->dev = pci_info->dev->next) {
		dev = pci_info->dev;
		print_verbose("Trying to match %02x:%02x.%d (0x%04x:0x%04x)\n",
			      dev->bus, dev->dev, dev->func, dev->vendor_id,
			      dev->device_id);
		if (pci_filter_match(&pci_info->filter, dev))
			return SUCCESS;
	}

	errno = EINVAL; /* dev is NULL, device not found */
	return FAILURE;
}

void get_more_dev_info(struct pci_info *pci_info)
{
	pci_fill_info(pci_info->dev,
		      PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_SIZES);
}

void clear_stdin()
{
	char ch = '\0';
	while ((ch = getchar()) != '\n' && ch != EOF);
}

bool get_user_exit()
{
	char buf[BUF_SIZE] = {'\0'};
	size_t len;
	int in_val;

	do {
		printf("Would you like to exit or rerun the blink sequence?\n"
		       "1. rerun blink sequence.\n"
		       "2. exit.\n"
		       "enter number: ");
		fflush(stdout);

		fgets(buf, BUF_SIZE, stdin);
		if (buf[0] != '\0') {
			len = strlen(buf) - 1; /* set to last index */
			if (buf[len] == '\n')
				buf[len] = '\0';
			else
				clear_stdin();
		}

		in_val = convInt(buf, CN_BASE_10, "in_val");
	} while (in_val != USER_RERUN_LED_SEQ && in_val != USER_EXIT_PROG);

	return (in_val == USER_EXIT_PROG) ? true : false;
}

int set_led(struct pci_info *pci_info, int led_num, bool state)
{
	u32 reg_val;

	reg_val = mem_read32(pci_info->mem_addr, LEDCTRL);
	switch (led_num) {
	case LED0:
		reg_val &= ~(LED0_MODE_MASK);
		if (state)
			reg_val |= LED0_MODE_ON;
		else
			reg_val |= LED0_MODE_OFF;
	break;
	case LED1:
		reg_val &= ~(LED1_MODE_MASK);
		if (state)
			reg_val |= LED1_MODE_ON;
		else
			reg_val |= LED1_MODE_OFF;
	break;
	case LED2:
		reg_val &= ~(LED2_MODE_MASK);
		if (state)
			reg_val |= LED2_MODE_ON;
		else
			reg_val |= LED2_MODE_OFF;
	break;
	case LED3:
		reg_val &= ~(LED3_MODE_MASK);
		if (state)
			reg_val |= LED3_MODE_ON;
		else
			reg_val |= LED3_MODE_OFF;
	break;
	default:
		errnum_msg(EINVAL, "set_led: Invalid led num");
		return FAILURE;
	break;
	}

	mem_write32(pci_info->mem_addr, LEDCTRL, reg_val);
	return SUCCESS;
}

void set_all_leds(struct pci_info *pci_info, bool state)
{
	set_led(pci_info, LED0, state);
	set_led(pci_info, LED1, state);
	set_led(pci_info, LED2, state);
	set_led(pci_info, LED3, state);
}


void led_blink_sequence(struct pci_info *pci_info)
{
	volatile void *mem_addr = pci_info->mem_addr;
	u32 led_ctrl_init = mem_read32(mem_addr, LEDCTRL);
	u32 reg_val;
	int i;

	printf("initial LED control register state: %08X\n", led_ctrl_init);

	/* turn on LED2 and LED0 on for 2 seconds */
	set_led(pci_info, LED0, LED_ON);
	set_led(pci_info, LED2, LED_ON);
	sleep(TWO_SEC);

	reg_val = mem_read32(mem_addr, LEDCTRL);
	printf("LED0 and LED2 on, reg val = %08X\n", reg_val);

	/* turn all LEDs off for 2 second */
	set_all_leds(pci_info, LED_OFF);
	sleep(TWO_SEC);

	reg_val = mem_read32(mem_addr, LEDCTRL);
	printf("All LEDs off, reg val = %08X\n", reg_val);

	/* loop 5 times and turn on all LEDs on for 1 second */
	for (i=0; i < BLINK_SEQ_LOOP; ++i) {
		set_all_leds(pci_info, LED_ON);

		reg_val = mem_read32(mem_addr, LEDCTRL);
		printf("All LEDs on, reg val = %08X\n", reg_val);

		sleep(ONE_SEC);

		set_all_leds(pci_info, LED_OFF);

		reg_val = mem_read32(mem_addr, LEDCTRL);
		printf("All LEDs off, reg val = %08X\n", reg_val);

		sleep(ONE_SEC);
	}

	/* restore LED control reg to initial state */
	mem_write32(mem_addr, LEDCTRL, led_ctrl_init);

	reg_val = mem_read32(mem_addr, LEDCTRL);
	printf("Setting LED reg back to init state, reg val = %08X\n", reg_val);
}

u32 get_good_packets_received(struct pci_info *pci_info)
{
	return mem_read32(pci_info->mem_addr, GPRC);
}
