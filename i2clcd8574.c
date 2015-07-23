#include "i2clcd8574.h"

static uint busno = 1;      //I2C Bus number
static uint address = DEFAULT_CHIP_ADDRESS; //Device address
static uint topo = LCD_DEFAULT_ORGANIZATION;
static uint cursor = 1;
static uint blink = 1;

static struct i2c_client *client;
static struct i2c_adapter *adapter;
static LcdData_t *data;
static int major = DEVICE_MAJOR;
static int minor = 0;
static struct class *lcdi2c_class = NULL;
static struct device *lcdi2c_device = NULL;

module_param(busno, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(address, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param_array(pinout, uint, NULL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(cursor, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(blink, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(topo, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_param(major, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

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
                        "\t\tDefault set to 16x2");

static int lcdi2c_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "attepmting to open device\n");
    try_module_get(THIS_MODULE);
    return SUCCESS;
}

static int lcdi2c_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "attepmting to release device\n");
    module_put(THIS_MODULE);
    return SUCCESS;
}

static ssize_t lcdi2c_read(struct file *file, char __user *buffer, 
			   size_t length, loff_t *offset)
{
    printk(KERN_INFO "read device\n");
    return length;
}

static ssize_t lcdi2c_write(struct file *file, const char __user *buffer, 
			    size_t length, loff_t *offset)
{
    printk(KERN_INFO "write device\n");
    return length;
}


static int lcdi2c_probe(struct i2c_client *client, struct i2c_device_id *id)
{
    
    data = (LcdData_t *) devm_kzalloc(&client->dev, sizeof(LcdData_t), 
				      GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    
    i2c_set_clientdata(client, data);
    sema_init(&data->sem, 1);

    data->row = 0;
    data->column = 0;
    data->handle = client;
    data->backlight = 1;
    data->cursor = cursor;
    data->blink = blink;

    lcdinit(data, topo);
    lcdprint(data, "I2C HD44780 v 0.1.0\njarekzok@gmail.com");

    dev_info(&client->dev, "%ux%u LCD using bus 0x%X, at address 0x%X", 
	     data->organization.columns, 
	     data->organization.rows, busno, address);
    
    return 0;
}

static int lcdi2c_remove(struct i2c_client *client)
{
	LcdData_t *data = i2c_get_clientdata(client);
        
        dev_info(&client->dev, "going to be removed");
        if (data)
            lcdfinalize(data);
        
	return 0;
}

/*
 * Driver data (common to all clients)
 */

static const unsigned short normal_i2c[] = { DEFAULT_CHIP_ADDRESS, 
					     DEFAULT_CHIP_ADDRESS + 1, 
                                             DEFAULT_CHIP_ADDRESS + 2, 
					     DEFAULT_CHIP_ADDRESS +3, 
                                             I2C_CLIENT_END };
        

static const struct i2c_device_id lcdi2c_id[] = {
	{ "lcdi2c", 0 },
        { },
};
MODULE_DEVICE_TABLE(i2c, lcdi2c_id);

static struct i2c_driver lcdi2c_driver = {
	.driver = {
                .owner  = THIS_MODULE,
		.name	= "lcdi2c",
	},
        .probe          = lcdi2c_probe,
        .remove         = lcdi2c_remove,
	.id_table	= lcdi2c_id,
	.address_list	= normal_i2c,
};


static struct file_operations lcdi2c_fops = {
	.read = lcdi2c_read,
	.write = lcdi2c_write,
//	.ioctl = NULL, //lcdi2c_ioctl,
	.open = lcdi2c_open,
	.release = lcdi2c_release,
        .owner = THIS_MODULE,
};

static ssize_t lcdi2c_reset(struct device* dev, struct device_attribute* attr, 
			    const char* buf, size_t count)
{
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (count > 0 && buf[0] == '1')
        lcdinit(data, topo);
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_backlight(struct device* dev, 
				struct device_attribute* attr, 
				const char* buf, size_t count)
{
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (count > 0)
        lcdsetbacklight(data, (buf[0] == '1'));
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_backlight_show(struct device *dev, 
				     struct device_attribute *attr, char *buf)
{
    int count = 0;
    
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%c", data->backlight ? '1' : '0');
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_cursorpos(struct device* dev, 
				struct device_attribute* attr, 
				const char* buf, size_t count)
{
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (count >= 2)
    {
        count = 2;
        lcdsetcursor(data, buf[0], buf[1]);
    }
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_cursorpos_show(struct device *dev, 
				     struct device_attribute *attr, 
				     char *buf)
{
    ssize_t count = 0;
    
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (buf)
    {
        count = 2;
        buf[0] = data->column;
        buf[1] = data->row;
    }
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_data(struct device* dev, 
			   struct device_attribute* attr, 
			   const char* buf, size_t count)
{
    uint8_t i, ic, ir, memaddr;
    
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (count > 0)
    {
        memset(data->buffer, 0x20, LCD_BUFFER_SIZE); //Fill buffer with spaces
        for (i = 0; i < (count % LCD_BUFFER_SIZE); i++)
        {
            ic = i % data->organization.columns;
            ir = i / data->organization.columns;
            memaddr = ic + data->organization.addresses[ir];
            data->buffer[memaddr] = buf[i];
        }
        
        lcdhome(data);
        lcdflushbuffer(data);
    }
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_data_show(struct device *dev, 
				struct device_attribute *attr, char *buf)
{
    uint8_t i=0, ic, ir, memaddr;
    
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (buf)
    {
        for (i = 0; i < LCD_BUFFER_SIZE; i++)
        {
            ic = i % data->organization.columns;
            ir = i / data->organization.columns;
            memaddr = ic + data->organization.addresses[ir];
            buf[i] = data->buffer[memaddr];
        }
    }
    
    up(&data->sem);
    return LCD_BUFFER_SIZE;
}

static ssize_t lcdi2c_meta_show(struct device *dev, 
				struct device_attribute *attr, char *buf)
{
    ssize_t count = 0;
    char tmp[12], lines[54];
    
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (buf)
    {
        int i;
        memset(lines, 0, 54);
        for (i=0; i < data->organization.rows; i++)
        {
            snprintf(tmp, 12, "R[%d]=0x%02X ",i, data->organization.addresses[i]);
            strncat(lines, tmp, 54);
        }
        
        count = snprintf(buf, PAGE_SIZE,  
                         "Topology:%s=%d\n"
                         "Rows:%d\n"
                         "Columns:%d\n"
                         "Lines addresses:%s\n"
                         "Pins:RS=%d RW=%d E=%d BCKLIGHT=%d D[4]=%d D[5]=%d D[6]=%d D[7]=%d\n", 
                         data->organization.toponame, 
                         data->organization.topology, 
                         data->organization.rows,
                         data->organization.columns,
                         lines,
                         PIN_RS, PIN_RW, PIN_EN,
                         PIN_BACKLIGHTON,
                         PIN_DB4, PIN_DB5, PIN_DB6, PIN_DB7
                        );
    }
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_cursor(struct device* dev, 
			     struct device_attribute* attr, 
			     const char* buf, size_t count)
{
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (count > 0)
    {
        data->cursor = (buf[0] == '1');
        lcdcursor(data, data->cursor);
    }
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_cursor_show(struct device *dev, 
				  struct device_attribute *attr, char *buf)
{
    int count = 0;
    
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%c", data->cursor ? '1' : '0');
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_blink(struct device* dev, 
			    struct device_attribute* attr, 
			    const char* buf, size_t count)
{
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (count > 0)
    {
        data->blink = (buf[0] == '1');
        lcdblink(data, data->blink);
    }
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_blink_show(struct device *dev, 
				 struct device_attribute *attr, char *buf)
{
    int count = 0;
    
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%c", data->blink ? '1' : '0');
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_home(struct device* dev, struct device_attribute* attr, 
			   const char* buf, size_t count)
{
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (count > 0 && buf[0] == '1')
        lcdhome(data);
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_clear(struct device* dev, struct device_attribute* attr, 
			    const char* buf, size_t count)
{
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (count > 0 && buf[0] == '1')
        lcdclear(data);
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_scrollhz(struct device* dev, struct device_attribute* attr, 
			       const char* buf, size_t count)
{
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    if (count > 0)
        lcdscrollhoriz(data, buf[0] - '0');
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_customchar(struct device* dev, 
				 struct device_attribute* attr, 
				 const char* buf, size_t count)
{
    uint8_t i;
    
    if ((count > 0 && (count % 9)) || count == 0)
    {
         dev_err(&client->dev, "incomplete character bitmap definition\n");
         return 0;
    }
    
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    for (i = 0; i < count; i+=9)
    {
        if (buf[i] > 7)
        {
            dev_err(&client->dev, "character number %d can only have values"
				  "starting from 0 to 7\n", buf[i]);
            continue;
        }
        
        lcdcustomchar(data, buf[i], buf + i + 1);
    }
    
    up(&data->sem);
    return count;
}

static ssize_t lcdi2c_customchar_show(struct device *dev, 
				      struct device_attribute *attr, char *buf)
{
    int count = 0, c, i;
    
    if(down_interruptible(&data->sem))
        return -ERESTARTSYS;
    
    for (c = 0; c < 8; c++)
    {
        buf[c * 8] = c;
        for (i = 0; i < 8; i++)
        {
            buf[i + c * 8] = data->customchars[c][i];
            count++;
        }
    }
    
    up(&data->sem);
    return count;
}

DEVICE_ATTR(reset, 0664, NULL, lcdi2c_reset);
DEVICE_ATTR(backlight, 0664,lcdi2c_backlight_show, lcdi2c_backlight);
DEVICE_ATTR(cursorpos, 0664, lcdi2c_cursorpos_show, lcdi2c_cursorpos);
DEVICE_ATTR(data, 0664, lcdi2c_data_show, lcdi2c_data);
DEVICE_ATTR(meta, 0444, lcdi2c_meta_show, NULL);
DEVICE_ATTR(cursor, 0664, lcdi2c_cursor_show, lcdi2c_cursor);
DEVICE_ATTR(blink, 0664, lcdi2c_blink_show, lcdi2c_blink);
DEVICE_ATTR(home, 0664, NULL, lcdi2c_home);
DEVICE_ATTR(clear, 0664, NULL, lcdi2c_clear);
DEVICE_ATTR(scrollhz, 0664, NULL, lcdi2c_scrollhz);
DEVICE_ATTR(customchar, 0664, lcdi2c_customchar_show, lcdi2c_customchar);

static const struct attribute *i2clcd_attrs[] = {
	&dev_attr_reset.attr,
	&dev_attr_backlight.attr,
	&dev_attr_cursorpos.attr,
	&dev_attr_data.attr,
	&dev_attr_meta.attr,
        &dev_attr_cursor.attr,
        &dev_attr_blink.attr,
        &dev_attr_home.attr,
        &dev_attr_clear.attr,
        &dev_attr_scrollhz.attr,
        &dev_attr_customchar.attr,
	NULL,
};

static const struct attribute_group i2clcd_device_attr_group = {
	.attrs = (struct attribute **) i2clcd_attrs,
};

static int __init i2clcd857_init(void)
{
    int ret;
    struct i2c_board_info board_info = {
                    .type = "lcdi2c",
                    .addr = address,
                    
    };
    
    adapter = i2c_get_adapter(busno);
    if (!adapter) return -EINVAL;
    
     client = i2c_new_device(adapter, &board_info);
     if (!client) return -EINVAL;
    
    ret = i2c_add_driver(&lcdi2c_driver);
    if (ret) 
    {
        i2c_put_adapter(adapter);
        return ret;
    }
    
    if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) 
    {
        i2c_put_adapter(adapter);
        dev_err(&client->dev, "no algorithms associated to i2c bus\n");
        return -ENODEV;
    }
    
    i2c_put_adapter(adapter);

    major = register_chrdev(major, DEVICE_NAME, &lcdi2c_fops);
    if (major < 0)
    {
        dev_warn(&client->dev, " failed to register device with error: %d\n", 
		 major);
        goto failed_chrdev;
    }
    
    lcdi2c_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);
    if (IS_ERR(lcdi2c_class))
    {
        dev_warn(&client->dev, " class creation failed %s\n", DEVICE_CLASS_NAME);
        goto failed_class;
    }

    lcdi2c_device = device_create(lcdi2c_class, NULL, MKDEV(major, minor), NULL, 
				  DEVICE_NAME);
    if (IS_ERR(lcdi2c_device))
    {
        dev_warn(&client->dev, " device %s creation failed\n", DEVICE_NAME);
        goto failed_device;
    }
    
    if(sysfs_create_group(&lcdi2c_device->kobj, &i2clcd_device_attr_group))
    {
        dev_warn(&client->dev, " device attribute group creation failed\n");
        goto failed_device;
    }
    
     dev_info(&client->dev, " registered with major %u\n", major);
    

    return ret;
    
    failed_device:
        class_unregister(lcdi2c_class);
        class_destroy(lcdi2c_class);
    failed_class:
        unregister_chrdev(major, DEVICE_NAME);
    failed_chrdev:
        i2c_unregister_device(client);
        i2c_del_driver(&lcdi2c_driver);
    return -1;
}


static void __exit i2clcd857_exit(void)
{
    
    unregister_chrdev(major, DEVICE_NAME);
    
    sysfs_remove_group(&lcdi2c_device->kobj, &i2clcd_device_attr_group);
    device_destroy(lcdi2c_class, MKDEV(major, 0));
    class_unregister(lcdi2c_class);
    class_destroy(lcdi2c_class);
    unregister_chrdev(major, DEVICE_NAME);

     if (client)
         i2c_unregister_device(client);
    
    i2c_del_driver(&lcdi2c_driver);
}

module_init(i2clcd857_init);
module_exit(i2clcd857_exit);

//module_i2c_driver(lcdi2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jarek Zok <jarekzok@gmail.com>");
MODULE_DESCRIPTION("Driver for HD44780 LCD with PCF8574 I2C extension.");
MODULE_VERSION("0.1.0");

