#include <linux/module.h>
#include <linux/types.h>
#include <linux/time.h>

static int __init time_example1_init(void)
{
	unsigned long j_later = jiffies + (2 * HZ);

	printk(KERN_INFO "jiffies before: %lu\n", jiffies);

	while (time_before(jiffies, j_later))
		continue;

	printk(KERN_INFO "jiffies after: %lu\n", jiffies);

	return 0;
}

static void __exit time_example1_exit(void)
{
	printk(KERN_INFO "unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PJ Waskiewicz");
MODULE_VERSION("0.1");

module_init(time_example1_init);
module_exit(time_example1_exit);
