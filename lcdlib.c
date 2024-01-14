//
// Created by tester on 27.12.23.
//

#include <linux/delay.h>

#include "lcdlib.h"

/* Pin mapping array */
uint pinout[8] = {0, 1, 2, 3, 4, 5, 6, 7}; //I2C module pinout configuration in order:
//RS,RW,E,BL,D4,D5,D6,D7


void _udelay_(u32 usecs) {
    udelay(usecs);
}

/**
 * write a byte to i2c device, sets backlight pin
 * on or off depending on current stup in LcdData struture
 * given as parameter.
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 byte to send to LCD
 * @return none
 *
 */
static void _buswrite(LcdDescriptor_t *lcd, u8 data) {
    data |= lcd->backlight ? (1 << PIN_BACKLIGHT) : 0;
    LOWLEVEL_WRITE(lcd->driver_data.client, data);
}

/**
 * write a byte to i2c device, strobing EN pin of LCD
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 byte to send to LCD
 * @return none
 *
 */
static void _strobe(LcdDescriptor_t *lcd, u8 data) {
    _buswrite(lcd, data | (1 << PIN_EN));
    USLEEP(1);
    _buswrite(lcd, data & (~(1 << PIN_EN)));
    USLEEP(50);
}

/**
 * write a byte using 4 bit interface
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 byte to send to LCD
 * @return none
 *
 */
static void _write4bits(LcdDescriptor_t *lcd, u8 value) {
    _buswrite(lcd, value);
    _strobe(lcd, value);
}

/**
 * write a byte to a LCD splitting byte into two nibbles
 * of 4 bits each.
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 byte to send to LCD
 * @param u8 mode of communication (RS line of a LCD)
 * @return none
 *
 */
static void lcdsend(LcdDescriptor_t *lcd, u8 value, u8 mode) {
    u8 highnib = value & 0xF0;
    u8 lownib = value << 4;

    _write4bits(lcd, (highnib) | mode);
    _write4bits(lcd, (lownib) | mode);
}

/**
 * copy raw_data of raw_data from host to LCD
 *
 * @param LcdData_t* lcd handler structure address
 * @return none
 *
 */
void lcdflushbuffer(LcdDescriptor_t *lcd) {
    u8 col = lcd->column, row = lcd->row;

    for (u8 i = 0; i < (lcd->organization.columns * lcd->organization.rows); i++) {
        lcdcommand(lcd, LCD_DDRAM_SET + ITOMEMADDR(lcd, i));
        lcdsend(lcd, lcd->raw_data[i], (1 << PIN_RS));
    }
    lcdsetcursor(lcd, col, row);
}

/**
 * send command to a LCD.
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 command byte to send
 * @return none
 *
 */
void lcdcommand(LcdDescriptor_t *lcd, u8 data) {
    lcdsend(lcd, data, 0);
}

/**
 * write byte of data to LCD
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 data byte to send
 * @return none
 *
 */
void lcdwrite(LcdDescriptor_t *lcd, u8 data) {
    u8 memaddr;

    memaddr = (lcd->column + (lcd->row * lcd->organization.columns)) % LCD_BUFFER_SIZE;
    lcd->raw_data[memaddr] = data;

    lcdsend(lcd, data, (1 << PIN_RS));
}

/**
 * set cursor to given position. Row number and column number
 * depends on current LCD topology, if values are to high
 * then are wrappped around and set to 0
 *
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 column number
 * @param u8 row number
 * @return none
 *
 */
void lcdsetcursor(LcdDescriptor_t *lcd, u8 column, u8 row) {
    lcd->column = (column >= lcd->organization.columns ? 0 : column);
    lcd->row = (row >= lcd->organization.rows ? 0 : row);
    lcdcommand(lcd, LCD_DDRAM_SET | PTOMEMADDR(lcd, lcd->column, lcd->row));
}

/**
 * switches backlight on or off.
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 false value switechs backlight off, otherwise backlight will
 *                be switched on
 * @return none
 *
 */
void lcdsetbacklight(LcdDescriptor_t *lcd, u8 backlight) {
    lcd->backlight = backlight;
    _buswrite(lcd, lcd->backlight ? (1 << PIN_BACKLIGHT) : 0);
}

/**
 * switches visibility of cursor on or off
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 false will switch it off, otherwise it will be switched on
 * @return none
 *
 */
void lcdcursor(LcdDescriptor_t *lcd, u8 cursor) {
    if (cursor)
        lcd->display_control |= LCD_CURSOR;
    else
        lcd->display_control &= ~LCD_CURSOR;

    lcdcommand(lcd, lcd->display_control);
}

/**
 * switch blink of a cursor on or of
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 false will switch it off, otherwise it will be switched on
 * @return none
 *
 */
void lcdblink(LcdDescriptor_t *lcd, u8 blink) {
    if (blink)
        lcd->display_control |= LCD_BLINK;
    else
        lcd->display_control &= ~LCD_BLINK;

    lcdcommand(lcd, lcd->display_control);
}

/**
 * will set LCD back to home, which usually is cursor set at position 0,0
 *
 * @param LcdData_t* lcd handler structure address
 * @return none
 *
 */
void lcdhome(LcdDescriptor_t *lcd) {
    lcd->column = 0;
    lcd->row = 0;
    lcdcommand(lcd, LCD_HOME);
    MSLEEP(2);
}

/**
 * clears raw_data by setting all bytes to 0x20 (ASCII code for space) and
 * send clear command to a LCD
 *
 * @param LcdData_t* lcd handler structure address
 * @return none
 *
 */
void lcdclear(LcdDescriptor_t *lcd) {
    memset(lcd->raw_data, 0x20, LCD_BUFFER_SIZE); //Fill raw_data with spaces
    lcdcommand(lcd, LCD_CLEAR);
    MSLEEP(2);
}

/**
 * scrolls content of a LCD horizontally. This is internal feature of HD44780
 * it scrolls content without actually changing content of internal RAM, so
 * raw_data of host stays intact as well
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 direction of a scroll, true - right, false - left
 * @return none
 *
 */
void lcdscrollhoriz(LcdDescriptor_t *lcd, u8 direction) {
    lcdcommand(lcd, LCD_DS_SHIFTDISPLAY |
                    (direction ? LCD_DS_SHIFTRIGHT : LCD_DS_SHIFTLEFT));
}

/**
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 direction of a scroll, true - down, false - up
 *
 */
void lcdscrollvert(LcdDescriptor_t *lcd, const char *line, uint len, u8 direction) {
    if (direction) {
        memcpy(lcd->raw_data + lcd->organization.columns,
               lcd->raw_data,
               lcd->organization.columns * (lcd->organization.rows - 1));
        memcpy(lcd->raw_data, line, lcd->organization.columns);
    } else {
        for (uint row = 1; row < lcd->organization.rows; row++) {
            memcpy(lcd->raw_data + (row - 1) * lcd->organization.columns,
                   lcd->raw_data + row * lcd->organization.columns,
                   lcd->organization.columns);
        }
        memcpy(lcd->raw_data + (lcd->organization.rows - 1) * lcd->organization.columns, line, len);
    }
    lcdflushbuffer(lcd);
}

/**
 * prints C string data on LCD, this function is doing some simple
 * interpretation of some special characters in string, like \n \r or
 * backspace. Every carriage return or return character will move cursor
 * to line below current one and backspace character will move cursor to the
 * left until it reaches first character of the line. If string is longer, than
 * what LCD is capable to display, cursor wraps around and print will overwrite
 * existing text.
 *
 * @param LcdData_t* lcd handler structure address
 * @param char* data 0 terinated string
 * @return u8 bytes written
 *
 */
u8 lcdprint(LcdDescriptor_t *lcd, const char *data) {
    int i = 0;
    const int max_len = (lcd->organization.columns * lcd->organization.rows);

    do {
        switch(data[i]) {
            case '\n':
            case '\r':
                if (lcd->organization.rows > 1) //For one-liners we do not reset column
                    //counter
                    lcd->column = 0;
                lcd->row = (lcd->row + 1) % lcd->organization.rows;
                i++;
                continue;
            case 0x08: //BS
                if (lcd->column > 0)
                    lcd->column -= 1;
                i++;
                continue;
            default:
                lcdcommand(lcd, LCD_DDRAM_SET | PTOMEMADDR(lcd, lcd->column, lcd->row));
                lcdwrite(lcd, data[i]);
                lcd->column = (lcd->column + 1) % lcd->organization.columns;
                if (lcd->column == 0)
                    lcd->row = (lcd->row + 1) % lcd->organization.rows;
                i++;
                break;
        }
    } while (data[i] && i < max_len);
    return (lcd->column + (lcd->row * lcd->organization.columns));
}

/**
 * allows to define custom character. It is feature of HD44780 controller.
 *
 * @param LcdData_t* lcd handler structure address
 * @param u8 character number to define 0-7
 * @param u8* array of 8 bytes of bitmap definition
 * @return none
 *
 */
void lcdcustomchar(LcdDescriptor_t *lcd, u8 num, const u8 *bitmap) {
    u8 i;

    num &= 0x07;
    lcdcommand(lcd, LCD_CGRAM_SET | (num << 3));

    for (i = 0; i < 8; i++) {
        lcd->custom_chars[num][i] = bitmap[i];
        lcdsend(lcd, bitmap[i], (1 << PIN_RS));
    }
}

/**
 * LCD de-initialization procedure
 *
 * @param LcdData_t* lcd handler structure address
 * @return none
 *
 */
void lcdfinalize(LcdDescriptor_t *lcd) {
    lcdsetbacklight(lcd, 0);
    lcdclear(lcd);
    lcdcommand(lcd, LCD_DC_DISPLAYOFF | LCD_DC_CURSOROFF | LCD_DC_CURSORBLINKOFF);
}

/**
 * initialization procedure of LCD
 *
 * @param LcdData_t* lcd handler structure address
 * @param lcd_topology number representing topology of LCD
 * @return none
 *
 */
void lcdinit(LcdDescriptor_t *lcd, lcd_topology_t topo) {
    memset(lcd->raw_data, 0x20, LCD_BUFFER_SIZE); //Fill raw_data with spaces

    if (topo > LCD_TOPO_8x2)
        topo = LCD_TOPO_16x2;

    lcd->organization.topology = topo;
    lcd->organization.columns = topoaddr[topo][4];
    lcd->organization.rows = topoaddr[topo][5];
    memcpy(lcd->organization.addresses, topoaddr[topo], sizeof(topoaddr[topo]) - 2);
    lcd->organization.toponame = toponames[topo];

    lcd->display_control = 0;

    lcd->display_function = LCD_FS_4BITDATA | LCD_FS_1LINE | LCD_FS_5x8FONT;
    if (lcd->organization.rows > 1)
        lcd->display_function |= LCD_FS_2LINES;

    MSLEEP(50);
    _buswrite(lcd, lcd->backlight ? (1 << PIN_BACKLIGHT) : 0);
    MSLEEP(100);

    _write4bits(lcd, (1 << PIN_DB4) | (1 << PIN_DB5));
    MSLEEP(5);

    _write4bits(lcd, (1 << PIN_DB4) | (1 << PIN_DB5));
    MSLEEP(5);

    _write4bits(lcd, (1 << PIN_DB4) | (1 << PIN_DB5));
    MSLEEP(15);

    _write4bits(lcd, (1 << PIN_DB5));

    lcdcommand(lcd, lcd->display_function);

    lcd->display_control |= (LCD_DC_DISPLAYON | LCD_DC_CURSOROFF | LCD_DC_CURSORBLINKOFF);
    lcdcommand(lcd, lcd->display_control);

    lcdclear(lcd);
    lcdhome(lcd);

    lcdsetcursor(lcd, lcd->column, lcd->row);
    lcdcursor(lcd, lcd->cursor);
    lcdblink(lcd, lcd->blink);
}
