#include "i2clcd8574.h"

#define CRIT_BEG(d, error) if(down_interruptible(&d->sem)) return -error
#define CRIT_END(d) up(&d->sem)

static uint busno = 1;      //I2C Bus number
static uint address = DEFAULT_CHIP_ADDRESS; //Device address
static uint topo = LCD_DEFAULT_ORGANIZATION;
static uint cursor = 1;
static uint blink = 1;
static IOCTLDescription_t ioctls[] = {
  { .ioctlcode = LCD_IOCTL_GETCHAR, .name = "GETCHAR", },
  { .ioctlcode = LCD_IOCTL_SETCHAR, .name = "SETCHAR", },
  { .ioctlcode = LCD_IOCTL_GETPOSITION, .name = "GETPOSITION" },
  { .ioctlcode = LCD_IOCTL_SETPOSITION, .name = "SETPOSITION" },
  { .ioctlcode = LCD_IOCTL_RESET, .name = "RESET" },
  { .ioctlcode = LCD_IOCTL_HOME, .name = "HOME" },
  { .ioctlcode = LCD_IOCTL_GETBACKLIGHT, .name = "GETBACKLIGHT" },
  { .ioctlcode = LCD_IOCTL_SETBACKLIGHT, .name = "SETBACKLIGHT" },
  { .ioctlcode = LCD_IOCTL_GETCURSOR, .name = "GETCURSOR" },
  { .ioctlcode = LCD_IOCTL_SETCURSOR, .name = "SETCURSOR" },
  { .ioctlcode = LCD_IOCTL_GETBLINK,  .name = "GETBLINK" },
  { .ioctlcode = LCD_IOCTL_SETBLINK,  .name = "SETBLINK" },
  { .ioctlcode = LCD_IOCTL_SCROLLHZ,  .name = "SCROLLHZ" },
  { .ioctlcode = LCD_IOCTL_GETCUSTOMCHAR, .name = "GETCUSTOMCHAR" },
  { .ioctlcode = LCD_IOCTL_SETCUSTOMCHAR, .name = "SETCUSTOMCHAR" },
  { .ioctlcode = LCD_IOCTL_CLEAR, .name = "CLEAR" },
};


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
			"\t\t7 - 8x2\n"
                        "\t\tDefault set to 16x2");

static int lcdi2c_open(struct inode *inode, struct file *file)
{
    CRIT_BEG(data, EBUSY);
    
    data->deviceopencnt++;
    data->devicefileptr = 0;
    try_module_get(THIS_MODULE);
    CRIT_END(data);
    
    return SUCCESS;
}

static int lcdi2c_release(struct inode *inode, struct file *file)
{
    CRIT_BEG(data, EBUSY);
    
    data->deviceopencnt--;
    
    module_put(THIS_MODULE);
    CRIT_END(data);
    return SUCCESS;
}

static ssize_t lcdi2c_fopread(struct file *file, char __user *buffer, 
			   size_t length, loff_t *offset)
{
    uint8_t i = 0;

    CRIT_BEG(data, EBUSY);
    if (data->devicefileptr == (data->organization.columns * data->organization.rows))
      return 0;
       
    while(i < length && i < (data->organization.columns * data->organization.rows))
    {    
      put_user(data->buffer[i], buffer++);
      data->devicefileptr++;
      i++;
    }
    
    i %= (data->organization.columns * data->organization.rows);
    ITOP(data, i, data->column, data->row);
    lcdsetcursor(data, data->column, data->row);
    (*offset) = i;
    CRIT_END(data);
    
    return i;
}

static ssize_t lcdi2c_fopwrite(struct file *file, const char __user *buffer, 
			    size_t length, loff_t *offset)
{
    uint8_t i, str[81];
  
    CRIT_BEG(data, EBUSY);

    for(i = 0; i < length && i < (data->organization.columns * data->organization.rows); i++)
      get_user(str[i], buffer + i);
    str[i] = 0;
    (*offset) = lcdprint(data, str);    

    CRIT_END(data);
    return i;
}

loff_t lcdi2c_lseek(struct file *file, loff_t offset, int orig)
{
  uint8_t memaddr, oldoffset;
  
  CRIT_BEG(data, EBUSY);
  memaddr = data->column + (data->row * data->organization.columns);
  oldoffset = memaddr;
  memaddr = (memaddr + (uint8_t)offset) % (data->organization.rows * data->organization.columns);
  data->column =  (memaddr % data->organization.columns);
  data->row = (memaddr / data->organization.columns);
  lcdsetcursor(data, data->column, data->row);
  CRIT_END(data);
  
  return oldoffset;
}

static long lcdi2c_ioctl(struct file *file, 
			unsigned int ioctl_num,
			unsigned long arg)
{
  
  char *buffer = (char*)arg, ccb[10];
  uint8_t memaddr, i, ch;
  long status = SUCCESS;
  
  CRIT_BEG(data, EAGAIN);
  
  switch (ioctl_num)
  {
    case LCD_IOCTL_SETCHAR:
      get_user(ch, buffer);
      memaddr = (1 + data->column + (data->row * data->organization.columns)) % LCD_BUFFER_SIZE;
      lcdwrite(data, ch);
      data->column = (memaddr % data->organization.columns);
      data->row = (memaddr / data->organization.columns);
      lcdsetcursor(data, data->column, data->row);
      break;
    case LCD_IOCTL_GETCHAR:
      memaddr = (data->column + (data->row * data->organization.columns)) % LCD_BUFFER_SIZE;
      ch = data->buffer[memaddr];
      put_user(ch, buffer);
      break;
    case LCD_IOCTL_GETPOSITION:
      printk(KERN_INFO "GETPOSITION called\n");
      put_user(data->column, buffer);
      put_user(data->row, buffer+1);
      break;
    case LCD_IOCTL_SETPOSITION:
      get_user(data->column, buffer);
      get_user(data->row, buffer+1);
      lcdsetcursor(data, data->column, data->row);
      break;
    case LCD_IOCTL_RESET:
      get_user(ch, buffer);
      if (ch == '1')
	lcdinit(data, data->organization.topology);
      break;
    case LCD_IOCTL_HOME:
      printk(KERN_INFO "HOME called\n");
      get_user(ch, buffer);
      if (ch == '1')
	lcdhome(data);
      break;
    case LCD_IOCTL_GETCURSOR:
      put_user(data->cursor ? '1' : '0', buffer);
      break;
    case LCD_IOCTL_SETCURSOR:
      get_user(ch, buffer);
      lcdcursor(data, (ch == '1'));
      break;
    case LCD_IOCTL_GETBLINK:
      put_user(data->blink ? '1' : '0', buffer);
      break;
    case LCD_IOCTL_SETBLINK:
      get_user(ch, buffer);
      lcdblink(data, (ch == '1'));
      break;
    case LCD_IOCTL_GETBACKLIGHT:
      put_user(data->backlight ? '1' : '0', buffer);
      break;
    case LCD_IOCTL_SETBACKLIGHT:
      get_user(ch, buffer);
      lcdsetbacklight(data, (ch == '1'));
      break;
    case LCD_IOCTL_SCROLLHZ:
      get_user(ch, buffer);
      lcdscrollhoriz(data, ch - '0');
      break;
    case LCD_IOCTL_GETCUSTOMCHAR:
      get_user(ch, buffer);
      for (i=0; i<8; i++)
	put_user(data->customchars[ch][i], buffer + i + 1);
      break;
    case LCD_IOCTL_SETCUSTOMCHAR:
      for (i = 0; i < 9; i++)
	get_user(ccb[i], buffer + i);
      lcdcustomchar(data, ccb[0], ccb+1);
      break;
    case LCD_IOCTL_CLEAR:
      get_user(ch, buffer);
      if (ch == '1')
	lcdclear(data);
      break;
    default:
      printk(KERN_INFO "Unknown IOCTL\n");
      break;
  }
  CRIT_END(data);
  
  return status;
}

static int lcdi2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
    data->deviceopencnt = 0;
    data->major = major;

    lcdinit(data, topo);
    lcdprint(data, "HD44780\nDriver");

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

static ssize_t lcdi2c_reset(struct device* dev, struct device_attribute* attr, 
			    const char* buf, size_t count)
{
    CRIT_BEG(data, ERESTARTSYS);
    
    if (count > 0 && buf[0] == '1')
        lcdinit(data, topo);
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_backlight(struct device* dev, 
				struct device_attribute* attr, 
				const char* buf, size_t count)
{
    CRIT_BEG(data, ERESTARTSYS);
    
    if (count > 0)
        lcdsetbacklight(data, (buf[0] == '1'));
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_backlight_show(struct device *dev, 
				     struct device_attribute *attr, char *buf)
{
    int count = 0;
    
    CRIT_BEG(data, ERESTARTSYS);
    
    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%c", data->backlight ? '1' : '0');
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_cursorpos(struct device* dev, 
				struct device_attribute* attr, 
				const char* buf, size_t count)
{
    CRIT_BEG(data, ERESTARTSYS);
    
    if (count >= 2)
    {
        count = 2;
        lcdsetcursor(data, buf[0], buf[1]);
    }
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_cursorpos_show(struct device *dev, 
				     struct device_attribute *attr, 
				     char *buf)
{
    ssize_t count = 0;
    
    CRIT_BEG(data, ERESTARTSYS);
    
    if (buf)
    {
        count = 2;
        buf[0] = data->column;
        buf[1] = data->row;
    }
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_data(struct device* dev, 
			   struct device_attribute* attr, 
			   const char* buf, size_t count)
{
    uint8_t i, addr, memaddr;
    
    CRIT_BEG(data, ERESTARTSYS);
    
    if (count > 0)
    {
	memaddr = data->column + (data->row * data->organization.columns); 
	for(i = 0; i < count; i++)
	{
	  addr = (memaddr + i) % (data->organization.columns * data->organization.rows);
	  data->buffer[addr] = buf[i];
	}
	lcdflushbuffer(data);        
    }
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_data_show(struct device *dev, 
				struct device_attribute *attr, char *buf)
{
    uint8_t i=0;
    
    CRIT_BEG(data, ERESTARTSYS);

    if (buf)
    {
        for (i = 0; i < (data->organization.columns * data->organization.rows); i++)
        {	    
            buf[i] = data->buffer[i];
        } 
    }
    
    CRIT_END(data);
    return (data->organization.columns * data->organization.rows);
}

static ssize_t lcdi2c_meta_show(struct device *dev, 
				struct device_attribute *attr, char *buf)
{
    ssize_t count = 0;
    char tmp[12], lines[54];
    
    CRIT_BEG(data, ERESTARTSYS);
    
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
                         "Rows addresses:%s\n"
                         "Pins:RS=%d RW=%d E=%d BCKLIGHT=%d D[4]=%d D[5]=%d D[6]=%d D[7]=%d\n"
			 "IOCTLS:\n",
                         data->organization.toponame, 
                         data->organization.topology, 
                         data->organization.rows,
                         data->organization.columns,
                         lines,
                         PIN_RS, PIN_RW, PIN_EN,
                         PIN_BACKLIGHTON,
                         PIN_DB4, PIN_DB5, PIN_DB6, PIN_DB7
                        );
	
 	for(i=0; i < (sizeof(ioctls) / sizeof(IOCTLDescription_t)); i++)
 	{
 	  count += snprintf(lines, 54, "\t%s=0x%02X\n", ioctls[i].name, ioctls[i].ioctlcode);
 	  strncat(buf, lines, PAGE_SIZE);
 	}
 	
	
    }
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_cursor(struct device* dev, 
			     struct device_attribute* attr, 
			     const char* buf, size_t count)
{
    CRIT_BEG(data, ERESTARTSYS);
    
    if (count > 0)
    {
        data->cursor = (buf[0] == '1');
        lcdcursor(data, data->cursor);
    }
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_cursor_show(struct device *dev, 
				  struct device_attribute *attr, char *buf)
{
    int count = 0;
    
    CRIT_BEG(data, ERESTARTSYS);
    
    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%c", data->cursor ? '1' : '0');
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_blink(struct device* dev, 
			    struct device_attribute* attr, 
			    const char* buf, size_t count)
{
    CRIT_BEG(data, ERESTARTSYS);
    
    if (count > 0)
    {
        data->blink = (buf[0] == '1');
        lcdblink(data, data->blink);
    }
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_blink_show(struct device *dev, 
				 struct device_attribute *attr, char *buf)
{
    int count = 0;
    
    CRIT_BEG(data, ERESTARTSYS);
    
    if (buf)
        count = snprintf(buf, PAGE_SIZE, "%c", data->blink ? '1' : '0');
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_home(struct device* dev, struct device_attribute* attr, 
			   const char* buf, size_t count)
{
    CRIT_BEG(data, ERESTARTSYS);
    
    if (count > 0 && buf[0] == '1')
        lcdhome(data);
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_clear(struct device* dev, struct device_attribute* attr, 
			    const char* buf, size_t count)
{
    CRIT_BEG(data, ERESTARTSYS);
    
    if (count > 0 && buf[0] == '1')
        lcdclear(data);
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_scrollhz(struct device* dev, struct device_attribute* attr, 
			       const char* buf, size_t count)
{
    CRIT_BEG(data, ERESTARTSYS);
    
    if (count > 0)
        lcdscrollhoriz(data, buf[0] - '0');
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_customchar(struct device* dev, 
				 struct device_attribute* attr, 
				 const char* buf, size_t count)
{
    uint8_t i;
      
    CRIT_BEG(data, ERESTARTSYS);
    
    if ((count > 0 && (count % 9)) || count == 0)
    {
         dev_err(&client->dev, "incomplete character bitmap definition\n");
	 CRIT_END(data);
         return -ETOOSMALL;
    }
    
    for (i = 0; i < count; i+=9)
    {
        if (buf[i] > 7)
        {
            dev_err(&client->dev, "character number %d can only have values"
				  "starting from 0 to 7\n", buf[i]);
	    CRIT_END(data);
	    return -ETOOSMALL;
        }
        
        lcdcustomchar(data, buf[i], buf + i + 1);
    }
    
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_customchar_show(struct device *dev, 
				      struct device_attribute *attr, char *buf)
{
    int count = 0, c, i;
    
    CRIT_BEG(data, ERESTARTSYS);
    
    for (c = 0; c < 8; c++)
    {
        buf[c * 9] = c;
	count++;
        for (i = 0; i < 8; i++)
        {
            buf[c * 9 + (i + 1)] = data->customchars[c][i];
            count++;
        }
    }
       
    CRIT_END(data);
    return count;
}

static ssize_t lcdi2c_char(struct device* dev, 
				 struct device_attribute* attr, 
				 const char* buf, size_t count)
{
    uint8_t memaddr;
    
    CRIT_BEG(data, ERESTARTSYS);
    
    if (buf && count > 0)
    {
      memaddr = (1 + data->column + (data->row * data->organization.columns)) % LCD_BUFFER_SIZE;
      lcdwrite(data, buf[0]);
      data->column = (memaddr % data->organization.columns);
      data->row = (memaddr / data->organization.columns);
      lcdsetcursor(data, data->column, data->row);
    }
    
    CRIT_END(data);
    return 1;
}

static ssize_t lcdi2c_char_show(struct device *dev, 
				struct device_attribute *attr, char *buf)
{
    uint8_t memaddr;
    
    CRIT_BEG(data, ERESTARTSYS);
    
    memaddr = (data->column + (data->row * data->organization.columns)) % LCD_BUFFER_SIZE;
    buf[0] = data->buffer[memaddr];
    
    CRIT_END(data);
    return 1;
}

DEVICE_ATTR(reset, S_IWUSR | S_IWGRP, NULL, lcdi2c_reset);
DEVICE_ATTR(backlight, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH,
	    lcdi2c_backlight_show, lcdi2c_backlight);
DEVICE_ATTR(position, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, 
	    lcdi2c_cursorpos_show, lcdi2c_cursorpos);
DEVICE_ATTR(data, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, 
	    lcdi2c_data_show, lcdi2c_data);
DEVICE_ATTR(meta, S_IRUSR | S_IRGRP, lcdi2c_meta_show, NULL);
DEVICE_ATTR(cursor, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, 
	    lcdi2c_cursor_show, lcdi2c_cursor);
DEVICE_ATTR(blink, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, 
	    lcdi2c_blink_show, lcdi2c_blink);
DEVICE_ATTR(home, S_IWUSR | S_IWGRP, NULL, lcdi2c_home);
DEVICE_ATTR(clear, S_IWUSR | S_IWGRP, NULL, lcdi2c_clear);
DEVICE_ATTR(scrollhz, S_IWUSR | S_IWGRP, NULL, lcdi2c_scrollhz);
DEVICE_ATTR(customchar, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, 
	    lcdi2c_customchar_show, lcdi2c_customchar);
DEVICE_ATTR(character, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP | S_IROTH, 
	    lcdi2c_char_show, lcdi2c_char );

static const struct attribute *i2clcd_attrs[] = {
	&dev_attr_reset.attr,
	&dev_attr_backlight.attr,
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
        dev_warn(&client->dev, "failed to register device with error: %d\n", 
		 major);
        goto failed_chrdev;
    }
    
    lcdi2c_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);
    if (IS_ERR(lcdi2c_class))
    {
        dev_warn(&client->dev, "class creation failed %s\n", DEVICE_CLASS_NAME);
        goto failed_class;
    }

    lcdi2c_device = device_create(lcdi2c_class, NULL, MKDEV(major, minor), NULL, 
				  DEVICE_NAME);
    if (IS_ERR(lcdi2c_device))
    {
        dev_warn(&client->dev, "device %s creation failed\n", DEVICE_NAME);
        goto failed_device;
    }
    
    if(sysfs_create_group(&lcdi2c_device->kobj, &i2clcd_device_attr_group))
    {
        dev_warn(&client->dev, "device attribute group creation failed\n");
        goto failed_device;
    }
    
     dev_info(&client->dev, "registered with major %u\n", major);
    

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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jarek Zok <jarekzok@gmail.com>");
MODULE_DESCRIPTION("Driver for HD44780 LCD with PCF8574 I2C extension.");
MODULE_VERSION("0.1.0");
