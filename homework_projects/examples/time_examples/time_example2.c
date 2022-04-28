#include <linux/module.h>
#include <linux/types.h>
#include <linux/time.h>


static struct my_local_struct {
	int foo;
	unsigned long jiff;
	struct timer_list example_timer;
} my_str;

static void example_timer_cb(struct timer_list *t)
{
	struct my_local_struct *val = from_timer(val, t, example_timer);

	printk(KERN_INFO "foo=%u elapsed time = %lu\n",
		val->foo, (jiffies - val->jiff));

	/* now mess with the data */
	val->foo++;
	val->jiff = jiffies;

	/* and restart the timer */
	mod_timer(&my_str.example_timer, (jiffies + (val->foo * HZ)));
}

static int __init time_example2_init(void)
{
	printk(KERN_INFO "%s started, HZ=%d\n", __func__, HZ);

	my_str.foo = 1;
	my_str.jiff = jiffies;

	timer_setup(&my_str.example_timer, example_timer_cb, 0);

	mod_timer(&my_str.example_timer, (jiffies + (1 * HZ)));

	return 0;
}

static void __exit time_example2_exit(void)
{
	del_timer_sync(&my_str.example_timer);
	printk(KERN_INFO "unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PJ Waskiewicz");
MODULE_VERSION("0.2");

module_init(time_example2_init);
module_exit(time_example2_exit);
