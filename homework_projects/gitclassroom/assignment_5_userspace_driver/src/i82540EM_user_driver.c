#include "i82540EM_user_driver.h"

#define HWREGU32(base, off) ((u32*)((char*)(base) + (off)))

#define SUCCESS 0
#define FAILURE -1

#define MEM_WINDOW_SZ  0x10000000

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
	u32 *mem_reg = HWREGU32(mem_addr, reg);
	u32 val = *mem_reg;
	return val;
}

void mem_write32(volatile void *mem_addr, u32 reg, u32 val)
{
	u32 *mem_reg = HWREGU32(mem_addr, reg);
	*mem_reg = val;
}


int open_dev(struct pci_info *pci_info, off_t base_addr)
{
	int fd;

	fd = open("/dev/mem", O_RDWR);
	if (errno) {
		err_msg("failed to open file descriptor.");
		return FAILURE;
	}

	pci_info->mem_addr = mmap(NULL, MEM_WINDOW_SZ, (PROT_READ|PROT_WRITE),
				  MAP_SHARED, fd, base_addr);
	if (pci_info->mem_addr == MAP_FAILED) {
		err_msg("mmap failed - try rebooting with iomem=relaxed");
		close(fd);
		return FAILURE;
	}

	return fd;
}

void clear_pci_info(struct pci_info *pci_info, bool unmap)
{
	if (unmap) {
		munmap((void *)pci_info->mem_addr, MEM_WINDOW_SZ);
		pci_info->mem_addr = NULL;
	}

	pci_info->portname     = NULL;
	pci_info->pci_bus_slot = NULL;
}

void close_dev(struct pci_info *pci_info, int fd)
{
	clear_pci_info(pci_info, MEM_UNMAP);
	close(fd);
}


int check_port_exists(struct pci_info *pci_info)
{
	char buf[BUF_SIZE] = {'\0'};
	FILE *input;

	snprintf(buf, BUF_SIZE, "ip -br link show %s", pci_info->portname);
	input = popen(buf, "r");
	if (errno) {
		err_msg("popen failed checking portname.");
		return FAILURE;
	} else if (!input) {
		errno = ENOMEM;
		err_msg("popen mem alloc failure.");
		return FAILURE;
	}

	fgets(buf, BUF_SIZE, input);
	fclose(input);
	if (strncmp(pci_info->portname, buf, strlen(pci_info->portname))) {
		errno = EINVAL;
		err_msg("%s not found\n", pci_info->portname);
		return FAILURE;
	}

	return SUCCESS;
}

bool pci_device_exists(char *pci_bus_slot, char *pci_entry, size_t pe_len)
{
	char buf[BUF_SIZE] = {'\0'};
	FILE *input;

	/* Does pci device specified by the user exist? */
	snprintf(buf, BUF_SIZE, "lspci -s %s", pci_bus_slot);
	input = popen(buf, "r");
	if (errno) {
		err_msg("popen failed checking portname.");
		return false;
	} else if (!input) {
		errno = ENOMEM;
		err_msg("popen mem alloc failure.");
		return false;
	}

	/* get the pci entry from input */
	fgets(pci_entry, pe_len, input);
	fclose(input);
	pe_len = strlen(buf);
	if (pe_len <= 1)
		return false;

	return true;
}

off_t get_bar_addr(struct pci_info *pci_info, char *pci_entry)
{
	char addr_str[ADDR_STR_SIZE] = {'\0'};
	char buf[BUF_SIZE] = {'\0'};
	off_t base_addr;
	size_t len;
	FILE *input;

	/* Let's make sure this is an Intel ethernet device.  A better
	 * way for doing this would be to look at the vendorId and the
	 * deviceId, but we're too lazy to find all the deviceIds that
	 * will work here, so we'll live a little dangerously and just
	 * be sure it is an Intel device according to the description.
	 * Oh, and this is exactly how programmers get into trouble.
	 */
	if (!strstr(pci_entry, "Ethernet controller") ||
	    !strstr(pci_entry, "Intel")) {
		errno = EINVAL;
		err_msg("%s wrong pci device, not intel\n", pci_entry);
		return FAILURE;
	}

	/* Only grab the first memory bar */
	snprintf(buf, BUF_SIZE,
		 "lspci -s %s -v | awk '/Memory at/ { print $3 }' | head -1",
		 pci_info->pci_bus_slot);
	input = popen(buf, "r");
	if (errno) {
		err_msg("popen failed checking pci_bus_slot.");
		return FAILURE;
	} else if (!input) {
		errno = ENOMEM;
		err_msg("popen mem alloc failure.");
		return FAILURE;
	}

	fgets(addr_str, ADDR_STR_SIZE, input);
	fclose(input);

	len = strlen(addr_str);
        if (len <= 1) {
		errno = EINVAL;
                err_msg("%s memory address invalid", addr_str);
		return FAILURE;
        }

	if (addr_str[len-1] == '\n')
		addr_str[len-1] = '\0';

	print_verbose("addr_str: %s\n", addr_str);

        base_addr = convU64_t(addr_str, CN_BASE_16 | CN_NOEXIT_, "addr_str");
	if (errno) {
		err_msg("failed to convert addr_str");
		return FAILURE;
	}

	return base_addr;
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
