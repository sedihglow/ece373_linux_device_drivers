/*
 * PJ Waskiewicz
 * 5/1/2022
 *
 * Example PCI code to load and pass info through file_operations
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

#define DEVNAME "ece_pci_example"
#define LEDCTL 0x00E00

/* Our private driver struct */
struct ece_pci_example_dev {
	struct pci_dev *pdev;
	dev_t ece_pci_example_node;
	struct cdev ece_pci_example_cdev;

	/* BAR addr */
	void __iomem *hw_addr;
};

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
	/*
	 * Only purpose is to save off our private data structure for
	 * the other system calls to use.  Prevents the need for ugly globals.
	 */
	struct ece_pci_example_dev *ece_dev;

	ece_dev = container_of(inode->i_cdev,
			       struct ece_pci_example_dev,
			       ece_pci_example_cdev);
	file->private_data = ece_dev;

	return 0;
}

int ece_pci_example_release(struct inode *inode, struct file *file)
{
	/* clear the private_data member just to be safe */
	file->private_data = NULL;

	return 0;
}

ssize_t ece_pci_example_read(struct file *file, char __user *buf,
			     size_t len, loff_t *offset)
{
	struct ece_pci_example_dev *ece_dev = file->private_data;
	u32 reg;

	/* verify the buffer we have is ok */
	if (!buf)
		return -EINVAL;

	reg = ioread32(ece_dev->hw_addr + LEDCTL);
	pr_info("In read, LEDCTL: 0x%08x\n", reg);
	return 0;
}

/* probe time */
static int ece_pci_example_probe(struct pci_dev *pdev,
				 const struct pci_device_id *ent)
{
	int err;
	struct ece_pci_example_dev *ece_dev;
	resource_size_t mmio_start, mmio_len;

	/* create our driver struct */
	ece_dev = kzalloc(sizeof(struct ece_pci_example_dev), GFP_KERNEL);
	if (!ece_dev) {
		err = -ENOMEM;
		goto err_alloc_dev;
	}

	/* Do the chardev setup in probe so we don't need globals */

	/* chardev creation and setup */
	err = alloc_chrdev_region(&ece_dev->ece_pci_example_node,
				  0, 1, DEVNAME);
	if (err) {
		dev_err(&pdev->dev, "alloc_chrdev_region() failed: %d\n", err);
		goto err_alloc_chardev;
	}

	cdev_init(&ece_dev->ece_pci_example_cdev, &ece_pci_example_fops);
	ece_dev->ece_pci_example_cdev.owner = THIS_MODULE;
	err = cdev_add(&ece_dev->ece_pci_example_cdev,
		       ece_dev->ece_pci_example_node, 1);
	if (err) {
		dev_err(&pdev->dev, "cdev_init() failed: %d\n", err);
		goto err_cdev_init;
	}

	/* device class setup and device creation */
	ece_pci_example_class = class_create(THIS_MODULE, "ece_dev");
	device_create(ece_pci_example_class, NULL,
		      ece_dev->ece_pci_example_node, NULL, DEVNAME);

	err = pci_request_selected_regions(pdev,
	                                  pci_select_bars(pdev, IORESOURCE_MEM),
	                                  DEVNAME);
	if (err)
		goto err_pci_reg;

	/* enable bus mastering - needed for descriptor writebacks */
	pci_set_master(pdev);

	/* save off our driver struct for later */
	ece_dev->pdev = pdev;
	pci_set_drvdata(pdev, ece_dev);

	/* finish mapping the BAR */
	mmio_start = pci_resource_start(pdev, 0);
	mmio_len = pci_resource_len(pdev, 0);

	ece_dev->hw_addr = ioremap(mmio_start, mmio_len);
	if (!ece_dev->hw_addr)
		goto err_ioremap;

	/* all done! */
	return 0;

	/* error handling */
err_ioremap:
	pci_release_selected_regions(pdev,
	                             pci_select_bars(pdev, IORESOURCE_MEM));
err_pci_reg:
	/* destroy the dev entry */
	device_destroy(ece_pci_example_class, ece_dev->ece_pci_example_node);
	class_destroy(ece_pci_example_class);
err_cdev_init:
	unregister_chrdev_region(ece_dev->ece_pci_example_node, 1);
err_alloc_chardev:
	kfree(ece_dev);
err_alloc_dev:
	return err;
}

/* remove */
static void ece_pci_example_remove(struct pci_dev *pdev)
{
	struct ece_pci_example_dev *ece_dev;

	ece_dev = pci_get_drvdata(pdev);

	/* start tearing down */

	/* destroy the dev entry */
	device_destroy(ece_pci_example_class, ece_dev->ece_pci_example_node);
	class_destroy(ece_pci_example_class);

	/* tear down the cdev */
	cdev_del(&ece_dev->ece_pci_example_cdev);
	unregister_chrdev_region(ece_dev->ece_pci_example_node, 1);

	/* unmap the BAR */
	iounmap(ece_dev->hw_addr);
	kfree(ece_dev);
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
	/* Register the PCI device */
	return pci_register_driver(&ece_pci_example_driver);
}

static void __exit ece_pci_example_driver_exit(void)
{
	/* unregister the PCI driver */
	pci_unregister_driver(&ece_pci_example_driver);
}

module_init(ece_pci_example_driver_init);
module_exit(ece_pci_example_driver_exit);

MODULE_AUTHOR("PJ Waskiewicz");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
