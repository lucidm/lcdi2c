#include "i2clcd8574.h"
#include <string.h>
#include <time.h>

uint8_t pinTranslation[8] = {};

int file;
uint8_t bus = 1;
char *devpath = "/dev/i2c-%d", dev[20];
uint8_t address = 0x27;
uint8_t topo = 4;


int main(int argc, char **argv)
{
    char buf[10] = {0};
    char customChars[2][8] = {
        {
            0b01000,
            0b00100,
            0b01110,
            0b11011,
            0b10011,
            0b11111,
            0b11111,
            0b01110
        },
        {
            0b00010,
            0b00100,
            0b01110,
            0b11011,
            0b10011,
            0b11111,
            0b11111,
            0b01110
        },
    };
    LcdData_t lcd;
    
    lcd.backlight = 0;
    lcd.cursor = 0;
    lcd.blink = 0;
    lcd.row = 0;
    lcd.column = 0;

    
    if (argc == 1)
    {
        fprintf(stderr, "Wrong number of arguments.\n\tProper command call: "
	       "%s <bus number> <hex address> <topology>\n"
               "\tWhere bus number is number of bus LCD is connected to,"
	       " default set to: %d\n"
               "\taddress is address of the I2C expander in hexadecimal,"
	       " default set to: 0x%02X\n"
               "\ttopology depends on your LCD configuration and is "
	       "one of below options:\n"
               "\t\t0 - 40x2\n"
               "\t\t1 - 20x4\n" 
               "\t\t2 - 20x2\n" 
               "\t\t3 - 16x4\n" 
               "\t\t4 - 16x2 - default\n"
               "\t\t5 - 16x1 Type 1\n"
               "\t\t6 - 16x1 Type 2\n", 
               argv[0],
               bus,
               address);
        exit(-1);
    }
    
    if (argc == 2) //Assuming topology was only given
    {
        if (argv[1][0] < '0' || argv[1][0] > '6')
        {
            fprintf(stderr, "Topology can be a number between 0 and 9\n");
            exit(-1);
        }
        topo = argv[1][0] - '0';
    }
    
    if (argc == 3)
    {
        if (argv[2][0] < '0' || argv[2][0] > '6')
        {
            fprintf(stderr, "Topology can be a number between 0 and 9\n");
            exit(-1);
        }
        topo = argv[2][0] - '0';
        
        address = (uint8_t)strtol(argv[1], NULL, 0);
    }
    
    if (argc == 4)
    {
        if (argv[3][0] < '0' || argv[3][0] > '6')
        {
            fprintf(stderr, "Topology can be a number between 0 and 9\n");
            exit(-1);
        }
        topo = argv[3][0] - '0';
        
        address = (uint8_t)strtol(argv[2], NULL, 0);
        
        bus = (uint8_t)strtol(argv[1], NULL, 10);
    }
    
    snprintf(dev, 20, devpath, bus);
    if ((lcd.handle = open(dev, O_RDWR)) < 0)
    {
        fprintf(stderr, "Failed to open I2C device %s\n", dev);
        exit(-1);
    }
    
    if (ioctl(lcd.handle, I2C_SLAVE, address) < 0)
    {
        fprintf(stderr, "Failed to acquire bus access at address 0x%02X.", 
		address);
        close(lcd.handle);
        exit(-1);
    }
    
    srand(time(NULL));

    printf("LCD init...");
    lcdinit(&lcd, topo);
    printf(" init done.\n");
    
    printf("LCD on BUS:\t%d\nAt address:\t0x%02X\nOrganization:\t%s\n\n", bus, 
	   address, lcd.organization.toponame);
    
    printf("LCD Backlight ON...");
    lcdsetbacklight(&lcd, 1);
    printf(" done (press Enter)");
    getchar();
    
    printf("LCD Cursor ON...");
    lcdcursor(&lcd, 1);
    printf(" done (press Enter)");
    getchar();
    
    printf("LCD Cursor OFF...");
    lcdcursor(&lcd, 0);
    printf(" done (press Enter)");
    getchar();

    printf("LCD Blink ON...");
    lcdblink(&lcd, 1);
    printf(" done (press Enter)");
    getchar();

    printf("LCD Blink OFF...");
    lcdblink(&lcd, 0);
    printf(" done (press Enter)");
    getchar();
    
    
    printf("LCD Testing Cursor position...");
    int i, j, k = 0, l = 0;
    for(i = 0; i < LCD_BUFFER_SIZE; i++)
    {
        k = rand() % LCD_BUFFER_SIZE;
        j = k % lcd.organization.columns;
        l = k / lcd.organization.columns;
        lcdcustomchar(&lcd, 6, customChars[(i / 8) % 2]);
        lcdsetcursor(&lcd, j, l);
        lcdwrite(&lcd, '\06');
        MSLEEP(30);
    }

    printf(" done (press Enter)");
    getchar();
    lcdsetcursor(&lcd, 0, 0);
    
    lcdcursor(&lcd, 1);
    lcdprint(&lcd, "01234567899876543210"
                   "ABCDEFGHIJKLMNOPQRST"
		   "abcdefghijklmnopqrst"
		   "!@#$%^&*())(*&^%$#@!");
    printf(" done (press Enter)");
    getchar();
    lcdsetcursor(&lcd, lcd.organization.columns - 1, lcd.organization.rows - 1);
    
    for(i = 0; i < lcd.organization.columns * lcd.organization.rows; i++)
    {
        lcdscrollhoriz(&lcd, 1);
        MSLEEP(60);
    }
    
    for(i = 0; i < lcd.organization.columns * lcd.organization.rows; i++)
    {
        lcdscrollhoriz(&lcd, 0);
        MSLEEP(60);
    }

    printf("LCD Home...");
    lcdhome(&lcd);
    printf(" done (press Enter)");
    getchar();

    printf("LCD Testing Clear...");
    lcdclear(&lcd);
    printf(" done (press Enter)");
    getchar();
    
    lcdfinalize(&lcd);
    
    close(lcd.handle);
}