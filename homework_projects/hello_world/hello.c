/*
 * Hello Kernel
 * written by: James Ross
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define SUCCESS 0

static int __init hello_init(void)
{
	pr_info("Hello, kernel\n");
	return SUCCESS;
}

static void __exit hello_exit(void)
{
	pr_info("Goodbye, kernel\n");
}

MODULE_AUTHOR("James Ross");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
module_init(hello_init);
module_exit(hello_exit);
