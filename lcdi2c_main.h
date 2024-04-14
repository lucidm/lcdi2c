#ifndef _I2CLCD8574_H
#define _I2CLCD8574_H

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
#define META_BUFFER_LEN (100)
#define SHORT_STR_LEN (12)
#define SUCCESS (0)

#define DEVICE_NAME "lcdi2c"
#define DEVICE_MAJOR (0)
#define DEVICE_CLASS_NAME "alphalcd"

// According to https://www.kernel.org/doc/html/latest/userspace-api/ioctl/ioctl-number.html this code is free
// It doesn't mean that it will be free in the future, but that is a good start
#define LCD_IOCTL_BASE (0xF5)

#define IOCTLB (1)  //Multibyte argument
#define IOCTLC (2)  //Single character argument
#define LCD_IOCTL_GETCHAR _IOR(LCD_IOCTL_BASE, IOCTLC | (0x01 << 2), u8)
#define LCD_IOCTL_SETCHAR _IOW(LCD_IOCTL_BASE, IOCTLC | (0x02 << 2), u8)
#define LCD_IOCTL_GETLINE _IOR(LCD_IOCTL_BASE, IOCTLB | (0x03 << 2), LcdLineArgs_t)
#define LCD_IOCTL_SETLINE _IOW(LCD_IOCTL_BASE, IOCTLB | (0x04 << 2), LcdLineArgs_t)
#define LCD_IOCTL_GETBUFFER _IOR(LCD_IOCTL_BASE, IOCTLB | (0x05 << 2), LcdBuffer_t)
#define LCD_IOCTL_SETBUFFER _IOW(LCD_IOCTL_BASE, IOCTLB | (0x06 << 2), LcdBuffer_t)
#define LCD_IOCTL_GETPOSITION _IOR(LCD_IOCTL_BASE, IOCTLB | (0x07 << 2), LcdPositionArgs_t)
#define LCD_IOCTL_SETPOSITION _IOW(LCD_IOCTL_BASE, IOCTLB | (0x08 << 2), LcdPositionArgs_t)
#define LCD_IOCTL_GETBACKLIGHT _IOR(LCD_IOCTL_BASE, IOCTLC | (0x09 <<2), LcdBoolArgs_t)
#define LCD_IOCTL_SETBACKLIGHT _IOW(LCD_IOCTL_BASE, IOCTLC | (0x0A << 2), LcdBoolArgs_t)
#define LCD_IOCTL_GETCURSOR _IOR(LCD_IOCTL_BASE, IOCTLC | (0x0B << 2), LcdBoolArgs_t)
#define LCD_IOCTL_SETCURSOR _IOW(LCD_IOCTL_BASE, IOCTLC | (0x0C<< 2), LcdBoolArgs_t)
#define LCD_IOCTL_GETBLINK _IOR(LCD_IOCTL_BASE, IOCTLC | (0x0D << 2), LcdBoolArgs_t)
#define LCD_IOCTL_SETBLINK _IOW(LCD_IOCTL_BASE, IOCTLC | (0x0E << 2), LcdBoolArgs_t)
#define LCD_IOCTL_GETCUSTOMCHAR _IOWR(LCD_IOCTL_BASE, IOCTLB | (0x0F << 2), LcdCustomCharArgs_t)
#define LCD_IOCTL_SETCUSTOMCHAR _IOW(LCD_IOCTL_BASE, IOCTLB | (0x10 << 2), LcdCustomCharArgs_t)
#define LCD_IOCTL_SCROLLHZ _IOW(LCD_IOCTL_BASE, IOCTLC | (0x11 << 2), u8)
#define LCD_IOCTL_SCROLLVERT _IOW(LCD_IOCTL_BASE, IOCTLC | (0x12 << 2), LcdScrollArgs_t)
#define LCD_IOCTL_CLEAR _IO(LCD_IOCTL_BASE, IOCTLC | (0x13 << 2))
#define LCD_IOCTL_RESET _IO(LCD_IOCTL_BASE, IOCTLC | (0x14 << 2))
#define LCD_IOCTL_HOME  _IO(LCD_IOCTL_BASE, IOCTLC | (0x15 << 2))

#define SEM_DOWN(lcd_handler) down_interruptible(&lcd_handler->driver_data.sem)
#define SEM_UP(lcd_handler) up(&lcd_handler->driver_data.sem)

typedef struct ioctl_description {
  const uint32_t ioctl_code;
  const char name[24];
} IOCTLDescription_t;

static int lcdi2c_register(struct i2c_client *client);
static void lcdi2c_unregister(struct i2c_client *client);
static int lcdi2c_probe(struct i2c_client *client);
static void lcdi2c_remove(struct i2c_client *client);
static void lcdi2c_shutdown(struct i2c_client *client);
static ssize_t lcdi2c_fopread(struct file *file, char __user *buffer, size_t length, loff_t *offset);
static ssize_t lcdi2c_fopwrite(struct file *file, const char __user *buffer, size_t length, loff_t *offset);
loff_t lcdi2c_lseek(struct file *file, loff_t offset, int orig);
static long lcdi2c_ioctl(struct file *file, unsigned int ioctl_num, unsigned long arg);
static int lcdi2c_open(struct inode *inode, struct file *file);
static int lcdi2c_release(struct inode *inode, struct file *file);

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
static ssize_t lcdi2c_scrollvert(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_customchar_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lcdi2c_customchar(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_char_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t lcdi2c_char(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lcdi2c_line_show(struct device *dev, struct device_attribute *attr, char *buf);

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
DEVICE_ATTR(scrollvert, S_IWUSR | S_IWGRP, NULL, lcdi2c_scrollvert);
DEVICE_ATTR(customchar, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_customchar_show, lcdi2c_customchar);
DEVICE_ATTR(character, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_char_show, lcdi2c_char);
DEVICE_ATTR(line, S_IRUSR | S_IRGRP | S_IROTH, lcdi2c_line_show, NULL);

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
        &dev_attr_scrollvert.attr,
        &dev_attr_customchar.attr,
        &dev_attr_character.attr,
        &dev_attr_line.attr,
        NULL,
};

#endif