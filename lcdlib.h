//
// Created by jarekzok@gmail.com on 27.12.23.
//

#ifndef LCDI2C_LCDLIB_H
#define LCDI2C_LCDLIB_H

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/semaphore.h>

#define LCDI2C8574_DESCRIPTION "LCD driver for PCF8574 I2C expander"
#define LCDI2C8574_VERSION "0.2.1"

extern uint pinout[8];

#define PINTR(pin) (pinout[pin])

#define PIN_BACKLIGHTON     PINTR(3)
#define PIN_EN              PINTR(2)
#define PIN_RW              PINTR(1)
#define PIN_RS              PINTR(0)

#define PIN_DB4             PINTR(4)
#define PIN_DB5             PINTR(5)
#define PIN_DB6             PINTR(6)
#define PIN_DB7             PINTR(7)

#define LCD_CMD_CLEARDISPLAY    (0)
#define LCD_CMD_HOME            (1)
#define LCD_CMD_ENTRYMODE       (2)
#define LCD_CMD_DISPLAYCONTROL  (3)
#define LCD_CMD_DISPLAYSHIFT    (4)
#define LCD_CMD_FUNCTIONSET     (5)
#define LCD_CMD_SETCGRAMADDR    (6)
#define LCD_CMD_SETDDRAMADDR    (7)

#define DEFAULT_CHIP_ADDRESS (0x27)
#define LCD_BUFFER_SIZE (0x68) //20 columns * 4 rows + 4 extra chars
#define LCD_DEFAULT_COLS (16)
#define LCD_DEFAULT_ROWS (2)
#define LCD_DEFAULT_ORGANIZATION LCD_TOPO_16x2

//for convenience
#define LCD_MODE_COMMAND        (0)
#define LCD_MODE_DATA           (1 << PIN_RS)
#define LCD_CLEAR               (1 << LCD_CMD_CLEARDISPLAY)
#define LCD_HOME                (1 << LCD_CMD_HOME)
#define LCD_CGRAM_SET           ((1 << LCD_CMD_SETCGRAMADDR))
#define LCD_DDRAM_SET           ((1 << LCD_CMD_SETDDRAMADDR))

//For LCD_ENTRYMODE
#define LCD_EM_SHIFTINC         ((1 << LCD_CMD_ENTRYMODE) | (1 << 1))
#define LCD_EM_SHIFTDEC         ((1 << LCD_CMD_ENTRYMODE))
#define LCD_EM_ENTRYLEFT        ((1 << LCD_CMD_ENTRYMODE) | (1 << 0))
#define LCD_EM_ENTRYRIGHT       ((1 << LCD_CMD_ENTRYMODE))

//For LCD_DISPLAYCONTROL
#define LCD_BLINK (1 << 0)
#define LCD_CURSOR (1 << 1)
#define LCD_DISPLAY (1 << 2)
#define LCD_DC_CURSORBLINKON    ((1 << LCD_CMD_DISPLAYCONTROL) | LCD_BLINK)
#define LCD_DC_CURSORBLINKOFF   ((1 << LCD_CMD_DISPLAYCONTROL))
#define LCD_DC_CURSORON         ((1 << LCD_CMD_DISPLAYCONTROL) | LCD_CURSOR)
#define LCD_DC_CURSOROFF        ((1 << LCD_CMD_DISPLAYCONTROL))
#define LCD_DC_DISPLAYON        ((1 << LCD_CMD_DISPLAYCONTROL) | LCD_DISPLAY)
#define LCD_DC_DISPLAYOFF       ((1 << LCD_CMD_DISPLAYCONTROL))

//For LCD_CMD_DISPLAYSHIFT
#define LCD_DS_SHIFTDISPLAY     ((1 << LCD_CMD_DISPLAYSHIFT) | (1 << 3))
#define LCD_DS_MOVECURSOR       ((1 << LCD_CMD_DISPLAYSHIFT))
#define LCD_DS_SHIFTRIGHT       ((1 << LCD_CMD_DISPLAYSHIFT) | (1 << 2))
#define LCD_DS_SHIFTLEFT        ((1 << LCD_CMD_DISPLAYSHIFT))

//For LCD_CMD_FUNCTIONSET
#define LCD_FS_8BITDATA         ((1 << LCD_CMD_FUNCTIONSET) | (1 << 4))
#define LCD_FS_4BITDATA         ((1 << LCD_CMD_FUNCTIONSET))
#define LCD_FS_1LINE            ((1 << LCD_CMD_FUNCTIONSET))
#define LCD_FS_2LINES           ((1 << LCD_CMD_FUNCTIONSET) | (1 << 3))
#define LCD_FS_5x10FONT         ((1 << LCD_CMD_FUNCTIONSET) | (1 << 2))
#define LCD_FS_5x8FONT          ((1 << LCD_CMD_FUNCTIONSET))

#define LCD_REG_DATA        (1)
#define LCD_REG_COMMAND     (0)

#define LCD_READ            (1)
#define LCD_WRITE           (0)


#define USLEEP(usecs) _udelay_(usecs)
#define MSLEEP(msecs) mdelay(msecs)

#define LOWLEVEL_WRITE(client, data) i2c_smbus_write_byte(client, data)
//Byte index to position as row and column
#define ITOP(data, i, col, row) *(&col) = (u8) ((i) % data->organization.columns); *(&row) = (u8) ((i) / data->organization.columns)
//Byte index to memory address
#define ITOMEMADDR(data, i)   (((i) % data->organization.columns) + data->organization.addresses[((i) / data->organization.columns)])
//Position as row and column to memory address
#define PTOMEMADDR(data, col, row) ((col % data->organization.columns) + data->organization.addresses[(row % data->organization.rows)])

typedef enum lcd_topology {
    LCD_TOPO_40x2 = 0,
    LCD_TOPO_20x4 = 1,
    LCD_TOPO_20x2 = 2,
    LCD_TOPO_16x4 = 3,
    LCD_TOPO_16x2 = 4,
    LCD_TOPO_16x1T1 = 5,
    LCD_TOPO_16x1T2 = 6,
    LCD_TOPO_8x2 = 7,
} lcd_topology;

/*
  LCD topology description, first four bytes defines subsequent row of text addresses and how
  are they mapped in internal RAM of LCD, last two bytes describes number of columns and number
  of rows.
  LCD_TOPO_16x1T1 is same as 8x2. RAM is divided in two areas by 8 bytes, left bytes
  are starting from address 0x00, right hand half of LCD is starting from 0x40, that's
  exactly how 8x2 LCD is organized. So this type of LCD can be considered as 8x2 LCD with two
  rows laying in the same line on both halfs of an LCD, instead both lines stacked on the top
  of each other.
  Type 2 of 16x1 has straighforward organization, first sixteen bytes representing a row on an LCD.
*/
static const u8 topoaddr[][6] = {{0x00, 0x40, 0x00, 0x00, 40, 2}, //LCD_TOPO_40x2
                                 {0x00, 0x40, 0x14, 0x54, 20, 4}, //LCD_TOPO_20x4
                                 {0x00, 0x40, 0x00, 0x00, 20, 2}, //LCD_TOPO_20x2
                                 {0x00, 0x40, 0x10, 0x50, 16, 4}, //LCD_TOPO_16x4
                                 {0x00, 0x40, 0x00, 0x00, 16, 2}, //LCD_TOPO_16x2
                                 {0x00, 0x40, 0x00, 0x40, 8,  2}, //LCD_TOPO_16x1T1
                                 {0x00, 0x08, 0x00, 0x08, 16, 1}, //LCD_TOPO_16x1T2
                                 {0x00, 0x40, 0x00, 0x40, 8,  2}, //LCD_TOPO_8x2
};

/* Text representation of various LCD topologies */
static const char *toponames[] = {
        "40x2",
        "20x4",
        "20x2",
        "16x4",
        "16x2",
        "16x1 type 1",
        "16x1 type 2",
        "8x2",
};

typedef struct lcd_organization
{
    u8 columns;
    u8 rows;
    u8 addresses[4];
    lcd_topology topology;
    const char *toponame;
} LcdOrganization_t;

typedef struct lcddata
{
    struct i2c_client *handle;
    struct semaphore sem;
    int major;

    LcdOrganization_t organization;
    u8 backlight;
    u8 cursor;
    u8 blink;
    u8 column;
    u8 row;
    u8 displaycontrol;
    u8 displayfunction;
    u8 displaymode;
    u8 swscreen;
    u8 buffer[LCD_BUFFER_SIZE];
    u8 customchars[8][8];
    u16 deviceopencnt;
    u8 devicefileptr;
    char welcome[16];
} LcdHandler_t;

void _udelay_(u32 usecs);
void lcdflushbuffer(LcdHandler_t *lcd);
void lcdcommand(LcdHandler_t *lcd, u8 data);
void lcdwrite(LcdHandler_t *lcd, u8 data);
void lcdsetcursor(LcdHandler_t *lcd, u8 column, u8 row);
void lcdsetbacklight(LcdHandler_t *lcd, u8 backlight);
void lcdcursor(LcdHandler_t *lcd, u8 cursor);
void lcdblink(LcdHandler_t *lcd, u8 blink);
u8 lcdprint(LcdHandler_t *lcd, const char *data);
void lcdfinalize(LcdHandler_t *lcd);
void lcdinit(LcdHandler_t *lcd, lcd_topology topo);
void lcdhome(LcdHandler_t *lcd);
void lcdclear(LcdHandler_t *lcd);
void lcdscrollvert(LcdHandler_t *lcd, u8 direction);
void lcdscrollhoriz(LcdHandler_t *lcd, u8 direction);
void lcdcustomchar(LcdHandler_t *lcd, u8 num, const u8 *bitmap);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jarek Zok <jarekzok@gmail.com>");
MODULE_DESCRIPTION(LCDI2C8574_DESCRIPTION);
MODULE_VERSION(LCDI2C8574_VERSION);

#endif //LCDI2C_LCDLIB_H
