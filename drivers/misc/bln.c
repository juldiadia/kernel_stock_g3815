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

static bool bln_enabled = true; /* is BLN function is enabled */
static bool bln_ongoing = false; /* ongoing LED Notification */
static int bln_blink_state = 0;
static bool bln_suspended = false; /* is system suspended */
static struct bln_implementation *bln_imp = NULL;
static bool in_kernel_blink = false;
static unsigned int blink_count;
static int bln_maxtime = 0; // 0 infinite or x minutes
static int bln_brightness = 1; //express 2: got 2 values {0,1}
static int blink_maxtime = 0; // 0 infinite or x minutes
//static struct wake_lock bln_wake_lock;

void maxtime_callback(unsigned long data);
static struct timer_list maxtime_timer ;//= TIMER_INITIALIZER(maxtime_callback, 0, 0);

void bl_timer_callback(unsigned long data);
static struct timer_list blink_timer ;//= TIMER_INITIALIZER(bl_timer_callback, 0, 0);

static void blink_callback(void);

#define BLINK_INTERVAL 4000 /* on / off every 2000ms */
//#define MAX_BLINK_COUNT 300 /* 10 minutes */
#define BACKLIGHTNOTIFICATION_VERSION 9

/* Functions */
static void bln_enable_backlights(void);
static void bln_disable_backlights(void);
static void bln_early_suspend(struct early_suspend *h);
static void bln_late_resume(struct early_suspend *h);
static void enable_led_notification(void);
static void disable_led_notification(void);
static void blink_callback(void);
void bl_timer_callback(unsigned long data);
void maxtime_callback(unsigned long data);

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

static void bln_early_suspend(struct early_suspend *h)
{
	bln_suspended = true;
}

static void bln_late_resume(struct early_suspend *h)
{
	bln_suspended = false;
}

static struct early_suspend bln_suspend_data = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend = bln_early_suspend,
	.resume = bln_late_resume,
};

static void enable_led_notification(void)
{

	if (!bln_enabled)
		return;

	if ( in_kernel_blink ) {
		//wake_lock(&bln_wake_lock);

		/* Start timer */
		init_timer(&blink_timer);
		blink_timer.expires = jiffies +
				msecs_to_jiffies(BLINK_INTERVAL);
		blink_timer.function = bl_timer_callback;
		if(blink_maxtime != 0)
			blink_count = blink_maxtime*15; //4 sec per blink
		else
			blink_count = 10; //Value higher than 1 to keep looping
		add_timer(&blink_timer);
	}

	bln_enable_backlights();

	//Maxtime
	if(bln_maxtime >= 1){
		init_timer(&maxtime_timer);
		maxtime_timer.expires = jiffies +
				msecs_to_jiffies(bln_maxtime*1000*60);
		maxtime_timer.function = maxtime_callback;
		add_timer(&maxtime_timer);
	}
	pr_info("%s: notification led enabled\n", __FUNCTION__);
	bln_ongoing = true;
}

static void disable_led_notification()
{
	if (!bln_ongoing)
		return;

	pr_info("%s: notification led disabled\n", __FUNCTION__);

	bln_blink_state = 0;
	bln_ongoing = false;

	if( bln_maxtime >=1 )
		del_timer(&maxtime_timer);

	if (bln_suspended)
		bln_disable_backlights();

	if (in_kernel_blink)
		del_timer(&blink_timer);

	//wake_unlock(&bln_wake_lock);
}

static void blink_callback(void)
{
	if ( blink_maxtime >= 1 ) --blink_count;
	if ( blink_count == 0 ) {
		pr_info("%s: notification timed out\n", __FUNCTION__);
		bln_enable_backlights();
		del_timer(&blink_timer);
		//wake_unlock(&bln_wake_lock);
		return;
	}

	if (bln_blink_state)
		bln_enable_backlights();
	else
		bln_disable_backlights();

	bln_blink_state = !bln_blink_state;
}

void bl_timer_callback(unsigned long data)
{
	//schedule_work(&blink_work);
	mod_timer(&blink_timer, jiffies + msecs_to_jiffies(BLINK_INTERVAL));
	blink_callback();
}

void maxtime_callback(unsigned long data)
{
	disable_led_notification();
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

static ssize_t notification_led_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", (bln_ongoing ? 1 : 0));
}

static ssize_t notification_led_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1) {
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

static ssize_t blink_maxtime_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", (blink_maxtime));
}

static ssize_t blink_maxtime_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1)
		if (data >= 1 || data == 0)
			blink_maxtime = data;
		else
			pr_info("%s: input error maxtime = 0(infinite) or x minutes\n", __FUNCTION__);
	else
		pr_info("%s: input error\n", __FUNCTION__);

	return size;
}

static ssize_t maxtime_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", (bln_maxtime));
}

static ssize_t maxtime_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1)
		if (data >= 1 || data == 0)
			bln_maxtime = data;
		else
			pr_info("%s: input error maxtime = 0(infinite) or x minutes\n", __FUNCTION__);
	else
		pr_info("%s: input error\n", __FUNCTION__);

	return size;
}

static ssize_t brightness_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", (bln_brightness));
}

static ssize_t brightness_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1)
		if (data > 0 && data < 1)
			bln_brightness = data;
		else
			pr_info("%s: just value 1 is allowed\n", __FUNCTION__);
	else
		pr_info("%s: input error\n", __FUNCTION__);

	return size;
}

static ssize_t in_kernel_blink_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", (in_kernel_blink ? 1 : 0));
}

static ssize_t in_kernel_blink_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1)
		in_kernel_blink = !!(data);
	else
		pr_info("%s: input error\n", __FUNCTION__);

	return size;
}
static ssize_t blink_control_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", bln_blink_state);
}

static ssize_t blink_control_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (!bln_ongoing)
		return size;

	if (sscanf(buf, "%u\n", &data) == 1) {
		if (data == 1) {
			bln_blink_state = 1;
			bln_disable_backlights();
		} else if (data == 0) {
			bln_blink_state = 0;
			bln_enable_backlights();
		} else {
			pr_info("%s: wrong input %u\n", __FUNCTION__, data);
		}
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

static DEVICE_ATTR(blink_control, S_IRUGO | S_IWUGO, blink_control_read,
		blink_control_write);
static DEVICE_ATTR(enabled, S_IRUGO | S_IWUGO,
		backlightnotification_status_read,
		backlightnotification_status_write);
static DEVICE_ATTR(notification_led, S_IRUGO | S_IWUGO,
		notification_led_status_read,
		notification_led_status_write);
static DEVICE_ATTR(maxtime, S_IRUGO | S_IWUGO,
		maxtime_read,
		maxtime_write);
static DEVICE_ATTR(blink_maxtime, S_IRUGO | S_IWUGO,
		blink_maxtime_read,
		blink_maxtime_write);
static DEVICE_ATTR(brightness, S_IRUGO | S_IWUGO,
		brightness_read,
		brightness_write);
static DEVICE_ATTR(in_kernel_blink, S_IRUGO | S_IWUGO,
		in_kernel_blink_status_read,
		in_kernel_blink_status_write);
static DEVICE_ATTR(version, S_IRUGO , backlightnotification_version, NULL);

static struct attribute *bln_notification_attributes[] = {
	&dev_attr_blink_control.attr,
	&dev_attr_enabled.attr,
	&dev_attr_notification_led.attr,
	&dev_attr_blink_maxtime.attr,
	&dev_attr_maxtime.attr,
	&dev_attr_brightness.attr,
	&dev_attr_in_kernel_blink.attr,
	&dev_attr_version.attr,
	NULL
};

static struct attribute_group bln_notification_group = {
	.attrs  = bln_notification_attributes,
};

static struct miscdevice bln_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "backlightnotification",
};


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

	pr_info("%s misc_register(%s)\n", __FUNCTION__, bln_device.name);
	ret = misc_register(&bln_device);
	if (ret) {
		pr_err("%s misc_register(%s) fail\n", __FUNCTION__,
				bln_device.name);
		return 1;
	}

	/* add the bln attributes */
	if (sysfs_create_group(&bln_device.this_device->kobj,
				&bln_notification_group) < 0) {
		pr_err("%s sysfs_create_group fail\n", __FUNCTION__);
		pr_err("Failed to create sysfs group for device (%s)!\n",
				bln_device.name);
	}

	register_early_suspend(&bln_suspend_data);

    /* Initialize wake locks */
	//wake_lock_init(&bln_wake_lock, WAKE_LOCK_SUSPEND, "bln_wake");

	return 0;
}

device_initcall(bln_control_init);
