#include <linux/module.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/delay.h>

static int __init delay_example_init(void)
{
	printk(KERN_INFO "before mdelay: %lu\n", jiffies);
	mdelay(1000);
	printk(KERN_INFO "after mdelay: %lu\n", jiffies);

	printk(KERN_INFO "before udelay: %lu\n", jiffies);
	udelay(1000);
	printk(KERN_INFO "after udelay: %lu\n", jiffies);

	printk(KERN_INFO "before ndelay: %lu\n", jiffies);
	ndelay(1000);
	printk(KERN_INFO "after ndelay: %lu\n", jiffies);

	return 0;
}

static void __exit delay_example_exit(void)
{
	printk(KERN_INFO "unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PJ Waskiewicz");
MODULE_VERSION("0.1");

module_init(delay_example_init);
module_exit(delay_example_exit);
