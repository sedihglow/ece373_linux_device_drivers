#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <pci/pci.h>

int main(int argc, char **argv)
{
	struct pci_access *pacc;
	struct pci_filter filter;
	struct pci_dev *dev;
	volatile void *hw_addr;
	int fd;

	/* must run as root */
	if (getuid() != 0) {
		fprintf(stderr, "%s: must be run as root\n", argv[0]);
		exit(1);
	}

	/* get the pci_access structure to start digging into the bus */
	pacc = pci_alloc();
	if (pacc == NULL) {
		perror("pci_alloc");
		exit(1);
	}

	pci_filter_init(pacc, &filter);

	/* now check if our vendor/device ID are found on the bus */
	pci_init(pacc);        /* get underlying PCI accessors ready to go */
	pci_scan_bus(pacc);    /* get the list of all devices on the bus */

	filter.vendor = PCI_VENDOR_ID_INTEL; /* 0x8086 - pci.h include chain */
 	/* this could be put into a list to match on */
	filter.device = 0x100e; /* Intel 82540EM - e1000 device */

	/* now iterate and try to find our device */
	for (dev = pacc->devices; dev; dev = dev->next) {
		fprintf(stderr, "Trying to match %02x:%02x.%d (0x%04x:0x%04x)\n",
			dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id);
		if (pci_filter_match(&filter, dev))
			break;
	}

	if (dev == NULL) {
		fprintf(stderr, "Couldn't find device on bus\n");
		exit(1);
	}

	/* ask for more info from the bus */
	pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_SIZES);

	fprintf(stderr, "Found device match: %02x:%02x.%d (0x%04x:0x%04x)\n",
		dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id);
	fprintf(stderr, "\tBAR[0] addr: 0x%016lx, BAR[0] length: %d\n",
		dev->base_addr[0], dev->size[0]);

	fd = open("/dev/mem", O_RDWR);
	if (fd < 0) {
		perror("open(/dev/mem)");
		exit(1);
	}

	hw_addr = mmap(NULL, dev->size[0], PROT_READ | PROT_WRITE, MAP_SHARED,
		       fd, (dev->base_addr[0] & PCI_ADDR_MEM_MASK));
	if (hw_addr == MAP_FAILED) {
		perror("mmap - try rebooting with iomem=relaxed");
		exit(1);
	}

	fprintf(stderr, "CTRL: 0x%08x, LEDCTL: %08x\n",
		*((u32 *)(hw_addr + 0x0)), *((u32 *)(hw_addr + 0x00E0)));

	munmap((void *)hw_addr, dev->size[0]);
	close(fd);

	/* clean up, aisle wherever! */
	pci_cleanup(pacc);

	return 0;
}
