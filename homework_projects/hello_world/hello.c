/*
 * Hello Kernel
 */

#include <linux/init.h>
#include <linux/module.h>

#define SUCCESS 0

MODULE_LICENSE("Dual BSD/GPL");

static int __init hello_init(void)
{
    printk(KERN_INFO "Hello, kernel\n");
    return SUCCESS;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "Goodbye, kernel\n");

