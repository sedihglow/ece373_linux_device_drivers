#include <linux/module.h>
#include <linux/types.h>
#include <linux/time.h>
#include <asm/timex.h>
#include <linux/delay.h>

static int __init get_cycles_example_init(void)
{
	u32 c_start, c_done, c_duration;

	c_start = get_cycles();

	mdelay(2000);

	c_done = get_cycles();

	c_duration = c_done - c_start;

	printk(KERN_INFO "Cycles burned: %u\n", c_duration);

	return 0;
}

static void __exit get_cycles_example_exit(void)
{
	printk(KERN_INFO "unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PJ Waskiewicz");
MODULE_VERSION("0.1");

module_init(get_cycles_example_init);
module_exit(get_cycles_example_exit);
