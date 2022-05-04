/*
 * PJ Waskiewicz * 5/1/2022
 *
 * Example PCI code to load and pass info through file_operations, using
 * a global struct definition
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/pci.h>

#define DEVNAME "ece_pci_example_static"
#define LEDCTL 0x00E00

/* Our private driver struct */
struct ece_pci_example_dev {
	struct pci_dev *pdev;
	dev_t ece_pci_example_node;
	struct cdev ece_pci_example_cdev;

	/* BAR addr */
	void __iomem *hw_addr;
} ece_dev;

/* Class for our auto-generated /dev entry */
static struct class *ece_pci_example_class;

/* Function prototypes */
int ece_pci_example_open(struct inode *inode, struct file *file);
int ece_pci_example_release(struct inode *inode, struct file *file);
ssize_t ece_pci_example_read(struct file *file, char __user *buf,
			     size_t len, loff_t *offset);

/* File operations for our system calls */
static struct file_operations ece_pci_example_fops = {
	.owner = THIS_MODULE,
	.open = ece_pci_example_open,
	.read = ece_pci_example_read,
	.release = ece_pci_example_release,
};

static struct pci_device_id ece_pci_example_pci_tbl[] = {
	{ PCI_DEVICE(0x8086, 0x100e) },
	{ } /* Terminating entry - this is required */
};
MODULE_DEVICE_TABLE(pci, ece_pci_example_pci_tbl);

int ece_pci_example_open(struct inode *inode, struct file *file)
{
	return 0;
}

int ece_pci_example_release(struct inode *inode, struct file *file)
{
	return 0;
}

ssize_t ece_pci_example_read(struct file *file, char __user *buf,
			     size_t len, loff_t *offset)
{
	u32 reg;

	/* verify the buffer we have is ok */
	if (!buf)
		return -EINVAL;

	reg = ioread32(ece_dev.hw_addr + LEDCTL);
	pr_info("In read, LEDCTL: 0x%08x\n", reg);

	iowrite32(0x4E, ece_dev.hw_addr + LEDCTL);

	reg = ioread32(ece_dev.hw_addr + LEDCTL);
	pr_info("In read, after write LEDCTL: 0x%08x\n", reg);
	return 4;
}

/* probe time */
static int ece_pci_example_probe(struct pci_dev *pdev,
				 const struct pci_device_id *ent)
{
	int err;
	resource_size_t mmio_start, mmio_len;

	pr_info("pci_example: Starting probe\n");
	err = pci_request_selected_regions(pdev,
	                                  pci_select_bars(pdev, IORESOURCE_MEM),
	                                  DEVNAME);
	if (err)
		goto err_pci_reg;

	/* enable bus mastering - needed for descriptor writebacks */
	pci_set_master(pdev);

	/* finish mapping the BAR */
	mmio_start = pci_resource_start(pdev, 0);
	mmio_len = pci_resource_len(pdev, 0);

	ece_dev.hw_addr = ioremap(mmio_start, mmio_len);
	if (!ece_dev.hw_addr)
		goto err_ioremap;

	pr_info("pci_example: end probe\n");

	/* all done! */
	return 0;

	/* error handling */
err_ioremap:
	pci_release_selected_regions(pdev,
	                             pci_select_bars(pdev, IORESOURCE_MEM));
err_pci_reg:
	return err;
}

/* remove */
static void ece_pci_example_remove(struct pci_dev *pdev)
{
	/* start tearing down */

	/* unmap the BAR */
	iounmap(ece_dev.hw_addr);
	pci_release_selected_regions(pdev,
	                             pci_select_bars(pdev, IORESOURCE_MEM));
}

/* PCI driver interface */
static struct pci_driver ece_pci_example_driver = {
	.name = DEVNAME,
	.id_table = ece_pci_example_pci_tbl,
	.probe = ece_pci_example_probe,
	.remove = ece_pci_example_remove,
};

static int __init ece_pci_example_driver_init(void)
{
	int err;

	pr_info("pci_example: loading module with init...\n");

	/* chardev creation and setup */
	err = alloc_chrdev_region(&ece_dev.ece_pci_example_node,
				  0, 1, DEVNAME);
	if (err) {
		pr_err("alloc_chrdev_region() failed: %d\n", err);
		goto err_alloc_chrdev;
	}

	cdev_init(&ece_dev.ece_pci_example_cdev, &ece_pci_example_fops);
	ece_dev.ece_pci_example_cdev.owner = THIS_MODULE;
	err = cdev_add(&ece_dev.ece_pci_example_cdev,
		       ece_dev.ece_pci_example_node, 1);
	if (err) {
		pr_err("cdev_init() failed: %d\n", err);
		goto err_cdev_init;
	}

	/* device class setup and device creation */
	ece_pci_example_class = class_create(THIS_MODULE, "ece_dev");
	device_create(ece_pci_example_class, NULL,
		      ece_dev.ece_pci_example_node, NULL, DEVNAME);
	/* Register the PCI device */
	err = pci_register_driver(&ece_pci_example_driver);
	if (err) {
		pr_err("pci_register_driver() failed: %d\n", err);
		goto err_pci_register;
	}

	pr_info("pci_example: end init\n");

	return 0;

err_pci_register:
	device_destroy(ece_pci_example_class, ece_dev.ece_pci_example_node);
	class_destroy(ece_pci_example_class);

	cdev_del(&ece_dev.ece_pci_example_cdev);
err_cdev_init:
	unregister_chrdev_region(ece_dev.ece_pci_example_node, 1);

err_alloc_chrdev:
	return err;
}

static void __exit ece_pci_example_driver_exit(void)
{
	/* unregister the PCI driver */
	pci_unregister_driver(&ece_pci_example_driver);

	/* destroy the dev entry */
	device_destroy(ece_pci_example_class, ece_dev.ece_pci_example_node);
	class_destroy(ece_pci_example_class);

	/* tear down the cdev */
	cdev_del(&ece_dev.ece_pci_example_cdev);
	unregister_chrdev_region(ece_dev.ece_pci_example_node, 1);

}

module_init(ece_pci_example_driver_init);
module_exit(ece_pci_example_driver_exit);

MODULE_AUTHOR("PJ Waskiewicz");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
