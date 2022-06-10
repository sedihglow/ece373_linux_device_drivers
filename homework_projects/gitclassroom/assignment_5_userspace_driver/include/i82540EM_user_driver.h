#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pci/pci.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <time.h>

#include "err_handle.h"
#include "convNum.h"

/* good packets received count register */
#define GPRC 0x04074

/* LED register definitions */
#define LEDCTRL 0x00E00 /* led control register offset */

/* led number for set_led */
#define LED0 0
#define LED1 1
#define LED2 2
#define LED3 3

/* passed to set_led functions */
#define LED_ON  true
#define LED_OFF false

#define LED0_MODE_MASK 0x0000000F
#define LED1_MODE_MASK 0x00000F00
#define LED2_MODE_MASK 0x000F0000
#define LED3_MODE_MASK 0x0F000000

#define LED0_MODE_ON 0x0000000E
#define LED1_MODE_ON 0x00000E00
#define LED2_MODE_ON 0x000E0000
#define LED3_MODE_ON 0x0E000000

#define LED0_MODE_OFF 0x0000000F
#define LED1_MODE_OFF 0x00000F00
#define LED2_MODE_OFF 0x000F0000
#define LED3_MODE_OFF 0x0F000000

/* definitions passed to clear_pci_info for unmapping mem option */
#define MEM_UNMAP    true
#define MEM_NO_UNMAP false

#define DEV_ID 0x100e /* intel 82540EM device ID */

struct pci_info {
	volatile void *mem_addr;
	char *portname;
	char *pci_bus_slot;

	struct pci_dev *dev;
	struct pci_access *pacc;
	struct pci_filter filter;
};

void set_verbose(bool state);
bool get_verbose(void);
void print_verbose(char *fmt, ...);

u32 mem_read32(volatile void *mem_addr, u32 reg);
void mem_write32(volatile void *mem_addr, u32 reg, u32 val);

int open_dev(struct pci_info *pci_info, off_t base_addr);
void close_dev(struct pci_info *pci_info, int fd);

/* gets called in close_dev */
void clear_pci_info(struct pci_info *pci_info, bool unmap);

int check_port_exists(struct pci_info *pci_info);
bool pci_device_exists(char *pci_bus_slot, char *pci_entry, size_t pe_len);

off_t get_bar_addr(struct pci_info *pci_info, char *pci_entry);

void clear_stdin();
bool get_user_exit();

int set_led(struct pci_info *pci_info, int led_num, bool state);
void set_all_leds(struct pci_info *pci_info, bool state);
void led_blink_sequence(struct pci_info *pci_info);


u32 get_good_packets_received(struct pci_info *pci_info);
