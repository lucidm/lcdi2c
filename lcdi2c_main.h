#ifndef _I2CLCD8574_H
#define _I2CLCD8574_H

#include <linux/string.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/aio.h>
#include <linux/version.h>
#include <asm/uaccess.h>

#include "lcdlib.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

#define WS_MAX_LEN (16)
#define DEFAULT_WS "HDD44780\nDriver"
#define META_BUFFER_LEN (74)
#define SHORT_STR_LEN (12)
#define SUCCESS (0)

#define DEVICE_NAME "lcdi2c"
#define DEVICE_MAJOR (0)
#define DEVICE_CLASS_NAME "alphalcd"

#define LCD_IOCTL_BASE (0xF5)
//Binary argument
#define IOCTLB (1)
//Character argument 
#define IOCTLC (2)
#define LCD_IOCTL_GETCHAR _IOR(LCD_IOCTL_BASE, IOCTLC | (0x01 << 2), char *)
#define LCD_IOCTL_SETCHAR _IOW(LCD_IOCTL_BASE, IOCTLC | (0x01 << 2), char *)
#define LCD_IOCTL_GETPOSITION _IOR(LCD_IOCTL_BASE, IOCTLB | (0x02 << 2), char *)
#define LCD_IOCTL_SETPOSITION _IOW(LCD_IOCTL_BASE, IOCTLB | (0x02 << 2), char *)
#define LCD_IOCTL_RESET _IOW(LCD_IOCTL_BASE, IOCTLC | (0x03 << 2), char *)
#define LCD_IOCTL_HOME  _IOW(LCD_IOCTL_BASE, IOCTLC | (0x04 << 2), char *)
#define LCD_IOCTL_SETBACKLIGHT _IOW(LCD_IOCTL_BASE, IOCTLC | (0x05 << 2), char *)
#define LCD_IOCTL_GETBACKLIGHT _IOR(LCD_IOCTL_BASE, IOCTLC | (0x05 <<2), char *)
#define LCD_IOCTL_SETCURSOR _IOW(LCD_IOCTL_BASE, IOCTLC | (0x06<< 2), char *)
#define LCD_IOCTL_GETCURSOR _IOR(LCD_IOCTL_BASE, IOCTLC | (0x06 << 2), char *)
#define LCD_IOCTL_SETBLINK _IOW(LCD_IOCTL_BASE, IOCTLC | (0x07 << 2), char *)
#define LCD_IOCTL_GETBLINK _IOR(LCD_IOCTL_BASE, IOCTLC | (0x07 << 2), char *)
#define LCD_IOCTL_SCROLLHZ _IOW(LCD_IOCTL_BASE, IOCTLC | (0x08 << 2), char *)
#define LCD_IOCTL_SETCUSTOMCHAR _IOW(LCD_IOCTL_BASE, IOCTLB | (0x09 << 2), char *)
#define LCD_IOCTL_GETCUSTOMCHAR _IOR(LCD_IOCTL_BASE, IOCTLB | (0x09 << 2), char *)
#define LCD_IOCTL_CLEAR _IOW(LCD_IOCTL_BASE, IOCTLC | (0x0A << 2), char *)

typedef struct ioctl_description {
  const uint32_t ioctl_code;
  const char name[24];
} IOCTLDescription_t;

static ssize_t lcdi2c_reset(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_backlight_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lcdi2c_backlight(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_cursorpos_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lcdi2c_cursorpos(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_data_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lcdi2c_data(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_meta_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lcdi2c_cursor_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lcdi2c_cursor(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_blink_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lcdi2c_blink(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_home(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_clear(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_scrollhz(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_customchar_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lcdi2c_customchar(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_char_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lcdi2c_char(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

DEVICE_ATTR(reset, S_IWUSR | S_IWGRP, NULL, lcdi2c_reset);
DEVICE_ATTR(brightness, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_backlight_show, lcdi2c_backlight);
DEVICE_ATTR(position, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_cursorpos_show, lcdi2c_cursorpos);
DEVICE_ATTR(data, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_data_show, lcdi2c_data);
DEVICE_ATTR(meta, S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_meta_show, NULL);
DEVICE_ATTR(cursor, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_cursor_show, lcdi2c_cursor);
DEVICE_ATTR(blink, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_blink_show, lcdi2c_blink);
DEVICE_ATTR(home, S_IWUSR | S_IWGRP, NULL, lcdi2c_home);
DEVICE_ATTR(clear, S_IWUSR | S_IWGRP, NULL, lcdi2c_clear);
DEVICE_ATTR(scrollhz, S_IWUSR | S_IWGRP, NULL, lcdi2c_scrollhz);
DEVICE_ATTR(customchar, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_customchar_show, lcdi2c_customchar);
DEVICE_ATTR(character, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_char_show, lcdi2c_char);

static const struct attribute *i2clcd_attrs[] = {
        &dev_attr_reset.attr,
        &dev_attr_brightness.attr,
        &dev_attr_position.attr,
        &dev_attr_data.attr,
        &dev_attr_meta.attr,
        &dev_attr_cursor.attr,
        &dev_attr_blink.attr,
        &dev_attr_home.attr,
        &dev_attr_clear.attr,
        &dev_attr_scrollhz.attr,
        &dev_attr_customchar.attr,
        &dev_attr_character.attr,
        NULL,
};

#endif