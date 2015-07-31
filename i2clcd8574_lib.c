#include "i2clcd8574.h"

//LCD topology description, first four bytes defines subsequent row of text addresses and how
//are they mapped in internal RAM of LCD, last two bytes describes number of columns and number
//of rows
static const uint8_t topoaddr[][6] ={{0x00, 0x40, 0x00, 0x00, 40, 2}, //LCD_TOPO_40x2
                                     {0x00, 0x40, 0x14, 0x54, 20, 4}, //LCD_TOPO_20x4
                                     {0x00, 0x40, 0x00, 0x00, 20, 2}, //LCD_TOPO_20x2
                                     {0x00, 0x40, 0x10, 0x50, 16, 4}, //LCD_TOPO_16x4
                                     {0x00, 0x40, 0x00, 0x00, 16, 2}, //LCD_TOPO_16x2
                                     {0x00, 0x40, 0x00, 0x00, 16, 1}, //LCD_TOPO_16x1T1
                                     {0x00, 0x00, 0x00, 0x00, 16, 1}, //LCD_TOPO_16x1T2
				     {0x00, 0x40, 0x00, 0x40, 8,  2}, //LCD_TOPO_8x2
                                    };

//Text representation of various LCD topologies
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

//Pin mapping array
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

/**
 * write a byte to i2c device, sets backlight pin
 * on or off depending on current stup in LcdData struture
 * given as parameter.
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t byte to send to LCD
 * @return none
 * 
 */
static void _buswrite(LcdData_t *lcd, uint8_t data)
{
    data |= lcd->backlight ? (1 << PIN_BACKLIGHTON) : 0;
    LOWLEVEL_WRITE(lcd->handle, data);
}

/**
 * write a byte to i2c device, strobing EN pin of LCD
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t byte to send to LCD
 * @return none
 * 
 */
static void _strobe(LcdData_t *lcd, uint8_t data)
{
    _buswrite(lcd, data | (1 << PIN_EN));
    USLEEP(1);
    _buswrite(lcd, data & (~(1 << PIN_EN)));
    USLEEP(50);
}

/**
 * write a byte using 4 bit interface
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t byte to send to LCD
 * @return none
 * 
 */
static void _write4bits(LcdData_t *lcd, uint8_t value)
{
    _buswrite(lcd, value);
    _strobe(lcd, value);
}

/**
 * write a byte to a LCD splitting byte into two nibbles
 * of 4 bits each.
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t byte to send to LCD
 * @param uint8_t mode of communication (RS line of a LCD)
 * @return none
 * 
 */
static void lcdsend(LcdData_t *lcd, uint8_t value, uint8_t mode)
{
    uint8_t highnib = value & 0xF0;
    uint8_t lownib = value << 4;
    
    _write4bits(lcd, (highnib) | mode);
    _write4bits(lcd, (lownib) | mode);
}

/**
 * copy buffer of buffer from host to LCD
 * 
 * @param LcdData_t* lcd handler structure address
 * @return none
 * 
 */
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

/**
 * send command to a LCD. 
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t command byte to send
 * @return none
 * 
 */
void lcdcommand(LcdData_t *lcd, uint8_t data)
{
    lcdsend(lcd, data, 0);
}

/**
 * write byte of data to LCD
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t data byte to send
 * @return none
 * 
 */
void lcdwrite(LcdData_t *lcd, uint8_t data)
{
    uint8_t memaddr;
    
    memaddr = (lcd->column + (lcd->row * lcd->organization.columns)) % LCD_BUFFER_SIZE;
    lcd->buffer[memaddr] = data;
    
    lcdsend(lcd, data, (1 << PIN_RS));
}

/**
 * set cursor to given position. Row number and column number
 * depends on current LCD topology, if values are to high 
 * then are wrappped around and set to 0
 * 
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t column number
 * @param uint8_t row number
 * @return none
 * 
 */
void lcdsetcursor(LcdData_t *lcd, uint8_t column, uint8_t row)
{
     lcd->column = (column >= lcd->organization.columns ? 0 : column);
     lcd->row = (row >= lcd->organization.rows ? 0 : row);
    lcdcommand(lcd, LCD_DDRAM_SET | PTOMEMADDR(lcd, lcd->column, lcd->row));
}

/**
 * switches backlight on or off.
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t false value switechs backlight off, otherwise backlight will
 *                be switched on
 * @return none
 * 
 */
void lcdsetbacklight(LcdData_t *lcd, uint8_t backlight)
{
    lcd->backlight = backlight ? 1 : 0;
    _buswrite(lcd, lcd->backlight ? (1 << PIN_BACKLIGHTON) : 0);
}

/**
 * switches visibility of cursor on or off
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t false will switch it off, otherwise it will be switched on
 * @return none
 * 
 */
void lcdcursor(LcdData_t *lcd, uint8_t cursor)
{
    if (cursor)
        lcd->displaycontrol |= LCD_CURSOR;
    else
        lcd->displaycontrol &= ~LCD_CURSOR;
    
    lcdcommand(lcd, lcd->displaycontrol);
}

/**
 * switch blink of a cursor on or of
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t false will switch it off, otherwise it will be switched on
 * @return none
 * 
 */
void lcdblink(LcdData_t *lcd, uint8_t blink)
{
    if (blink)
        lcd->displaycontrol |= LCD_BLINK;
    else
        lcd->displaycontrol &= ~LCD_BLINK;
    
    lcdcommand(lcd, lcd->displaycontrol);
}

/**
 * will set LCD back to home, which usually is cursor set at position 0,0
 * 
 * @param LcdData_t* lcd handler structure address
 * @return none
 * 
 */
void lcdhome(LcdData_t *lcd)
{
    lcd->column = 0;
    lcd->row = 0;
    lcdcommand(lcd, LCD_HOME);
    MSLEEP(2);
}

/**
 * clears buffer by setting all bytes to 0x20 (ASCII code for space) and
 * send clear command to a LCD
 * 
 * @param LcdData_t* lcd handler structure address
 * @return none
 * 
 */
void lcdclear(LcdData_t *lcd)
{
    memset(lcd->buffer, 0x20, LCD_BUFFER_SIZE); //Fill buffer with spaces
    lcdcommand(lcd, LCD_CLEAR);
    MSLEEP(2);
}

/**
 * scrolls content of a LCD horizontally. This is internal feature of HD44780
 * it scrolls content without actually changing content of internal RAM, so
 * buffer of host stays intact as well
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t direction of a scroll, true - right, false - left
 * @return none
 * 
 */
void lcdscrollhoriz(LcdData_t *lcd, uint8_t direction)
{
    lcdcommand(lcd, LCD_DS_SHIFTDISPLAY | 
	      (direction ? LCD_DS_SHIFTRIGHT : LCD_DS_SHIFTLEFT));
}

/**
 * TODO - not implemented yet
 * 
 * @param LcdData_t* lcd handler structure address
 * 
 */
void lcdscrollvert(LcdData_t *lcd, uint8_t direction)
{
    //TODO: Vertical scroll
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
 * @return uint8_t bytes written
 * 
 */
uint8_t lcdprint(LcdData_t *lcd, const char *data)
{
    int i = 0;
    
    while (i < (lcd->organization.columns * lcd->organization.rows) && data[i])
    {
      if (data[i] == '\n' || data[i] == '\r')
      {
	lcd->column = 0;
	lcd->row = (lcd->row + 1) % lcd->organization.rows;
	i++;
	continue;
      } else if (data[i] == 0x08) //BS
      {
	if (lcd->column > 0)
	  lcd->column -= 1;
	i++;
	continue;
      }
      
      lcdcommand(lcd, LCD_DDRAM_SET | PTOMEMADDR(lcd, lcd->column, lcd->row));
      lcdwrite(lcd, data[i]);
      i++;
      lcd->column = (lcd->column + 1) % lcd->organization.columns;
      if (lcd->column == 0)
	lcd->row = (lcd->row + 1) % lcd->organization.rows;
    }
    
    return (lcd->column + (lcd->row * lcd->organization.columns));
}

/**
 * alows to define custom character. It is feature of HD44780 controller.
 * 
 * @param LcdData_t* lcd handler structure address
 * @param uint8_t character number to define 0-7
 * @param uint8_t* array of 8 bytes of bitmap definition
 * @return none
 * 
 */
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

/**
 * LCD deinitialization procedure 
 * 
 * @param LcdData_t* lcd handler structure address
 * @return none
 * 
 */
void lcdfinalize(LcdData_t *lcd)
{
    lcdsetbacklight(lcd, 0);
    lcdclear(lcd);
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
void lcdinit(LcdData_t *lcd, lcd_topology topo)
{
    memset(lcd->buffer, 0x20, LCD_BUFFER_SIZE); //Fill buffer with spaces
    
    if (topo > LCD_TOPO_8x2)
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
    MSLEEP(15);
    
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


