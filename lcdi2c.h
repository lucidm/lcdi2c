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
#include <asm/uaccess.h>

#include "lcdlib.h"

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

#endif