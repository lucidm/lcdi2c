#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "lcdi2c_main.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jarek Zok <jarekzok@gmail.com>");
MODULE_DESCRIPTION(LCDI2C_DESCRIPTION);
MODULE_VERSION(LCDI2C_VERSION);

static uint topo = LCD_DEFAULT_ORGANIZATION;
static uint bus = 0;
static uint address = 0;
static uint cursor = 0;
static uint blink = 0;
static uint swscreen = 0;
static uint pinout_cnt = 0;
static char *wscreen = DEFAULT_WS;
static LcdDescriptor_t *lcdi2c_gDescriptor;

module_param(bus, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(address, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param_array(pinout, uint, &pinout_cnt, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(cursor, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(blink, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(topo, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(swscreen, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(wscreen, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

MODULE_PARM_DESC(pinout, " I2C module pinout configuration, eight "
                         "numbers\n\t\trepresenting following LCD module"
                         "pins in order: RS,RW,E,D4,D5,D6,D7,\n"
                         "\t\tdefault 0,1,2,3,4,5,6,7");
MODULE_PARM_DESC(cursor, " Show cursor at start 1 - Yes, 0 - No, default 1");
MODULE_PARM_DESC(blink, " Blink cursor 1 - Yes, 0 - No, defualt 1");
MODULE_PARM_DESC(topo, " Display organization, following values are supported:\n"
                       "\t\t0 - 40x2\n"
                       "\t\t1 - 20x4\n"
                       "\t\t2 - 20x2\n"
                       "\t\t3 - 16x4\n"
                       "\t\t4 - 16x2\n"
                       "\t\t5 - 16x1 Type 1\n"
                       "\t\t6 - 16x1 Type 2\n"
                       "\t\t7 - 8x2\n"
                       "\t\tDefault set to 4 (16x2)");
MODULE_PARM_DESC(swscreen, " Show welcome screen on load, 1 - Yes, 0 - No, default 0");
MODULE_PARM_DESC(wscreen, " Welcome screen string, default \""DEFAULT_WS"\"");

static const IOCTLDescription_t ioControls[] = {
        {.ioctl_code = LCD_IOCTL_GETCHAR, .name = "GETCHAR",},
        {.ioctl_code = LCD_IOCTL_SETCHAR, .name = "SETCHAR",},
        {.ioctl_code = LCD_IOCTL_GETLINE, .name = "GETLINE"},
        {.ioctl_code = LCD_IOCTL_SETLINE, .name = "SETLINE"},
        {.ioctl_code = LCD_IOCTL_GETBUFFER, .name = "GETBUFFER"},
        {.ioctl_code = LCD_IOCTL_SETBUFFER, .name = "SETBUFFER"},
        {.ioctl_code = LCD_IOCTL_GETPOSITION, .name = "GETPOSITION"},
        {.ioctl_code = LCD_IOCTL_SETPOSITION, .name = "SETPOSITION"},
        {.ioctl_code = LCD_IOCTL_RESET, .name = "RESET"},
        {.ioctl_code = LCD_IOCTL_HOME, .name = "HOME"},
        {.ioctl_code = LCD_IOCTL_GETBACKLIGHT, .name = "GETBACKLIGHT"},
        {.ioctl_code = LCD_IOCTL_SETBACKLIGHT, .name = "SETBACKLIGHT"},
        {.ioctl_code = LCD_IOCTL_GETCURSOR, .name = "GETCURSOR"},
        {.ioctl_code = LCD_IOCTL_SETCURSOR, .name = "SETCURSOR"},
        {.ioctl_code = LCD_IOCTL_GETBLINK, .name = "GETBLINK"},
        {.ioctl_code = LCD_IOCTL_SETBLINK, .name = "SETBLINK"},
        {.ioctl_code = LCD_IOCTL_SCROLLHZ, .name = "SCROLLHZ"},
        {.ioctl_code = LCD_IOCTL_SCROLLVERT, .name = "SCROLLVERT"},
        {.ioctl_code = LCD_IOCTL_GETCUSTOMCHAR, .name = "GETCUSTOMCHAR"},
        {.ioctl_code = LCD_IOCTL_SETCUSTOMCHAR, .name = "SETCUSTOMCHAR"},
        {.ioctl_code = LCD_IOCTL_CLEAR, .name = "CLEAR"},

};

/*
 * Driver data (common to all clients)
 */

static struct of_device_id lcdi2c_driver_ids[] = {
        {
                .compatible = "pcf8574,lcdi2c",
        }, { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lcdi2c_driver_ids);

static const struct i2c_device_id lcdi2c_id[] = {
        {"lcdi2c", 0},
        {},
};
MODULE_DEVICE_TABLE(i2c, lcdi2c_id);

static struct i2c_driver lcdi2c_driver = {
        .probe = lcdi2c_probe,
        .remove = lcdi2c_remove,
        .shutdown = lcdi2c_shutdown,
        .id_table = lcdi2c_id,
        .driver = {
                .name    = "lcdi2c",
                .of_match_table = lcdi2c_driver_ids,
        },
};

static struct file_operations lcdi2c_fops = {
        .read = lcdi2c_fopread,
        .write = lcdi2c_fopwrite,
        .llseek = lcdi2c_lseek,
        .unlocked_ioctl = lcdi2c_ioctl,
        .open = lcdi2c_open,
        .release = lcdi2c_release,
        .owner = THIS_MODULE,
};

static const struct attribute_group i2clcd_device_attr_group = {
        .attrs = (struct attribute **) i2clcd_attrs,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
static int lcdi2c_dev_uevent(const struct device *dev, struct kobj_uevent_env *env) {
#else
static int lcdi2c_dev_uevent(struct device *_, struct kobj_uevent_env *env) {
#endif
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static void set_welcome_message(LcdDescriptor_t *lcdData, char *welcome_msg) {
    strncpy(lcdData->welcome, strlen(welcome_msg) ? welcome_msg : DEFAULT_WS, WS_MAX_LEN);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
static int lcdi2c_probe(struct i2c_client *client) {
#else
static int lcdi2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {
#endif
    int ret = 0;

    lcdi2c_gDescriptor = (LcdDescriptor_t *) devm_kzalloc(&client->dev, sizeof(LcdDescriptor_t), GFP_KERNEL);

    if (!lcdi2c_gDescriptor)
        return -ENOMEM;

    if (device_property_present(&client->dev, "topology")) {
        if (device_property_read_u32(&client->dev, "topology", &topo)) {
            dev_err(&client->dev, "topology property read failed\n");
            return -EINVAL;
        }
    }

    sema_init(&lcdi2c_gDescriptor->driver_data.sem, 1);
    lcdi2c_gDescriptor->driver_data.client = client;
    lcdi2c_gDescriptor->driver_data.use_cnt = 0;
    lcdi2c_gDescriptor->driver_data.open_cnt = 0;
    lcdi2c_gDescriptor->backlight = 1;
    lcdi2c_gDescriptor->cursor = cursor;
    lcdi2c_gDescriptor->blink = blink;
    lcdi2c_gDescriptor->show_welcome_screen = swscreen;
    set_welcome_message(lcdi2c_gDescriptor, wscreen);
    i2c_set_clientdata(client, lcdi2c_gDescriptor);

    ret = lcdi2c_register(client);
    if (0 != ret) {
        kfree(lcdi2c_gDescriptor);
        return ret;
    }

    lcdinit(lcdi2c_gDescriptor, topo);
    if (lcdi2c_gDescriptor->show_welcome_screen) {
        lcdprint(lcdi2c_gDescriptor, lcdi2c_gDescriptor->welcome);
    }

    dev_info(&client->dev, "Registered LCD display with %u-columns x %u-rows on bus 0x%X at address 0x%X",
             lcdi2c_gDescriptor->organization.columns,
             lcdi2c_gDescriptor->organization.rows, client->adapter->nr, client->addr);
    return 0;
}

static void lcdi2c_shutdown(struct i2c_client *client) {
    LcdDescriptor_t *lcd_handler = i2c_get_clientdata(client);
    lcdfinalize(lcd_handler);
}

static void lcdi2c_remove(struct i2c_client *client) {
    LcdDescriptor_t *lcd_handler = i2c_get_clientdata(client);

    dev_info(&client->dev, "going to be removed");
    lcdfinalize(lcd_handler);
    lcdi2c_unregister(client);
}

static int lcdi2c_register(struct i2c_client *client) {
    LcdDescriptor_t *lcd_handler = i2c_get_clientdata(client);

    if (alloc_chrdev_region(&lcd_handler->driver_data.major, 0, 1, DEVICE_NAME) < 0) {
        dev_err(&client->dev, "failed to allocate major number\n");
        return -1;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    lcd_handler->driver_data.lcdi2c_class = class_create(DEVICE_CLASS_NAME);
#else
    lcd_handler->driver_data.lcdi2c_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);
#endif
    if (IS_ERR(lcd_handler->driver_data.lcdi2c_class)) {
        dev_warn(&client->dev, "class creation failed %s\n", DEVICE_CLASS_NAME);
        goto classError;
    }

    lcd_handler->driver_data.minor = lcd_handler->driver_data.major & MINORMASK;

    lcd_handler->driver_data.lcdi2c_class->dev_uevent = lcdi2c_dev_uevent;
    lcd_handler->driver_data.lcdi2c_device = device_create(lcd_handler->driver_data.lcdi2c_class,
                                                           NULL,
                                                           lcd_handler->driver_data.major,
                                                           NULL,
                                                           DEVICE_NAME);
    if (IS_ERR(lcd_handler->driver_data.lcdi2c_device)) {
        dev_warn(&client->dev, "device %s creation failed\n", DEVICE_NAME);
        goto fileError;
    }

    cdev_init(&lcd_handler->driver_data.cdev, &lcdi2c_fops);

    if (cdev_add(&lcd_handler->driver_data.cdev, lcd_handler->driver_data.major, 1) < 0) {
        dev_warn(&client->dev, "cdev_add failed\n");
        goto addError;
    }

    if (sysfs_create_group(&lcd_handler->driver_data.lcdi2c_device->kobj, &i2clcd_device_attr_group)) {
        dev_warn(&client->dev, "device attribute group creation failed\n");
        goto addError;
    }

    dev_info(&client->dev, "registered with Major: %u Minor: %u\n", lcd_handler->driver_data.major >> 20,
             lcd_handler->driver_data.major & MINORMASK);

    return 0;

addError:
    device_destroy(lcd_handler->driver_data.lcdi2c_class, lcd_handler->driver_data.major);
fileError:
    class_destroy(lcd_handler->driver_data.lcdi2c_class);
classError:
    unregister_chrdev_region(lcd_handler->driver_data.major, 1);
    i2c_unregister_device(client);
    i2c_del_driver(&lcdi2c_driver);
    return -1;
}


static void lcdi2c_unregister(struct i2c_client *client) {
    LcdDescriptor_t *lcd_handler = i2c_get_clientdata(client);

    cdev_del(&lcd_handler->driver_data.cdev);
    sysfs_remove_group(&lcd_handler->driver_data.lcdi2c_device->kobj, &i2clcd_device_attr_group);
    device_destroy(lcd_handler->driver_data.lcdi2c_class, lcd_handler->driver_data.major);
    class_destroy(lcd_handler->driver_data.lcdi2c_class);
    unregister_chrdev_region(lcd_handler->driver_data.major, 1);
}


static int lcdi2c_open(struct inode *_, struct file *__) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -EBUSY;
    }

    lcdi2c_gDescriptor->driver_data.open_cnt++;
    try_module_get(THIS_MODULE);
    SEM_UP(lcdi2c_gDescriptor);

    return SUCCESS;
}

static int lcdi2c_release(struct inode *_, struct file *__) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -EBUSY;
    }

    lcdi2c_gDescriptor->driver_data.open_cnt--;

    module_put(THIS_MODULE);
    SEM_UP(lcdi2c_gDescriptor);
    return SUCCESS;
}

static ssize_t lcdi2c_fopread(struct file *file, char __user *buffer,
                              size_t length, loff_t *offset) {
    size_t to_copy = length < LCD_BUFFER_SIZE ? length : LCD_BUFFER_SIZE, rest = 0;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -EBUSY;
    }

    rest = copy_to_user(buffer, lcdi2c_gDescriptor->raw_data, to_copy);

    *offset = to_copy - rest;
    SEM_UP(lcdi2c_gDescriptor);

    return to_copy - rest;
}

static ssize_t lcdi2c_fopwrite(struct file *file, const char __user *buffer,
                               size_t length, loff_t *offset) {
    size_t to_copy;
    size_t rest;
    u8 *buffer_ptr, *buffer_end;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -EBUSY;
    }

    to_copy = length < LCD_BUFFER_SIZE ? length : LCD_BUFFER_SIZE;
    buffer_ptr = lcdi2c_gDescriptor->raw_data + (lcdi2c_gDescriptor->column + (lcdi2c_gDescriptor->row * lcdi2c_gDescriptor->organization.columns));
    buffer_end = lcdi2c_gDescriptor->raw_data + LCD_BUFFER_SIZE;
    to_copy = buffer_end - buffer_ptr < to_copy ? buffer_end - buffer_ptr : to_copy;
    rest = copy_from_user(buffer_ptr, buffer, to_copy);
    lcdi2c_gDescriptor->column = (lcdi2c_gDescriptor->column + to_copy - rest) % lcdi2c_gDescriptor->organization.columns;
    lcdi2c_gDescriptor->row = (lcdi2c_gDescriptor->row + (lcdi2c_gDescriptor->column + to_copy - rest) /
            lcdi2c_gDescriptor->organization.columns) % lcdi2c_gDescriptor->organization.rows;

    lcdsetcursor(lcdi2c_gDescriptor, lcdi2c_gDescriptor->column, lcdi2c_gDescriptor->row);

    lcdflushbuffer(lcdi2c_gDescriptor);

    *offset = to_copy - rest;
    SEM_UP(lcdi2c_gDescriptor);

    return to_copy - rest;
}

loff_t lcdi2c_lseek(struct file *file, loff_t offset, int orig) {
    u8 memaddr, oldoffset;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -EBUSY;
    }
    memaddr = lcdi2c_gDescriptor->column + (lcdi2c_gDescriptor->row * lcdi2c_gDescriptor->organization.columns);
    oldoffset = memaddr;
    memaddr = (memaddr + (u8) offset) % (lcdi2c_gDescriptor->organization.rows * lcdi2c_gDescriptor->organization.columns);
    lcdi2c_gDescriptor->column = (memaddr % lcdi2c_gDescriptor->organization.columns);
    lcdi2c_gDescriptor->row = (memaddr / lcdi2c_gDescriptor->organization.columns);
    lcdsetcursor(lcdi2c_gDescriptor, lcdi2c_gDescriptor->column, lcdi2c_gDescriptor->row);
    SEM_UP(lcdi2c_gDescriptor);

    return oldoffset;
}

static long lcdi2c_ioctl(struct file *file,
                         unsigned int ioctl_num,
                         unsigned long __user arg) {
    u8 buff_offset, i;
    long status = SUCCESS;
    size_t to_copy = 0;
    LcdCharArgs_t *char_data;
    LcdCharArgs_t local_char;
    LcdBoolArgs_t *bool_data;
    LcdBoolArgs_t local_bool;
    LcdPositionArgs_t *position_data;
    LcdCustomCharArgs_t local_custom_char;
    LcdCustomCharArgs_t *custom_char;
    LcdLineArgs_t *line_data;
    LcdBufferArgs_t *buffer_data;


    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -EBUSY;
    }

    switch (ioctl_num) {
        case LCD_IOCTL_SETCHAR:
            char_data = (LcdCharArgs_t *) arg;
            if (copy_from_user(&local_char, char_data, sizeof(LcdCharArgs_t))) {
                status = -EIO;
                break;
            }
            buff_offset = (1 + lcdi2c_gDescriptor->column + (lcdi2c_gDescriptor->row * lcdi2c_gDescriptor->organization.columns)) % LCD_BUFFER_SIZE;
            lcdwrite(lcdi2c_gDescriptor, local_char.value);
            lcdi2c_gDescriptor->column = (buff_offset % lcdi2c_gDescriptor->organization.columns);
            lcdi2c_gDescriptor->row = (buff_offset / lcdi2c_gDescriptor->organization.columns);
            lcdsetcursor(lcdi2c_gDescriptor, lcdi2c_gDescriptor->column, lcdi2c_gDescriptor->row);
            break;
        case LCD_IOCTL_GETCHAR:
            char_data = (LcdCharArgs_t *) arg;
            buff_offset = (lcdi2c_gDescriptor->column + (lcdi2c_gDescriptor->row * lcdi2c_gDescriptor->organization.columns)) % LCD_BUFFER_SIZE;
            local_char.value = lcdi2c_gDescriptor->raw_data[buff_offset];
            if (copy_to_user(char_data, &local_char.value, sizeof(LcdCharArgs_t))) {
                status = -EIO;
            }
            break;
        case LCD_IOCTL_GETLINE:
            line_data = (LcdLineArgs_t *) arg;
            buff_offset = (lcdi2c_gDescriptor->row * lcdi2c_gDescriptor->organization.columns) % LCD_BUFFER_SIZE;
            if (copy_to_user(&line_data->line, lcdi2c_gDescriptor->raw_data + buff_offset, lcdi2c_gDescriptor->organization.columns)) {
                status = -EIO;
            }
            break;
        case LCD_IOCTL_SETLINE:
            line_data = (LcdLineArgs_t *) arg;
            buff_offset = (lcdi2c_gDescriptor->row * lcdi2c_gDescriptor->organization.columns) % LCD_BUFFER_SIZE;
            if (copy_from_user(lcdi2c_gDescriptor->raw_data + buff_offset, line_data->line, lcdi2c_gDescriptor->organization.columns)) {
                status = -EIO;
            } else {
                lcdflushbuffer(lcdi2c_gDescriptor);
            }
            break;
        case LCD_IOCTL_GETBUFFER:
            buffer_data = (LcdBufferArgs_t *) arg;
            if (copy_to_user(&buffer_data->buffer, lcdi2c_gDescriptor->raw_data, LCD_BUFFER_SIZE)) {
                status = -EIO;
            }
            break;
        case LCD_IOCTL_SETBUFFER:
            buffer_data = (LcdBufferArgs_t *) arg;
            if (copy_from_user(lcdi2c_gDescriptor->raw_data, buffer_data->buffer, LCD_BUFFER_SIZE)) {
                status = -EIO;
            } else {
                lcdflushbuffer(lcdi2c_gDescriptor);
            }
            break;
        case LCD_IOCTL_GETPOSITION:
            position_data = (LcdPositionArgs_t *) arg;
            put_user(lcdi2c_gDescriptor->column, &position_data->column);
            put_user(lcdi2c_gDescriptor->row, &position_data->row);
            break;
        case LCD_IOCTL_SETPOSITION:
            position_data = (LcdPositionArgs_t *) arg;
            get_user(lcdi2c_gDescriptor->column, &position_data->column);
            get_user(lcdi2c_gDescriptor->row, &position_data->row);
            lcdsetcursor(lcdi2c_gDescriptor, lcdi2c_gDescriptor->column, lcdi2c_gDescriptor->row);
            break;
        case LCD_IOCTL_RESET:
            lcdinit(lcdi2c_gDescriptor, lcdi2c_gDescriptor->organization.topology);
            break;
        case LCD_IOCTL_HOME:
            lcdhome(lcdi2c_gDescriptor);
            break;
        case LCD_IOCTL_GETCURSOR:
            bool_data = (LcdBoolArgs_t *) arg;
            local_bool.value = lcdi2c_gDescriptor->cursor ? 1 : 0;
            if (copy_to_user(bool_data, &local_bool, sizeof(LcdBoolArgs_t))) {
                status = -EIO;
            }
            break;
        case LCD_IOCTL_SETCURSOR:
            bool_data = (LcdBoolArgs_t *) arg;
            if (copy_from_user(&local_bool, bool_data, sizeof(LcdBoolArgs_t))) {
                status = -EIO;
                break;
            }
            lcdcursor(lcdi2c_gDescriptor, local_bool.value == 1);
            break;
        case LCD_IOCTL_GETBLINK:
            bool_data = (LcdBoolArgs_t *) arg;
            local_bool.value = lcdi2c_gDescriptor->blink ? 1 : 0;
            if (copy_to_user(bool_data, &local_bool, sizeof(LcdBoolArgs_t))) {
                status = -EIO;
            }
            break;
        case LCD_IOCTL_SETBLINK:
            bool_data = (LcdBoolArgs_t *) arg;
            if (copy_from_user(&local_bool, bool_data, sizeof(LcdBoolArgs_t))) {
                status = -EIO;
                break;
            }
            lcdblink(lcdi2c_gDescriptor, local_bool.value == 1);
            break;
        case LCD_IOCTL_GETBACKLIGHT:
            bool_data = (LcdBoolArgs_t *) arg;
            local_bool.value = lcdi2c_gDescriptor->backlight ? 1 : 0;
            if (copy_to_user(bool_data, &local_bool, sizeof(LcdBoolArgs_t))) {
                status = -EIO;
            }
            break;
        case LCD_IOCTL_SETBACKLIGHT:
            bool_data = (LcdBoolArgs_t *) arg;
            if (copy_from_user(&local_bool, bool_data, sizeof(LcdBoolArgs_t))) {
                status = -EIO;
                break;
            }
            lcdsetbacklight(lcdi2c_gDescriptor, local_bool.value == 1);
            break;
        case LCD_IOCTL_SCROLLHZ:
            bool_data = (LcdBoolArgs_t *) arg;
            if (copy_from_user(&local_bool, bool_data, sizeof(LcdBoolArgs_t))) {
                status = -EIO;
                break;
            }
            lcdscrollhoriz(lcdi2c_gDescriptor, local_bool.value == 1);
            break;
        case LCD_IOCTL_SCROLLVERT:
            LcdScrollArgs_t *scroll_data = (LcdScrollArgs_t *) arg;
            LcdScrollArgs_t local_scroll;
            if(copy_from_user(&local_scroll, scroll_data, sizeof(LcdScrollArgs_t))) {
                status = -EIO;
                break;
            }
            lcdscrollvert(lcdi2c_gDescriptor, local_scroll.line, sizeof(local_scroll.line), local_scroll.direction);
            break;
        case LCD_IOCTL_GETCUSTOMCHAR:
            custom_char = (LcdCustomCharArgs_t *) arg;
            for (i = 0; i < 8; i++)
                put_user(custom_char->custom_char[i], &lcdi2c_gDescriptor->custom_chars[custom_char->index][i]);
            break;
        case LCD_IOCTL_SETCUSTOMCHAR:
            to_copy = copy_from_user(&local_custom_char, (void*) arg, sizeof(LcdCustomCharArgs_t));
            if (to_copy) {
                status = -EIO;
                break;
            }
            lcdcustomchar(lcdi2c_gDescriptor, local_custom_char.index, local_custom_char.custom_char);
            break;
        case LCD_IOCTL_CLEAR:
            lcdclear(lcdi2c_gDescriptor);
            break;
        default:
            dev_err(lcdi2c_gDescriptor->driver_data.lcdi2c_device, "Unknown IOCTL: 0x%02X\n", ioctl_num);
            break;
    }
    if (status!= SUCCESS)
        dev_err(lcdi2c_gDescriptor->driver_data.lcdi2c_device, "IOCTL failed: 0x%02X\n", ioctl_num);
    SEM_UP(lcdi2c_gDescriptor);

    return status;
}

static ssize_t lcdi2c_reset(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }


    if (count > 0 && buf[0] == '1')
        lcdinit(lcdi2c_gDescriptor, topo);

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_backlight(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count) {
    u8 res;
    int er;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    er = kstrtou8(buf, 10, &res);
    if (er == 0) {
        lcdsetbacklight(lcdi2c_gDescriptor, res);
        er = count;
    } else if (er == -ERANGE)
        dev_err(dev, "Brightness parameter out of range (0-255).");
    else if (er == -EINVAL)
        dev_err(dev, "Brightness parameter has numerical value. \"%s\" was given", buf);
    else
        dev_err(dev, "Brightness parameter wasn't properly converted! err: %d", er);

    SEM_UP(lcdi2c_gDescriptor);
    return er;
}

static ssize_t lcdi2c_backlight_show(struct device *dev,
                                     struct device_attribute *attr, char *buf) {
    int count = 0;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%d", lcdi2c_gDescriptor->backlight);

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_cursorpos(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (count >= 2) {
        count = 2;
        lcdsetcursor(lcdi2c_gDescriptor, buf[0], buf[1]);
    }

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_cursorpos_show(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf) {
    ssize_t count = 0;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (buf) {
        count = 2;
        buf[0] = lcdi2c_gDescriptor->column;
        buf[1] = lcdi2c_gDescriptor->row;
    }

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_data(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf, size_t count) {
    u8 i, addr, memaddr;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (count > 0) {
        memaddr = lcdi2c_gDescriptor->column + (lcdi2c_gDescriptor->row * lcdi2c_gDescriptor->organization.columns);
        for (i = 0; i < count; i++) {
            addr = (memaddr + i) % (lcdi2c_gDescriptor->organization.columns * lcdi2c_gDescriptor->organization.rows);
            lcdi2c_gDescriptor->raw_data[addr] = buf[i];
        }
        lcdflushbuffer(lcdi2c_gDescriptor);
    }

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_data_show(struct device *dev,
                                struct device_attribute *attr, char *buf) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (buf) {
        for (u8 i = 0; i < (lcdi2c_gDescriptor->organization.columns * lcdi2c_gDescriptor->organization.rows); i++) {
            buf[i] = lcdi2c_gDescriptor->raw_data[i];
        }
    }

    SEM_UP(lcdi2c_gDescriptor);
    return (lcdi2c_gDescriptor->organization.columns * lcdi2c_gDescriptor->organization.rows);
}

static ssize_t lcdi2c_meta_show(struct device *dev,
                                struct device_attribute *attr, char *buf) {

    ssize_t count = 0;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }


    if (buf) {
        char tmp[SHORT_STR_LEN], lines[META_BUFFER_LEN];
        memset(lines, 0, META_BUFFER_LEN);
        for (int i = 0; i < lcdi2c_gDescriptor->organization.rows; i++) {
            snprintf(tmp, SHORT_STR_LEN, "%d: 0x%02X, ", i, lcdi2c_gDescriptor->organization.addresses[i]);
            strncat(lines, tmp, META_BUFFER_LEN);
        }

        count = snprintf(buf, PAGE_SIZE,
                            "---\n"
                            "metadata:\n"
                            "       show-welcome-screen: %d\n"
                            "       topology: %d\n"
                            "       topology-name: %s\n"
                            "       rows: %d\n"
                            "       columns: %d\n"
                            "       rows-offsets: {%s}\n"
                            "       raw-data-len: %d\n"
                            "       line-len: %d\n"
                            "       pins: {rs: %d, rw: %d, e: %d, backlight: %d,}\n"
                            "       data-lines: {4: %d, 5: %d, 6: %d, 7: %d,}\n"
                            "       busno: %d\n"
                            "       reg: 0x%02X\n"
                            "       ioctls:\n",
                         lcdi2c_gDescriptor->show_welcome_screen,
                         lcdi2c_gDescriptor->organization.topology,
                         lcdi2c_gDescriptor->organization.toponame,
                         lcdi2c_gDescriptor->organization.rows,
                         lcdi2c_gDescriptor->organization.columns,
                         lines,
                         LCD_BUFFER_SIZE,
                         LCD_MAX_LINE_LENGTH,
                         PIN_RS, PIN_RW, PIN_EN, PIN_BACKLIGHT,
                         PIN_DB4, PIN_DB5, PIN_DB6, PIN_DB7,
                         lcdi2c_gDescriptor->driver_data.client->adapter->nr,
                         lcdi2c_gDescriptor->driver_data.client->addr);

        for (int i = 0; i < (sizeof(ioControls) / sizeof(IOCTLDescription_t)); i++) {
            count += snprintf(lines, META_BUFFER_LEN, "                 %s: 0x%02X\n",
                              ioControls[i].name, ioControls[i].ioctl_code);
            strncat(buf, lines, PAGE_SIZE);
        }
        count += snprintf(lines, META_BUFFER_LEN, "...\n");
        strncat(buf, lines, PAGE_SIZE);
    }

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_cursor(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf, size_t count) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (count > 0) {
        lcdi2c_gDescriptor->cursor = (buf[0] == '1');
        lcdcursor(lcdi2c_gDescriptor, lcdi2c_gDescriptor->cursor);
    }

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_cursor_show(struct device *dev,
                                  struct device_attribute *attr, char *buf) {
    ssize_t count = 0;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%c", lcdi2c_gDescriptor->cursor ? '1' : '0');

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_blink(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (count > 0) {
        lcdi2c_gDescriptor->blink = (buf[0] == '1');
        lcdblink(lcdi2c_gDescriptor, lcdi2c_gDescriptor->blink);
    }

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_blink_show(struct device *dev,
                                 struct device_attribute *attr, char *buf) {
    ssize_t count = 0;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%c", lcdi2c_gDescriptor->blink ? '1' : '0');

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_home(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (count > 0 && buf[0] == '1')
        lcdhome(lcdi2c_gDescriptor);

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_clear(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (count > 0 && buf[0] == '1')
        lcdclear(lcdi2c_gDescriptor);

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_scrollhz(struct device *dev, struct device_attribute *attr,
                               const char *buf, size_t count) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (count > 0)
        lcdscrollhoriz(lcdi2c_gDescriptor, buf[0] - '0');

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_customchar(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count) {
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if ((count > 0 && (count % 9)) || count == 0) {
        dev_err(dev, "incomplete character bitmap definition\n");
        SEM_UP(lcdi2c_gDescriptor);
        return -ETOOSMALL;
    }

    for (int i = 0; i < count; i += 9) {
        if (buf[i] > 7) {
            dev_err(dev, "%d is out of range, valid range is 0-7\n", buf[i]);
            SEM_UP(lcdi2c_gDescriptor);
            return -ETOOSMALL;
        }
        lcdcustomchar(lcdi2c_gDescriptor, buf[i], buf + i + 1);
    }

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_customchar_show(struct device *dev,
                                      struct device_attribute *attr, char *buf) {
    ssize_t count = 0;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    for (int c = 0; c < 8; c++) {
        buf[c * 9] = c;
        count++;
        for (int i = 0; i < 8; i++) {
            buf[c * 9 + (i + 1)] = lcdi2c_gDescriptor->custom_chars[c][i];
            count++;
        }
    }

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_char(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf, size_t count) {
    u8 lcd_mem_addr;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    if (buf && count > 0) {
        lcd_mem_addr = (1 + lcdi2c_gDescriptor->column + (lcdi2c_gDescriptor->row * lcdi2c_gDescriptor->organization.columns)) % LCD_BUFFER_SIZE;
        lcdwrite(lcdi2c_gDescriptor, buf[0]);
        lcdi2c_gDescriptor->column = (lcd_mem_addr % lcdi2c_gDescriptor->organization.columns);
        lcdi2c_gDescriptor->row = (lcd_mem_addr / lcdi2c_gDescriptor->organization.columns);
        lcdsetcursor(lcdi2c_gDescriptor, lcdi2c_gDescriptor->column, lcdi2c_gDescriptor->row);
    }

    SEM_UP(lcdi2c_gDescriptor);
    return 1;
}

static ssize_t lcdi2c_char_show(struct device *dev,
                                struct device_attribute *attr, char *buf) {
    u8 lcd_mem_addr;
    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    lcd_mem_addr = (lcdi2c_gDescriptor->column + (lcdi2c_gDescriptor->row * lcdi2c_gDescriptor->organization.columns))
                      % LCD_BUFFER_SIZE;
    buf[0] = lcdi2c_gDescriptor->raw_data[lcd_mem_addr];

    SEM_UP(lcdi2c_gDescriptor);
    return 1;
}


static ssize_t lcdi2c_line_show(struct device *dev,
                                struct device_attribute *attr, char *buf) {
    u8 lcd_mem_addr;
    ssize_t count = 0;

    if (SEM_DOWN(lcdi2c_gDescriptor)) {
        return -ERESTARTSYS;
    }

    lcd_mem_addr = (lcdi2c_gDescriptor->row * lcdi2c_gDescriptor->organization.columns) % LCD_BUFFER_SIZE;
    for (int i = 0; i < lcdi2c_gDescriptor->organization.columns; i++) {
        buf[i] = lcdi2c_gDescriptor->raw_data[lcd_mem_addr + i];
        count++;
    }

    SEM_UP(lcdi2c_gDescriptor);
    return count;
}

static ssize_t lcdi2c_scrollvert(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count) {

    if (count > 0) {
        if (SEM_DOWN(lcdi2c_gDescriptor)) {
            return -ERESTARTSYS;
        }
        for (uint row = 1; row < lcdi2c_gDescriptor->organization.rows; row++) {
            memcpy(lcdi2c_gDescriptor->raw_data + ((row - 1) * lcdi2c_gDescriptor->organization.columns),
                   lcdi2c_gDescriptor->raw_data + (row * lcdi2c_gDescriptor->organization.columns),
                   lcdi2c_gDescriptor->organization.columns);
        }
        memset(lcdi2c_gDescriptor->raw_data + (lcdi2c_gDescriptor->organization.rows - 1) * lcdi2c_gDescriptor->organization.columns,
               ' ', lcdi2c_gDescriptor->organization.columns);
        lcdflushbuffer(lcdi2c_gDescriptor);
        SEM_UP(lcdi2c_gDescriptor);
    }
    return count;
}


module_i2c_driver(lcdi2c_driver);


