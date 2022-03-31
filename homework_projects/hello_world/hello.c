/*
 * Hello Kernel
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define SUCCESS 0

MODULE_LICENSE("Dual BSD/GPL");

static int __init hello_init(void)
{
	pr_info("Hello, kernel\n");
	return SUCCESS;
}

static void __exit hello_exit(void)
{
	pr_info("Goodbye, kernel\n");
}

module_init(hello_init);
module_exit(hello_exit);

