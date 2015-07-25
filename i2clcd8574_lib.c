#include "i2clcd8574.h"

static const uint8_t topoaddr[][6] ={{0x00, 0x40, 0x00, 0x00, 40, 2}, //LCD_TOPO_40x2
                                     {0x00, 0x40, 0x14, 0x54, 20, 4}, //LCD_TOPO_20x4
                                     {0x00, 0x40, 0x00, 0x00, 20, 2}, //LCD_TOPO_20x2
                                     {0x00, 0x40, 0x10, 0x50, 16, 4}, //LCD_TOPO_16x4
                                     {0x00, 0x40, 0x00, 0x00, 16, 2}, //LCD_TOPO_16x2
                                     {0x00, 0x40, 0x00, 0x00, 16, 1}, //LCD_TOPO_16x1T1
                                     {0x00, 0x00, 0x00, 0x00, 16, 1}, //LCD_TOPO_16x1T2
                                    };
                                    
static const char *toponames[] = {
                                    "40x2",
                                    "20x4",
                                    "20x2",
                                    "16x4",
                                    "16x2",
                                    "16x1 type 1",
                                    "16x1 type 2",
                                    };

uint pinout[8] = {0,1,2,3,4,5,6,7}; //I2C module pinout configuration in order: 
                                    //RS,RW,E,BL,D4,D5,D6,D7

void _udelay_(uint32_t usecs)
{
#ifdef MODULE
    udelay(usecs);
#else
    usleep(usecs);
#endif
}

static void _buswrite(LcdData_t *lcd, uint8_t data)
{
    data |= lcd->backlight ? (1 << PIN_BACKLIGHTON) : 0;
    LOWLEVEL_WRITE(lcd->handle, data);
}

static void _strobe(LcdData_t *lcd, uint8_t data)
{
    _buswrite(lcd, data | (1 << PIN_EN));
    USLEEP(1);
    _buswrite(lcd, data & (~(1 << PIN_EN)));
    USLEEP(50);
}

static void _write4bits(LcdData_t *lcd, uint8_t value)
{
    _buswrite(lcd, value);
    _strobe(lcd, value);
}

static void lcdsend(LcdData_t *lcd, uint8_t value, uint8_t mode)
{
    uint8_t highnib = value & 0xF0;
    uint8_t lownib = value << 4;
    
    _write4bits(lcd, (highnib) | mode);
    _write4bits(lcd, (lownib) | mode);
}

void lcdflushbuffer(LcdData_t *lcd)
{
    uint8_t col = lcd->column, row = lcd->row, i;

    for(i = 0; i < (lcd->organization.columns * lcd->organization.rows); i++)
    {
        lcdcommand(lcd, LCD_DDRAM_SET + ITOMEMADDR(lcd, i));
        lcdsend(lcd, lcd->buffer[i], (1 << PIN_RS));
    }
    lcdsetcursor(lcd, col, row);
}

void lcdcommand(LcdData_t *lcd, uint8_t data)
{
    lcdsend(lcd, data, 0);
}

void lcdwrite(LcdData_t *lcd, uint8_t data)
{
    uint8_t memaddr;
    
    memaddr = (lcd->column + (lcd->row * lcd->organization.columns)) % LCD_BUFFER_SIZE;
    lcd->buffer[memaddr] = data;
    
    lcdsend(lcd, data, (1 << PIN_RS));
}

void lcdsetcursor(LcdData_t *lcd, uint8_t column, uint8_t row)
{
     lcd->column = (column >= lcd->organization.columns ? 0 : column);
     lcd->row = (row >= lcd->organization.rows ? 0 : row);
    lcdcommand(lcd, LCD_DDRAM_SET | PTOMEMADDR(lcd, lcd->column, lcd->row));
}

void lcdsetbacklight(LcdData_t *lcd, uint8_t backlight)
{
    lcd->backlight = backlight ? 1 : 0;
    _buswrite(lcd, lcd->backlight ? (1 << PIN_BACKLIGHTON) : 0);
}

void lcdcursor(LcdData_t *lcd, uint8_t cursor)
{
    if (cursor)
        lcd->displaycontrol |= LCD_CURSOR;
    else
        lcd->displaycontrol &= ~LCD_CURSOR;
    
    lcdcommand(lcd, lcd->displaycontrol);
}

void lcdblink(LcdData_t *lcd, uint8_t blink)
{
    if (blink)
        lcd->displaycontrol |= LCD_BLINK;
    else
        lcd->displaycontrol &= ~LCD_BLINK;
    
    lcdcommand(lcd, lcd->displaycontrol);
}

void lcdhome(LcdData_t *lcd)
{
    lcd->column = 0;
    lcd->row = 0;
    lcdcommand(lcd, LCD_HOME);
    MSLEEP(2);
}

void lcdclear(LcdData_t *lcd)
{
    memset(lcd->buffer, 0x20, LCD_BUFFER_SIZE); //Fill buffer with spaces
    lcdcommand(lcd, LCD_CLEAR);
    MSLEEP(2);
}

void lcdscrollhoriz(LcdData_t *lcd, uint8_t direction)
{
    lcdcommand(lcd, LCD_DS_SHIFTDISPLAY | 
	      (direction ? LCD_DS_SHIFTRIGHT : LCD_DS_SHIFTLEFT));
}

void lcdscrollvert(LcdData_t *lcd, uint8_t direction)
{
    //TODO: Vertical scroll
}

void lcdprint(LcdData_t *lcd, const char *data)
{
    int i = 0, row, col;
    row = lcd->row;
    col = lcd->column;
    
    while(i < LCD_BUFFER_SIZE && data[i] != 0)
    {
        if (data[i] == '\n')
        {
            col = 0;
            row++;
            i++;
            continue;
        }

        if (col == 0)
        {
            lcdsetcursor(lcd, col, row);
        }
        
        col = (col + 1) % lcd->organization.columns;
        if (col == 0)
            row = (row + 1) % lcd->organization.rows;

        lcd->row = row;
        lcd->column = col;
        
        lcdwrite(lcd, data[i]);
        i++;
    }
    lcdsetcursor(lcd, col, row);
}

void lcdcustomchar(LcdData_t *lcd, uint8_t num, const uint8_t *bitmap)
{
    uint8_t i;
    
    num &= 0x07;
    lcdcommand(lcd, LCD_CGRAM_SET | (num << 3));
    
    for(i = 0; i < 8; i++)
    {
        lcd->customchars[num][i] = bitmap[i];
	lcdsend(lcd, bitmap[i], (1 << PIN_RS));
    }
}

void lcdfinalize(LcdData_t *lcd)
{
    lcdsetbacklight(lcd, 0);
    lcdclear(lcd);
    lcdclear(lcd);
    lcdcommand(lcd, LCD_DC_DISPLAYOFF | LCD_DC_CURSOROFF | LCD_DC_CURSORBLINKOFF);
}

void lcdinit(LcdData_t *lcd, lcd_topology topo)
{
    memset(lcd->buffer, 0x20, LCD_BUFFER_SIZE); //Fill buffer with spaces
    
    if (topo > LCD_TOPO_16x1T2)
        topo = LCD_TOPO_16x2;
        
    lcd->organization.topology = topo;
    lcd->organization.columns = topoaddr[topo][4];
    lcd->organization.rows = topoaddr[topo][5];
    memcpy(lcd->organization.addresses, topoaddr[topo], sizeof(topoaddr[topo]) - 2);
    lcd->organization.toponame = toponames[topo];
    
    lcd->displaycontrol = 0;
    lcd->displaymode = 0;
    
    lcd->displayfunction = LCD_FS_4BITDATA | LCD_FS_1LINE | LCD_FS_5x8FONT;
    if (lcd->organization.rows > 1)
        lcd->displayfunction |= LCD_FS_2LINES;
    
    MSLEEP(50);
    _buswrite(lcd, lcd->backlight ? (1 << PIN_BACKLIGHTON) : 0);
    MSLEEP(100);
    
    _write4bits(lcd, (1 << PIN_DB4) | (1 << PIN_DB5));
    MSLEEP(5);
    
    _write4bits(lcd, (1 << PIN_DB4) | (1 << PIN_DB5));
    MSLEEP(5);
    
    _write4bits(lcd, (1 << PIN_DB4) | (1 << PIN_DB5));
    USLEEP(150);
    
    _write4bits(lcd, (1 << PIN_DB5));
    
    lcdcommand(lcd, lcd->displayfunction);
    
    lcd->displaycontrol |= LCD_DC_DISPLAYON | LCD_DC_CURSOROFF | 
			   LCD_DC_CURSORBLINKOFF;
    lcdcommand(lcd, lcd->displaycontrol);
      
    lcdclear(lcd);
    lcdhome(lcd);
    
    lcdsetcursor(lcd, lcd->column, lcd->row);
    lcdcursor(lcd, lcd->cursor);
    lcdblink(lcd, lcd->blink);
}


