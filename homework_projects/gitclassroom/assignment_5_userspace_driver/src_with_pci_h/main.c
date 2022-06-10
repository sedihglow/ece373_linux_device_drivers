/*
 * ece373 assignment 5
 *
 * User space driver to manipulate LED's on the i82540EM intel ethernet card.
 *
 * written by: James Ross
 */

#include "i82540EM_user_driver.h"
#include <getopt.h>

#define ROOT_USER 0
#define REQ_OPTS  4

#define BUF_SIZE 128
#define PCI_ENTRY_SIZE 128 /* size for pci entry filled in pci_device_exists */

#define MIN_ARGC 5
#define MAX_ARGC 6

int main(int argc, char *argv[])
{
	struct pci_info pci_info;
	int mem_fd;
	int opt;
	u32 gpr; /* good packets reveived */
	bool exit_flag;

	if (getuid() != ROOT_USER)
		errExit("%s: Must run as root.");

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			/* make sure all arguments were passed with -v */
			if (argc == MIN_ARGC) {
				errExit("Must have all arguments.\n"
					"Usage: %s -s <bus:slot.func> "
					"-n <ethX> -b <blink_rate> -v",
					argv[0]);
			}

			set_verbose(true);
			break;
		default:
			errno = EINVAL;
			errExit("Invalid argument\n"
				"Usage: %s -v", argv[0]);
			break;
		}
	}

	print_verbose("verbose = %s\n", (get_verbose() ? "True" : "False"));

	setup_pci_data(&pci_info);
	if (errno)
		errExit("setup_pci_data failed: failed to alloc pci access.");

	find_pci_dev(&pci_info);
	if (errno)
		errExit("intel 82540EM not found.");

	get_more_dev_info(&pci_info);

	print_verbose("Found device match: %02x:%02x.%d (0x%04x:0x%04x)\n",
		      pci_info.dev->bus, pci_info.dev->dev, pci_info.dev->func,
		      pci_info.dev->vendor_id, pci_info.dev->device_id);
	print_verbose("\tBAR[0] addr: 0x%016lx, BAR[0] length: %d\n",
		      pci_info.dev->base_addr[BAR0], pci_info.dev->size[BAR0]);

	mem_fd = open_dev(&pci_info);
	if (errno)
		errExit("open_dev failed");

	do {
		printf("\n");

		led_blink_sequence(&pci_info);

		printf("\n");

		gpr = get_good_packets_received(&pci_info);

		printf("good packets received count register: %u\n", gpr);

		/* get user input to rerun or exit */
		exit_flag = get_user_exit();
	} while (!exit_flag);

	close_dev(&pci_info, mem_fd);
	exit(EXIT_SUCCESS);
}
