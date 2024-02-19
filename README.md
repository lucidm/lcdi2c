Linux kernel module for alphanumeric LCDs using HD44780 with PCF8574 I2C IO expander attached.

Prior to compilation, make sure you have linux-headers package installed. 
For RaspberryPi with Debian-based distro run ```sudo apt install linux-headers-rpi``` to install required package.
Or ```sudo apt install linux-headers-$(uname -r)``` for other Debian-based distros.

This version is using Device Tree overlay to load the module on boot.
In order to install device tree overlay for LCD module, you need to have device tree compiler installed:
```bash
sudo apt install device-tree-compiler
```


Requirements
------------
* Running Linux Kernel and its source code. Version 3.x or higher is supported.
* Prepared Kernel source for modules compilation.


Compile the module and install on running kernel:
```bash
make CONFIG_LCDI2C=m
sudo make CONFIG_LCDI2C=m install
````

Compile the overlay and install it:
```bash
make CONFIG_LCDI2C=m dtbo
sudo cp lcdi2c-overlay.dtbo /boot/overlays/
sudo nano /boot/config.txt
```
Add the following line to the end of the file:
```bash
dtoverlay=lcdi2c-overlay
```
Save the file and reboot the system, from this point on, LCD module will be loaded automatically on boot.


compilation
-----------

* Clone the repository to your local machine:
   * ```git clone https://github.com/lucidm/lcdi2c.git```

* Enter the directory:
    * ```cd lcdi2c```

* Compile the module:
    * ```make CONFIG_LCDI2C=m```

* Install the module:
    * ```sudo make install```

* Finally, load the module:
    * ```sudo modprobe lcdi2c topo=2 swscreen=0```
    where **topo** is LCD topology, **swscreen** is switch for welcome screen (1 - on/ 0 - off)
    * bus number and expander address comes from overlay configuration, if you'd like to change it, you can change the bus number in lcdi2c.dts file.
 
* Run the example:
     * ```./examples/lcddev.py```

module arguments
----------------
* Module expects some arguments to be set before loading. If none of them is given, module will load
  with default ones, which may, or may not be suitable for your particular configuration. Running 
  ```modinfo lcdi2c.ko``` will give you information about module expected arguments. Here's the list:
  
* **address** - I2C expander address, default set to 0x27

* **pinout** - array of number of pins configuration. Not all expanders are configured the same, so you
           have to be prepared for different pin configurations, this parameter will help you to make this out. 
           This parameter is a list of numbers representing pshysical pin number of the expander chip, 
           connected to your LCD control lines in order given order: RS,RW,E,BL,D4,D5,D6,D7.
           So for example, if your expander pins are pshysically connected in following configuration:
              D4 = PIN.0, D5 = PIN.1, D6 = PIN.2, D7 = PIN.3, RS = PIN.4, RW = PIN.5, E = PIN.6, BL = PIN.7,
           you should set this argument to: 4,5,6,7,0,1,2,3. 
           Pin BL is used for switching backlight of an LCD. Default value is: 0,1,2,3,4,5,6,7 some popular cheap
           expanders (usually with black solder mask on PCB) use this pin configuration.
           
* **cursor** - set to 1 will show cursor at start, 0 - will prevent from displaying the cursor. Default set to 1

* **blink**  - 1 will blink current character position, 0 - blinking character will be disabled. Default set to 1

* **major**  - driver will register new device in /dev/i2clcd, you can force major number of the device by using this 
           configuration option or leave it for kernel to decide. 
           Preffered is not to give this parameter and let the kernel decide. 
           You can later read the number form /sys filesystem.
           
* **topo**   - LCD topology, same as described in "testing" section. Default set to 4 (16x2).
* **swscreen** - switch for welcome screen, 1 - on, 0 - off. Default set to 1


/sys device interface
----------------
* Module has two sets of interfaces you can interact with your LCD. 
  * Through ```/dev/lcdi2c``` character device, which you can open and manipulate using standard open/close/read/write/ioctl quintet, or through /sys interface. 
  * Or you can use /sys interface which is preferred. But please bear in mind, that to get access to /sys interface, you have to have root privileges for write to some of the attributes, while IOCTL interface allow unprivileged users to get access to the device entirely.
 
  Let's take a look at /sys interface first. Once the module is loaded, driver will register new class device 
  ```/sys/class/alphalcd``` containing "lcdi2c" directory.
  
* ```/sys/class/alphalcd/lcdi2c``` All files in this directory are interfacing with lcdi2c module.
  
  - **brightness** write "0" to this file to switch backlight off, "1" to switch it on. Reading this file will tell you about
                current status of backlight.
                
  - **blink**   write "0" to switch blinking character off, "1" to switch it on. Reading this file will tell you 
                about current status of cursor's blinking.
                
  - **clear**   write only, writing "1" to this file will clear LCD.
  
  - **cursor**  write "0" for switch cursor off, "1" for switch cursor on. Reading this file will tell you about
                current status of the cursor.
                
  - **customchar** HD44780 and its clones usually provide way to define 8 custom characters, this file will let you to
                define those characters. This file is binary, each character consists of 9 bytes, first byte informs
                about character number (characters have numbers from 0 to 7), next eight bytes is bitmap definition of
                the character itself. Next 9 bytes repeats this pattern for another character. Total length of this file is
                72 bytes. Writing to this file will make you able to define your own set of new 8 characters. If you want
                to define new bitmat for a character, write at least 9 bytes, first byte is character number you'd like
                to change, next 8 bytes defines bitmap of the character. You are allowed to define more than one character per
                write, but keep in mind, you're always writing 9 * n bytes, where n is number of characters you'd like to define.
                
    - **data**  driver uses internal buffer which is 1:1 internal LCD RAM mirror, everything you write to LCD through
                driver interface will be represented in this file. You can also write to this file, and all that you wrote
                would be visible on the display. Keep in mind, size of RAM of LCD is limited only to 104 bytes.
                Internal RAM organization depends on "topo" parameter, however reading this file will represent what is actually
                on the screen, and writing to it will have 1:1 representation on the LCD, configuration of this buffer depends on
                current LCD display topology (for some displays, not all bytes in RAM are used to display data and it's true for
                this buffer file too).
                To get more information how those LCD RAMs are organized, please take a look at this page: 
                https://web.alfredstate.edu/faculty/weimandn/lcd/lcd_addressing/lcd_addressing_index.html
               
  - **dev**       - description of major:minor device number associated with /dev/lcdi2c device file.
  
  - **home**      - writing "1" will cause LCD to move cursor to first column and row of LCD.
  
  - **meta**      - description of currently used LCD. Read-only file in YAML format. This file contains information about
                    LCD topology, addresses, IOCTLs supported by the driver, etc and are used to generalize the interface for higher level API.
                
  
  - **position**  - this file will help you to set or read current cursor position. This file contains two bytes,
	        value of first byte informs about current cursor position at column, second byte contain information
	        about current cursor position at row. Writing two bytes to this file, will set cursor at position. 
	        
  - **reset**     - write only file, "1" write to this file will reset LCD to state after module was loaded.
  
  - **scrollhz**  - scroll horizontally, write "1" to this file to scroll content of LCD horizontally by 1 character to the right,
                scrolling to the left is made by writing "0" to this file. This scrolling technique will not change contents of internal RAM of
                your display, so "data" file will also keep its content intact. Opposite character which leave display during 
                the scroll will appear on the on the other outermost position, so this scroll always keeps information on the screen.
                This is the internal HD44780 mechanism.

/dev/lcdi2c device interace
---------------------------
* Module has alternative interface to drive connected LCD. It registers /dev/lcdi2c device file, which you're able to write to or read from.
  Typical method for accessing such devices is to use open() function, complementary close() function, read() and write() functions. To be able
  to use rest of features of HD44780 some ioctls commands are provided. List of all suprted IOCTLS commands with codes is available through 
  /sys/class/alphalcd/lcdi2c/meta file under IOCTLS: section. Command codes repeat functionality of /sys interface. However some are unavailable, like 
  "meta" for example. Reading and writing to the device is also different, you should write to it using write() function and complementary read()
  function to read data from device. Below is a list of supported IOCTL commands:
  
  - **CLEAR** - writing "1" as argument of this ioctl, will clear the display
  - **HOME**  - writing "1" as argument of this ioctl, will move cursor to first column and row of the display
  - **RESET** - writing "1" will reset LCD to default state
  - **GETCHAR** - this ioctl will return ASCII value of current character (character cursor is hovering at)
  - **SETCHAR** - this ioctl will set given ASCII character at position pointed by current cursor setting
  - **GETLINE** - gets text from current row of the display
  - **SETLINE** - sets text of current row of the display
  - **GETBUFFER** - gets whole buffer of the display, no special characters are interpreted, the lines ends according to LCD topology specification (for 16x2 LCD 16th character will be the last character from first line, 17th - first character from the second line, etc.). 
  - **SETBUFFER** - sets whole buffer of the display, it's up to host to provide correct formatting of the text, lines end marking is according to LCD topology specification.
  - **GETPOSITION** - will return current cursor position as two bytes, value of first byte represents current column, second one - current row
  - **SETPOSITION** - writing two bytes to this ioctl will set current cursor position on the display
  - **GETBACKLIGHT** - will return "0" if backlight is currently switched off, "1" otherwise
  - **SETBACKLIGHT** - "1" written to this ioctl will switch backlight on or if "0" is written, will switch it off
  - **GETCURSOR** - returns current cursor visibility status, "0" - invisible, "1" - visible
  - **SETCURSOR** - sets cursor visibility, "0" - invisible, "1" - visible
  - **GETBLINK** - returns blinking cursor status, "0" - cursors is not blinking, "1" - cursor is blinking
  - **SETBLINK** - sets or resets cursor blink, "0" - cursor will blink, "1" - cursor will not blink
  - **SCROLLHZ** - wrtting "0" to this ioctl will scroll screen to the left by one column, "1" - will scroll to the right
  - **SCROLLVERT** - writing "0" to this ioctl will scroll screen up by one row, "1" - will scroll down, the last or the first one row will be set empty after this operation  
  - **SETCUSTOMCHAR** - allows to define new character map for given character number. This ioctl expects 9 bytes of data exactly, first byte is character number
                 eight subsequent bytes defines actual bitmap of font. This control differs from "customchar" in a way, that you cannot send more than
                 one character definition at once. If you want to define more than one character, just call this ioctl multiple times for each character
                 you would like to define.
  - **GETCUSTOMCHAR** - Gets custom char bitmap definition, first byte marks the character number, for which you'd like to get bitmap definition from.
                  
media
-----
  - https://youtu.be/CNj7ykGRBHw Module working with 8x2 LCD
  - https://youtu.be/3B-uGth-hZk Module working with 16x2 LCD
  - https://youtu.be/qbU7RORUYO8 Module working with 40x2 LCD

Ruby GEM
--------
Francesc Oller
