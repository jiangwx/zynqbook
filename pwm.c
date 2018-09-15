/*  pwm.c - The simplest kernel module.

* Copyright (C) 2013 - 2018 Xilinx, Inc
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.

*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License along
*   with this program. If not, see <http://www.gnu.org/licenses/>.

*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#define DEVICE_NAME "PWM_MOUDLE"
#define PWM_MOUDLE_PHY_ADDR 0x43C00000		//This Address is based Vivado
#define PWM_CHANNEL 4

MODULE_AUTHOR("Xilinx XUP");
MODULE_DESCRIPTION("PWM moudle dirver");
MODULE_VERSION("v1.0");
MODULE_LICENSE("GPL");

static int pwm_driver_major;
static struct class* pwm_driver_class = NULL;

struct pwm_dev {
    dev_t devt;
    struct device *device;
    unsigned long freq_addr;
    unsigned long duty_addr;
};
static struct pwm_dev pwm_dev[PWM_CHANNEL];

static long frequency=0;

static struct file_operations pwm_driver_fops = {
    .owner = THIS_MODULE,
};

static ssize_t sys_pwm_frequency_set(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    long value = 0;
    int i;
    frequency=0;
    iowrite32(value, pwm_dev[MINOR(dev->devt)].freq_addr); //close pwm moudle before we modfiy the frequency

    for (i = 0; i < count-1; i++){
        frequency  *= 10;
        frequency += buf[i] - '0';
    }
    if(value>100000000) value=100000000;
    value=100000000/frequency;  // 100Mhz/frequency 100Mhz is set by Vivado
    iowrite32(value, pwm_dev[MINOR(dev->devt)].freq_addr);

    printk("CH%d freq:%ld, address:%x\n",MINOR(dev->devt), value, pwm_dev[MINOR(dev->devt)].freq_addr);
    return count;
} 


static ssize_t sys_pwm_duty_set (struct device* dev, struct device_attribute* attr, const char* buf, size_t count) //duty cycle 
{
	long value = 0;
	int i;

	iowrite32(value, pwm_dev[MINOR(dev->devt)].duty_addr); //close pwm moudle before we modfiy the duty cycle
	for (i = 0; i < count-1; i++){
		value  *= 10;
		value += buf[i] - '0';
	}
	if (value>100) 
		value=100;
	value = 100000000 / frequency * value / 100;
	iowrite32(value, pwm_dev[MINOR(dev->devt)].duty_addr);

    printk("CH%d duty:%ld, address:%x\n",MINOR(dev->devt), value, pwm_dev[MINOR(dev->devt)].duty_addr);

    return count;
} 

static DEVICE_ATTR(frequency, S_IWUSR, NULL, sys_pwm_frequency_set);
static DEVICE_ATTR(duty, S_IWUSR, NULL, sys_pwm_duty_set);

static int __init pwm_driver_module_init(void)
{
    int i,ret;
    char name[4];
    pwm_driver_major=register_chrdev(0, DEVICE_NAME, &pwm_driver_fops);
    if (pwm_driver_major < 0){
        printk("failed to register device.\n");
        return -1;
    }

    pwm_driver_class = class_create(THIS_MODULE, "pwm");
    if (IS_ERR(pwm_driver_class)){
        printk("failed to create pwm class.\n");
        unregister_chrdev(pwm_driver_major, DEVICE_NAME);
        return -1;
    }

	for(i=0;i<PWM_CHANNEL;i++ )
    {
        pwm_dev[i].devt=MKDEV(pwm_driver_major, i);
        pwm_dev[i].device = device_create(pwm_driver_class, NULL, pwm_dev[i].devt, NULL, "CH%d", MINOR(pwm_dev[i].devt));
        if (IS_ERR(pwm_dev[i].device)){
            printk("failed to create pwm CH%d.\n",MINOR(pwm_dev[i].devt));
            unregister_chrdev(pwm_driver_major, DEVICE_NAME);
            return -1;
        }
        pwm_dev[i].freq_addr = (unsigned long)ioremap(PWM_MOUDLE_PHY_ADDR, 64)+i*8;
        pwm_dev[i].duty_addr = (unsigned long)ioremap(PWM_MOUDLE_PHY_ADDR, 64)+i*8+4;
        printk("pwm channel %d freq_addr : %x, duty_addr : %x\r\n", i,pwm_dev[i].freq_addr,pwm_dev[i].duty_addr);
        
        ret = device_create_file(pwm_dev[i].device , &dev_attr_frequency);
        if (ret < 0)
        printk("failed to create pwm_frequency endpoint\n");
        ret = device_create_file(pwm_dev[i].device , &dev_attr_duty);
        if (ret < 0)
        printk("failed to create pwm_duty endpoint\n");
    }

    printk(" pwm driver initial successfully!\n");
    return 0;
}



static void __exit pwm_driver_module_exit(void)
{
    int i=0;
    for(i=0;i<PWM_CHANNEL;i++ )
    {
        device_remove_file(pwm_dev[i].device, &dev_attr_frequency);
        device_remove_file(pwm_dev[i].device, &dev_attr_duty);
    }
    device_destroy(pwm_driver_class, MKDEV(pwm_driver_major, 0));
    class_unregister(pwm_driver_class);
    class_destroy(pwm_driver_class);
    unregister_chrdev(pwm_driver_major, DEVICE_NAME);
    printk("pwm module exit.\n");
}

module_init(pwm_driver_module_init);
module_exit(pwm_driver_module_exit);
