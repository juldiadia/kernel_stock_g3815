/* drivers/misc/bln.c
 *
 * Copyright 2013  Julian Diaz <july491@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/bln.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/earlysuspend.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/timer.h>
//#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/sec_class.h>
#include <linux/err.h>

static bool bln_enabled = true; /* is BLN function is enabled */
static bool bln_ongoing = false; /* ongoing LED Notification */
static struct bln_implementation *bln_imp = NULL;
static int bln_brightness = 1; //express 2: got 2 values {0,1}

#define BACKLIGHTNOTIFICATION_VERSION 9

/* Functions */
static void bln_enable_backlights(void);
static void bln_disable_backlights(void);;
static void enable_led_notification(void);
static void disable_led_notification(void);

int get_bln_brightness(void){
	return bln_brightness;
}

static void bln_enable_backlights(void)
{
	if (bln_imp){
	    pr_info("%s: led_set brightness: %d\n", __FUNCTION__, bln_brightness);
		bln_imp->led_set(bln_brightness);
    }
}

static void bln_disable_backlights(void)
{
    if (bln_imp)
		bln_imp->led_set(0);
}

static void enable_led_notification(void)
{

	if (!bln_enabled || bln_ongoing)
		return;

	bln_enable_backlights();

	pr_info("%s: notification led enabled\n", __FUNCTION__);
	bln_ongoing = true;
}

static void disable_led_notification()
{
	if (!bln_ongoing)
		return;

	pr_info("%s: notification led disabled\n", __FUNCTION__);

	bln_ongoing = false;
	bln_disable_backlights();
}

// Attrs
static ssize_t backlightnotification_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (bln_enabled ? 1 : 0));
}

static ssize_t backlightnotification_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;
	if(sscanf(buf, "%u\n", &data) == 1) {
		pr_devel("%s: %u \n", __FUNCTION__, data);
		if (data == 1) {
			pr_info("%s: BLN function enabled\n", __FUNCTION__);
			bln_enabled = true;
		} else if (data == 0) {
			pr_info("%s: BLN function disabled\n", __FUNCTION__);
			bln_enabled = false;
			if (bln_ongoing)
				disable_led_notification();
		} else {
			pr_info("%s: invalid input range %u\n", __FUNCTION__,
					data);
		}
	} else {
		pr_info("%s: invalid input\n", __FUNCTION__);
	}

	return size;
}

static ssize_t notification_led_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", (bln_ongoing ? 1 : 0));
}

static ssize_t notification_led_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1) {
		pr_info("%s: debug input %u\n", __FUNCTION__, data);
		if (data >= 1) //Patch for original liblight
			enable_led_notification();
		else if (data == 0)
			disable_led_notification();
		else
			pr_info("%s: wrong input %u\n", __FUNCTION__, data);
	} else {
		pr_info("%s: input error\n", __FUNCTION__);
	}

	return size;
}

static ssize_t notification_blink_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data,on,off;

	if (sscanf(buf, "%x %u %u\n", &data, &on, &off) == 1) {
		pr_info("%s: debug input blink %x\n", __FUNCTION__, data);
		if (data != 0x000000) //Patch for original liblight
			enable_led_notification();
		else if (data == 0x000000)
			disable_led_notification();
		else
			pr_info("%s: wrong input %u\n", __FUNCTION__, data);
	} else if(sscanf(buf, "%x\n", &data) == 1) {
		pr_info("%s: debug input blink %x\n", __FUNCTION__, data);
		if (data != 0x000000) //Patch for original liblight
			enable_led_notification();
		else if (data == 0x0)
			disable_led_notification();
		else
			pr_info("%s: wrong input %u\n", __FUNCTION__, data);
	} else { 
		pr_info("%s: input error\n", __FUNCTION__);
	}

	return size;
}

static ssize_t backlightnotification_version(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", BACKLIGHTNOTIFICATION_VERSION);
}

static DEVICE_ATTR(enabled, S_IRUGO | S_IWUGO,
		backlightnotification_status_read,
		backlightnotification_status_write);
static DEVICE_ATTR(led_pattern, S_IRUGO | S_IWUGO,
		notification_led_read,
		notification_led_write);
static DEVICE_ATTR(led_blink, S_IRUGO | S_IWUGO,
		notification_led_read,
		notification_blink_write);
static DEVICE_ATTR(version, S_IRUGO , backlightnotification_version, NULL);

static struct device *bln_device;

void register_bln_implementation(struct bln_implementation *imp)
{
    /*Initialize bln*/
	bln_imp = imp;
}


bool bln_is_ongoing()
{
	return bln_ongoing;
}


static int __init bln_control_init(void)
{
	int ret;

	bln_device = device_create(sec_class, NULL, 0, NULL, "led");
	if (IS_ERR(bln_device)) {
		pr_err("Failed to create BLN device samsung\n");
		return 1;
	}

	ret = device_create_file(bln_device, &dev_attr_enabled);
	if (ret)
		return ret;
	ret = device_create_file(bln_device, &dev_attr_led_pattern);
	if (ret)
		return ret;
	ret = device_create_file(bln_device, &dev_attr_led_blink);
	if (ret)
		return ret;
	ret = device_create_file(bln_device, &dev_attr_version);
	if (ret)
		return ret;

	return 0;
}

device_initcall(bln_control_init);
