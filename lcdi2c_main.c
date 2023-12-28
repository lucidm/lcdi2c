#include "lcdi2c_main.h"


static int busno = 1;      //I2C Bus number
static uint address = DEFAULT_CHIP_ADDRESS; //Device address
static uint topo = LCD_DEFAULT_ORGANIZATION;
static uint cursor = 1;
static uint blink = 1;
static uint swscreen = 0;
static char *wscreen = DEFAULT_WS;
static const IOCTLDescription_t ioControls[] = {
        {.ioctl_code = LCD_IOCTL_GETCHAR, .name = "GETCHAR",},
        {.ioctl_code = LCD_IOCTL_SETCHAR, .name = "SETCHAR",},
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
        {.ioctl_code = LCD_IOCTL_GETCUSTOMCHAR, .name = "GETCUSTOMCHAR"},
        {.ioctl_code = LCD_IOCTL_SETCUSTOMCHAR, .name = "SETCUSTOMCHAR"},
        {.ioctl_code = LCD_IOCTL_CLEAR, .name = "CLEAR"},
};


static struct i2c_client *i2c_gClient;
static struct i2c_adapter *i2c_gAdapter;
static LcdHandler_t *lcd_gHandler;
static int major = DEVICE_MAJOR;
static int minor = 0;
static struct class *lcdi2c_class;
static struct device *lcdi2c_device;

module_param(busno, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(address, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param_array(pinout, uint, NULL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(cursor, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(blink, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(topo, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(major, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(swscreen, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(wscreen, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

MODULE_PARM_DESC(busno, " I2C Bus number, default 1");
MODULE_PARM_DESC(address, " LCD I2C Address, default 0x27");
MODULE_PARM_DESC(pinout, " I2C module pinout configuration, eight "
                         "numbers\n\t\trepresenting following LCD module"
                         "pins in order: RS,RW,E,D4,D5,D6,D7,\n"
                         "\t\tdefault 0,1,2,3,4,5,6,7");
MODULE_PARM_DESC(cursor, " Show cursor at start 1 - Yes, 0 - No, default 1");
MODULE_PARM_DESC(blink, " Blink cursor 1 - Yes, 0 - No, defualt 1");
MODULE_PARM_DESC(major, " Device major number, default 0");
MODULE_PARM_DESC(topo, " Display organization, following values are currently supported:\n"
                       "\t\t0 - 40x2\n"
                       "\t\t1 - 20x4\n"
                       "\t\t2 - 20x2\n"
                       "\t\t3 - 16x4\n"
                       "\t\t4 - 16x2\n"
                       "\t\t5 - 16x1 Type 1\n"
                       "\t\t6 - 16x1 Type 2\n"
                       "\t\t7 - 8x2\n"
                       "\t\tDefault set to 16x2");
MODULE_PARM_DESC(swscreen, " Show welcome screen on load, 1 - Yes, 0 - No, default 0");
MODULE_PARM_DESC(wscreen, " Welcome screen string, default \""DEFAULT_WS"\"");

static int lcdi2c_open(struct inode *inode, struct file *file) {
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -EBUSY;
    }

    lcd_gHandler->open_cnt++;
    lcd_gHandler->use_cnt = 0;
    try_module_get(THIS_MODULE);
    up(&lcd_gHandler->sem);

    return SUCCESS;
}

static int lcdi2c_release(struct inode *inode, struct file *file) {
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -EBUSY;
    }

    lcd_gHandler->open_cnt--;

    module_put(THIS_MODULE);
    up(&lcd_gHandler->sem);
    return SUCCESS;
}

static ssize_t lcdi2c_fopread(struct file *file, char __user *buffer,
                              size_t length, loff_t *offset) {
    u8 i = 0;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -EBUSY;
    }

    if (lcd_gHandler->use_cnt == (lcd_gHandler->organization.columns * lcd_gHandler->organization.rows))
        return 0;

    while (i < length && i < (lcd_gHandler->organization.columns * lcd_gHandler->organization.rows)) {
        put_user(lcd_gHandler->buffer[i], buffer++);
        lcd_gHandler->use_cnt++;
        i++;
    }

    i %= (lcd_gHandler->organization.columns * lcd_gHandler->organization.rows);
    ITOP(lcd_gHandler, i, lcd_gHandler->column, lcd_gHandler->row);
    lcdsetcursor(lcd_gHandler, lcd_gHandler->column, lcd_gHandler->row);
    (*offset) = i;
    up(&lcd_gHandler->sem);

    return i;
}

static ssize_t lcdi2c_fopwrite(struct file *file, const char __user *buffer,
                               size_t length, loff_t *offset) {
    u8 i, str[LCD_BUFFER_SIZE];

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -EBUSY;
    }

    for (i = 0; i < length && i < (lcd_gHandler->organization.columns * lcd_gHandler->organization.rows); i++)
        get_user(str[i], buffer + i);
    str[i] = 0;
    (*offset) = lcdprint(lcd_gHandler, str);

    up(&lcd_gHandler->sem);
    return i;
}

loff_t lcdi2c_lseek(struct file *file, loff_t offset, int orig) {
    u8 memaddr, oldoffset;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -EBUSY;
    }
    memaddr = lcd_gHandler->column + (lcd_gHandler->row * lcd_gHandler->organization.columns);
    oldoffset = memaddr;
    memaddr = (memaddr + (u8) offset) % (lcd_gHandler->organization.rows * lcd_gHandler->organization.columns);
    lcd_gHandler->column = (memaddr % lcd_gHandler->organization.columns);
    lcd_gHandler->row = (memaddr / lcd_gHandler->organization.columns);
    lcdsetcursor(lcd_gHandler, lcd_gHandler->column, lcd_gHandler->row);
    up(&lcd_gHandler->sem);

    return oldoffset;
}

static long lcdi2c_ioctl(struct file *file,
                         unsigned int ioctl_num,
                         unsigned long arg) {

    char *buffer = (char *) arg, ccb[10];
    u8 memaddr, i, ch;
    long status = SUCCESS;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -EBUSY;
    }

    switch (ioctl_num) {
        case LCD_IOCTL_SETCHAR:
            get_user(ch, buffer);
            memaddr = (1 + lcd_gHandler->column + (lcd_gHandler->row * lcd_gHandler->organization.columns)) % LCD_BUFFER_SIZE;
            lcdwrite(lcd_gHandler, ch);
            lcd_gHandler->column = (memaddr % lcd_gHandler->organization.columns);
            lcd_gHandler->row = (memaddr / lcd_gHandler->organization.columns);
            lcdsetcursor(lcd_gHandler, lcd_gHandler->column, lcd_gHandler->row);
            break;
        case LCD_IOCTL_GETCHAR:
            memaddr = (lcd_gHandler->column + (lcd_gHandler->row * lcd_gHandler->organization.columns)) % LCD_BUFFER_SIZE;
            ch = lcd_gHandler->buffer[memaddr];
            put_user(ch, buffer);
            break;
        case LCD_IOCTL_GETPOSITION:
            put_user(lcd_gHandler->column, buffer);
            put_user(lcd_gHandler->row, buffer + 1);
            break;
        case LCD_IOCTL_SETPOSITION:
            get_user(lcd_gHandler->column, buffer);
            get_user(lcd_gHandler->row, buffer + 1);
            lcdsetcursor(lcd_gHandler, lcd_gHandler->column, lcd_gHandler->row);
            break;
        case LCD_IOCTL_RESET:
            get_user(ch, buffer);
            if (ch == '1')
                lcdinit(lcd_gHandler, lcd_gHandler->organization.topology);
            break;
        case LCD_IOCTL_HOME:
            get_user(ch, buffer);
            if (ch == '1')
                lcdhome(lcd_gHandler);
            break;
        case LCD_IOCTL_GETCURSOR:
            put_user(lcd_gHandler->cursor ? '1' : '0', buffer);
            break;
        case LCD_IOCTL_SETCURSOR:
            get_user(ch, buffer);
            lcdcursor(lcd_gHandler, (ch == '1'));
            break;
        case LCD_IOCTL_GETBLINK:
            put_user(lcd_gHandler->blink ? '1' : '0', buffer);
            break;
        case LCD_IOCTL_SETBLINK:
            get_user(ch, buffer);
            lcdblink(lcd_gHandler, (ch == '1'));
            break;
        case LCD_IOCTL_GETBACKLIGHT:
            put_user(lcd_gHandler->backlight ? '1' : '0', buffer);
            break;
        case LCD_IOCTL_SETBACKLIGHT:
            get_user(ch, buffer);
            lcdsetbacklight(lcd_gHandler, (ch == '1'));
            break;
        case LCD_IOCTL_SCROLLHZ:
            get_user(ch, buffer);
            lcdscrollhoriz(lcd_gHandler, ch - '0');
            break;
        case LCD_IOCTL_GETCUSTOMCHAR:
            get_user(ch, buffer);
            for (i = 0; i < 8; i++)
                put_user(lcd_gHandler->custom_chars[ch][i], buffer + i + 1);
            break;
        case LCD_IOCTL_SETCUSTOMCHAR:
            for (i = 0; i < 9; i++)
                get_user(ccb[i], buffer + i);
            lcdcustomchar(lcd_gHandler, ccb[0], ccb + 1);
            break;
        case LCD_IOCTL_CLEAR:
            get_user(ch, buffer);
            if (ch == '1')
                lcdclear(lcd_gHandler);
            break;
        default:
            printk(KERN_INFO "Unknown IOCTL\n");
            break;
    }
    up(&lcd_gHandler->sem);

    return status;
}

static void set_welcome_message(LcdHandler_t *lcdData, char *welcome_msg) {
    strncpy(lcdData->welcome, strlen(welcome_msg) ? welcome_msg : DEFAULT_WS, WS_MAX_LEN);
}

static int lcdi2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    lcd_gHandler = (LcdHandler_t *) devm_kzalloc(&client->dev, sizeof(LcdHandler_t), GFP_KERNEL);

    if (!lcd_gHandler)
        return -ENOMEM;

    i2c_set_clientdata(client, lcd_gHandler);
    sema_init(&lcd_gHandler->sem, 1);
    lcd_gHandler->handle = client;
    lcd_gHandler->backlight = 1;
    lcd_gHandler->cursor = cursor;
    lcd_gHandler->blink = blink;
    lcd_gHandler->open_cnt = 0;
    lcd_gHandler->major = major;
    lcd_gHandler->show_welcome_screen = swscreen;

    set_welcome_message(lcd_gHandler, wscreen);

    lcdinit(lcd_gHandler, topo);
    if (lcd_gHandler->show_welcome_screen) {
        lcdprint(lcd_gHandler, lcd_gHandler->welcome);
    }

    dev_info(&client->dev, "%ux%u LCD using bus 0x%X, at address 0x%X",
             lcd_gHandler->organization.columns,
             lcd_gHandler->organization.rows, busno, address);
    return 0;
}

static void lcdi2c_remove(struct i2c_client *client) {
    LcdHandler_t *data = i2c_get_clientdata(client);

    dev_info(&client->dev, "going to be removed");
    if (data)
    {
        lcdfinalize(data);
        devm_kfree(&client->dev, data);
    }

}

/*
 * Driver data (common to all clients)
 */

static const struct i2c_device_id lcdi2c_id[] = {
        {"lcdi2c", 0},
        {},
};
MODULE_DEVICE_TABLE(i2c, lcdi2c_id);

static struct i2c_driver lcdi2c_driver = {
        .driver = {
                    .owner  = THIS_MODULE,
                    .name    = "lcdi2c",
                },
        .probe          = lcdi2c_probe,
        .remove         = lcdi2c_remove,
        .id_table    = lcdi2c_id,
};


#ifdef HAVE_PROC_OPS
static struct proc_ops lcdi2c_fops = {
        .proc_open = lcdi2c_open,
        .proc_read = lcdi2c_fopread,
        .proc_write = lcdi2c_fopwrite,
        .proc_lseek = lcdi2c_lseek,
        .proc_release = lcdi2c_release,
        .proc_ioctl = lcdi2c_ioctl,
};
#else
static struct file_operations lcdi2c_fops = {
        .read = lcdi2c_fopread,
        .write = lcdi2c_fopwrite,
        .llseek = lcdi2c_lseek,
        .unlocked_ioctl = lcdi2c_ioctl,
        .open = lcdi2c_open,
        .release = lcdi2c_release,
        .owner = THIS_MODULE,
};
#endif

static ssize_t lcdi2c_reset(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count) {
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }


    if (count > 0 && buf[0] == '1')
        lcdinit(lcd_gHandler, topo);

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_backlight(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count) {
    u8 res;
    int er;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    er = kstrtou8(buf, 10, &res);
    if (er == 0) {
        lcdsetbacklight(lcd_gHandler, res);
        er = count;
    } else if (er == -ERANGE)
        dev_err(dev, "Brightness parameter out of range (0-255).");
    else if (er == -EINVAL)
        dev_err(dev, "Brightness parameter has numerical value. \"%s\" was given", buf);
    else
        dev_err(dev, "Brightness parameter wasn't properly converted! err: %d", er);

    up(&lcd_gHandler->sem);
    return er;
}

static ssize_t lcdi2c_backlight_show(struct device *dev,
                                     struct device_attribute *attr, char *buf) {
    int count = 0;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%d", lcd_gHandler->backlight);

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_cursorpos(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count) {
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (count >= 2) {
        count = 2;
        lcdsetcursor(lcd_gHandler, buf[0], buf[1]);
    }

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_cursorpos_show(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf) {
    ssize_t count = 0;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (buf) {
        count = 2;
        buf[0] = lcd_gHandler->column;
        buf[1] = lcd_gHandler->row;
    }

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_data(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf, size_t count) {
    u8 i, addr, memaddr;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (count > 0) {
        memaddr = lcd_gHandler->column + (lcd_gHandler->row * lcd_gHandler->organization.columns);
        for (i = 0; i < count; i++) {
            addr = (memaddr + i) % (lcd_gHandler->organization.columns * lcd_gHandler->organization.rows);
            lcd_gHandler->buffer[addr] = buf[i];
        }
        lcdflushbuffer(lcd_gHandler);
    }

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_data_show(struct device *dev,
                                struct device_attribute *attr, char *buf) {
    u8 i = 0;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (buf) {
        for (i = 0; i < (lcd_gHandler->organization.columns * lcd_gHandler->organization.rows); i++) {
            buf[i] = lcd_gHandler->buffer[i];
        }
    }

    up(&lcd_gHandler->sem);
    return (lcd_gHandler->organization.columns * lcd_gHandler->organization.rows);
}

static ssize_t lcdi2c_meta_show(struct device *dev,
                                struct device_attribute *attr, char *buf) {

    ssize_t count = 0;
    char tmp[SHORT_STR_LEN], lines[META_BUFFER_LEN];

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }


    if (buf) {
        memset(lines, 0, META_BUFFER_LEN);
        for (int i = 0; i < lcd_gHandler->organization.rows; i++) {
            snprintf(tmp, SHORT_STR_LEN, "%d: 0x%02X, ", i, lcd_gHandler->organization.addresses[i]);
            strncat(lines, tmp, META_BUFFER_LEN);
        }

        count = snprintf(buf, PAGE_SIZE,
                            "---\n"
                            "metadata:\n"
                            "       show welcome screen: %d\n"
                            "       topology: %d\n"
                            "       topology name: %s\n"
                            "       rows: %d\n"
                            "       columns: %d\n"
                            "       rows-offsets: {%s}\n"
                            "       pins: {rs: %d, rw: %d, e: %d, backlight: %d,}\n"
                            "       data-lines: {4: %d, 5: %d, 6: %d, 7: %d,}\n"
                            "       busno: %d\n"
                            "       reg: 0x%02X\n"
                            "       ioctls:\n",
                         lcd_gHandler->show_welcome_screen,
                         lcd_gHandler->organization.topology,
                         lcd_gHandler->organization.toponame,
                         lcd_gHandler->organization.rows,
                         lcd_gHandler->organization.columns,
                         lines,
                         PIN_RS, PIN_RW, PIN_EN,
                         PIN_BACKLIGHTON,
                         PIN_DB4, PIN_DB5, PIN_DB6, PIN_DB7,
                         lcd_gHandler->handle->adapter->nr,
                         lcd_gHandler->handle->addr);

        for (int i = 0; i < (sizeof(ioControls) / sizeof(IOCTLDescription_t)); i++) {
            count += snprintf(lines, META_BUFFER_LEN, "                 %s: 0x%02X\n",
                              ioControls[i].name, ioControls[i].ioctl_code);
            strncat(buf, lines, PAGE_SIZE);
        }
        count += snprintf(lines, META_BUFFER_LEN, "...\n");
        strncat(buf, lines, PAGE_SIZE);
    }

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_cursor(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf, size_t count) {
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (count > 0) {
        lcd_gHandler->cursor = (buf[0] == '1');
        lcdcursor(lcd_gHandler, lcd_gHandler->cursor);
    }

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_cursor_show(struct device *dev,
                                  struct device_attribute *attr, char *buf) {
    ssize_t count = 0;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%c", lcd_gHandler->cursor ? '1' : '0');

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_blink(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count) {
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (count > 0) {
        lcd_gHandler->blink = (buf[0] == '1');
        lcdblink(lcd_gHandler, lcd_gHandler->blink);
    }

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_blink_show(struct device *dev,
                                 struct device_attribute *attr, char *buf) {
    ssize_t count = 0;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%c", lcd_gHandler->blink ? '1' : '0');

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_home(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count) {
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (count > 0 && buf[0] == '1')
        lcdhome(lcd_gHandler);

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_clear(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count) {
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (count > 0 && buf[0] == '1')
        lcdclear(lcd_gHandler);

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_scrollhz(struct device *dev, struct device_attribute *attr,
                               const char *buf, size_t count) {
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (count > 0)
        lcdscrollhoriz(lcd_gHandler, buf[0] - '0');

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_customchar(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count) {
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if ((count > 0 && (count % 9)) || count == 0) {
        dev_err(&i2c_gClient->dev, "incomplete character bitmap definition\n");
        up(&lcd_gHandler->sem);
        return -ETOOSMALL;
    }

    for (int i = 0; i < count; i += 9) {
        if (buf[i] > 7) {
            dev_err(&i2c_gClient->dev, "%d is out of range, valid range is 0-7\n", buf[i]);
            up(&lcd_gHandler->sem);
            return -ETOOSMALL;
        }
        lcdcustomchar(lcd_gHandler, buf[i], buf + i + 1);
    }

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_customchar_show(struct device *dev,
                                      struct device_attribute *attr, char *buf) {
    ssize_t count = 0;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    for (int c = 0; c < 8; c++) {
        buf[c * 9] = c;
        count++;
        for (int i = 0; i < 8; i++) {
            buf[c * 9 + (i + 1)] = lcd_gHandler->custom_chars[c][i];
            count++;
        }
    }

    up(&lcd_gHandler->sem);
    return count;
}

static ssize_t lcdi2c_char(struct device *dev,
                           struct device_attribute *attr,
                           const char *buf, size_t count) {
    u8 lcd_mem_addr;

    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    if (buf && count > 0) {
        lcd_mem_addr = (1 + lcd_gHandler->column + (lcd_gHandler->row * lcd_gHandler->organization.columns)) % LCD_BUFFER_SIZE;
        lcdwrite(lcd_gHandler, buf[0]);
        lcd_gHandler->column = (lcd_mem_addr % lcd_gHandler->organization.columns);
        lcd_gHandler->row = (lcd_mem_addr / lcd_gHandler->organization.columns);
        lcdsetcursor(lcd_gHandler, lcd_gHandler->column, lcd_gHandler->row);
    }

    up(&lcd_gHandler->sem);
    return 1;
}

static ssize_t lcdi2c_char_show(struct device *dev,
                                struct device_attribute *attr, char *buf) {
    u8 lcd_mem_addr;
    if (down_interruptible(&lcd_gHandler->sem)) {
        return -ERESTARTSYS;
    }

    lcd_mem_addr = (lcd_gHandler->column + (lcd_gHandler->row * lcd_gHandler->organization.columns))
                      % LCD_BUFFER_SIZE;
    buf[0] = lcd_gHandler->buffer[lcd_mem_addr];

    up(&lcd_gHandler->sem);
    return 1;
}

static const struct attribute_group i2clcd_device_attr_group = {
        .attrs = (struct attribute **) i2clcd_attrs,
};

static int lcdi2c_dev_uevent(const struct device *dev, struct kobj_uevent_env *env) {
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int __init i2clcd857_init(void) {
    int ret;
    struct i2c_board_info board_info = {
            .type = "lcdi2c",
            .addr = address,

    };

    i2c_gAdapter = i2c_get_adapter(busno);
    if (!i2c_gAdapter) return -EINVAL;

    i2c_gClient = i2c_new_client_device(i2c_gAdapter, &board_info);
    if (!i2c_gClient) return -EINVAL;

    ret = i2c_add_driver(&lcdi2c_driver);
    if (ret) {
        i2c_put_adapter(i2c_gAdapter);
        return ret;
    }

    if (!i2c_check_functionality(i2c_gAdapter, I2C_FUNC_I2C)) {
        i2c_put_adapter(i2c_gAdapter);
        dev_err(&i2c_gClient->dev, "no algorithms associated to i2c bus\n");
        return -ENODEV;
    }

    i2c_put_adapter(i2c_gAdapter);

    major = register_chrdev(major, DEVICE_NAME, &lcdi2c_fops);
    if (major < 0) {
        dev_warn(&i2c_gClient->dev, "failed to register device with error: %d\n",
                 major);
        goto failed_chrdev;
    }

    lcdi2c_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);
    if (IS_ERR(lcdi2c_class)) {
        dev_warn(&i2c_gClient->dev, "class creation failed %s\n", DEVICE_CLASS_NAME);
        goto failed_class;
    }

    lcdi2c_class->dev_uevent = lcdi2c_dev_uevent;

    lcdi2c_device = device_create(lcdi2c_class, NULL, MKDEV(major, minor), NULL,
                                  DEVICE_NAME);
    if (IS_ERR(lcdi2c_device)) {
        dev_warn(&i2c_gClient->dev, "device %s creation failed\n", DEVICE_NAME);
        goto failed_device;
    }

    if (sysfs_create_group(&lcdi2c_device->kobj, &i2clcd_device_attr_group)) {
        dev_warn(&i2c_gClient->dev, "device attribute group creation failed\n");
        goto failed_device;
    }

    dev_info(&i2c_gClient->dev, "registered with major %u\n", major);

    return ret;

    failed_device:
    class_unregister(lcdi2c_class);
    class_destroy(lcdi2c_class);
    failed_class:
    unregister_chrdev(major, DEVICE_NAME);
    failed_chrdev:
    i2c_unregister_device(i2c_gClient);
    i2c_del_driver(&lcdi2c_driver);
    return -1;
}


static void __exit i2clcd857_exit(void) {

    unregister_chrdev(major, DEVICE_NAME);

    sysfs_remove_group(&lcdi2c_device->kobj, &i2clcd_device_attr_group);
    device_destroy(lcdi2c_class, MKDEV(major, 0));
    class_unregister(lcdi2c_class);
    class_destroy(lcdi2c_class);
    unregister_chrdev(major, DEVICE_NAME);

    if (i2c_gClient)
        i2c_unregister_device(i2c_gClient);

    i2c_del_driver(&lcdi2c_driver);
}


module_init(i2clcd857_init);
module_exit(i2clcd857_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jarek Zok <jarekzok@gmail.com>");
MODULE_DESCRIPTION(LCDI2C8574_DESCRIPTION);
MODULE_VERSION(LCDI2C8574_VERSION);

