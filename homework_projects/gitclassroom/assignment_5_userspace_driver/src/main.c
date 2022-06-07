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
	off_t base_addr;
	int mem_fd;
	int opt;
	char pci_entry[PCI_ENTRY_SIZE] = {0};
	u32 gpr; /* good packets reveived */
	bool exit_flag;

	if (getuid() != ROOT_USER)
		errExit("%s: Must run as root.");

	if (argc != MIN_ARGC && argc != MAX_ARGC) {
		errExit("Must have all arguments other than -v.\n"
			"Usage: %s -s <bus:slot.func> -n <ethX> -b <blink_rate>"
			" -v", argv[0]);
	}

	while ((opt = getopt(argc, argv, "s:n:b:v")) != -1) {
		switch (opt) {
		case 's':
			pci_info.pci_bus_slot = optarg;
			break;
		case 'n':
			pci_info.portname = optarg;
			/*
			 * check if exists
			 * TODO: See if there is a better way with pci/pci.h
			 */
			check_port_exists(&pci_info);
			if (errno)
				errExit("Checking portname failed.");
			break;
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
				"Usage: %s -s <bus:slot.func> -n <ethX> "
				"-b <blink_rate> -v",
				argv[0]);
			break;
		}
	}

	print_verbose("bus:slot.func = %s\n"
		      "portname = %s\n"
		      "verbose = %s\n",
		      pci_info.pci_bus_slot, pci_info.portname,
		      (get_verbose() ? "True" : "False"));

	/* fills pci_entry */
	if (!pci_device_exists(pci_info.pci_bus_slot,
			       pci_entry, PCI_ENTRY_SIZE)) {
		if (errno)
			errExit("pci_device_exists failed.");

		noerrExit("%s pci device does not exist",
			  pci_info.pci_bus_slot);
	}

	base_addr = get_bar_addr(&pci_info, pci_entry);
	if (errno)
		errExit("Failed to get base address.");

	print_verbose("base address: %X\n", base_addr);

	mem_fd = open_dev(&pci_info, base_addr);
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
