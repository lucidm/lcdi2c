Linux kernel module for alphanumeric LCDs on HD44780 with attached I2C expander based on PCF8574.

The module requires kernel version 3.x or higher, access to I2C bus on destination machine 
(module was tested on RaspberryPI 2 with kernel 4.1). It is currently able to drive one LCD at
once, so you're forced to use only one LCD with this module.

requirements
------------
* Currently running Linux Kernel and matched source in version 3.x or higher
* Doing few changes in Makefile before compilation
* Prepared Kernel source with proper configuration
* Loaded kernel modules for i2c bus and i2c-dev if you want to test LCD before module
  compilation.

compilation
-----------
* If you want to test your configuration and check if LCD is working properly, you can
  make test application by running "make proof" command in driver directory.
  For further information about testing connected LCD, please read "testing" entitled
  part of this document.
* Make sure you have properly installed Linux Kernel source, with currently running kernel
  configuration.
* If you didn't make compilation of Kernel previously, go to Kernel source directory and
  run "make modules_prepare"
* In the module directory open Makefile in your favourite editor and change
  line "KDIR := ../../linux/linux" to "KDIR := <path to your kernel source>", and 
  save Makefile. Now you can finally compile the module.
* To compile module run command "make" in module directory.

testing
-------
* Connect LCD to I2C interface on machine you'd like to test.
* After successfully making proof application, you can run "proof" executable. Application
  expects three numbers as arguments, I2C bus number, address of the device and LCD organization
  of connected LCD. You can call proof without parameters to display help and description how
  to set proper organization of LCD.
  Bus number depends on machine your LCD is connected to, some have more than one bus, other have
  only one but enumarated differently. For example, earlier version of RaspberryPI has I2C bus
  enumerated starting from 0, so the device file will be /dev/i2c-0 and bus number in this case
  should be set to 0, while RaspberryPI 2, has bus enumarated starting from 1, hence device
  file is /dev/i2c-1 on RPI 2. 
  You should also set proper device address, it depends on type of the chip used in expander,
  mostly you can expect two kind of them: PCF8574 which has defalt address range 0x20 - 0x27 and
  PCF8574A with address range set to one of 0x38 - 0x4E, check your case, i2cdetect tool will be
  very helpful in case you don't really know what address your expander has.
  Last parameter describes actual LCD configuration, how many lines of text it has and how internal
  memory of the display is organized. Not all configurartion are supported, but you can choose from
  most popular ones.
  Application will make some tests excpecting interaction from user, it didn't really check if LCD
  module reacts properly but you should see what's happening on LCD and know if something is wrong. 
  If everything went fine, you can excpect kernel module to work properly as well.
* Binary for kernel module sits in same directory as the code, so to make module running you should run
  command insmod lcdi2c.ko with proper arguments which are described below.

  
module arguments
----------------
* Module expect some arguments to be set before loading. If none of them is given, module will be loaded
  with default values, which may, or may not be suitable for your particular LCD module. If you want to
  read more about module parameters please run command "modinfo lcdi2c.ko" which will display short description
  about module it self and arguments you can set.
* busno  - bus number, same as in proof application.
* address - I2C expander address, default set to 0x27
* pinout - array of number of pins, expander is connected to LCD. Not all expanders are configured the same, so you
           can get different pin configurations, this parameter will help you to make this out. It's a list of
           numbers representing pshysical pin number connected to your LCD in order RS,RW,E,BL,D4,D5,D6,D7.
           So for example, if your expander pins are pshysically connected in following configuration:
              D4 = PIN.0, D5 = PIN.1, D6 = PIN.2, D7 = PIN.3, RS = PIN.4, RW = PIN.5, E = PIN.6, BL = PIN.7,
           you should set this argument to: 4,5,6,7,0,1,2,3. 
           Pin BL is used for switching backlight of LCD. Default value is: 0,1,2,3,4,5,6,7 some popular cheap
           expanders (usually with black solder mask on PCB) use this configurartion.
* cursor - set to 1 will show cursor at start, 0 - will prevent from displaying the cursor. Default set to 1
* blink  - 1 will blink current character position, 0 - blinking character will be disabled. Default set to 1
* major  - driver will register new device in /dev/i2clcd, you can force major number of the device or leave it
           for kernel to decide for you. Preffered is not to give this parameter and let the kernel decide.
           You can later read this number form /sys
* topo   - LCD topology, same as described in "testing" section. Default set to 4 (16x2).

device interface
----------------
* Module has two sets of interfaces you can interact with your LCD. It registers /dev/lcdi2c character device, which you
  can open and manipulate using standard open/close/read/write/ioctl quintet or through /sys interface. However you
  are able to use /dev/lcdi2c device, /sys interface is preffered. 
  Let's work out /sys interface first. After module was loaded into kernel, driver will register new class devices called
  "/sys/class/alphalcd" in which named after the device file "lcdi2c"  directory will be created.
* "/sys/class/alphalcd/lcdi2c" is the directory containing all interace files required to interact with LCD, here is
  alphabetically ordered list of files with short descritpiion:
  - backlight - write "0" to this file to switch backlight off, "1" to switch it on. You can read this file to get
                current status of backlight.
  - blink     - write "0" to switch blinking character off, "1" to switch it on. Reading this file will tell you 
                about current status of blinking.
  - clear     - write only, writting this file with value "1" will clear LCD.
  - cursor    - write "0" for swithich cursor off, "1" for switch cursor on. Reading this file will tell you about
                current status of cursor.
  - cursorpos - using this file will help you to set or read current cursor position. This is only file in which values
                are actually binary in meaning of two bytes sent or read representing column and row number.
  - data      - driver uses internal buffer which is 1:1 internal LCD RAM mirror, everything you write to LCD through
                driver interface will be represented in this file. You can also write to this file, and all that you wrote
                would be display on the LCD. Keep in mind, size of RAM of LCD is limited only to 104 bytes.
                Internal RAM organization depends on "topo" parameter, however reading this file will represent what is actually
                on the screen, and writting to it will have 1:1 representation on the LCD, configuration of this buffer depends on
                current LCD display topology (for some displays, not all bytes in RAM are used to display data and it's true for
                this buffer file too).
                If you want to know more about this kind of displays RAM organization, please read link below
                http://web.alfredstate.edu/weimandn/lcd/lcd_addressing/lcd_addressing_index.html
                which will greatly explain how RAM organization differs in different LCD types.
  - dev       - description of major:minor device number associated with /dev/lcdi2c device file.
  - home      - writting "1" will cause LCD to move cursor to first column and row of LCD.
  - meta      - description of currently used LCD. You can only read this file to get current LCD configurartion.
  - reset     - write only file, "1" write to this file will reset LCD to state after module was loaded.
  - scrollhz  - scroll horizontally, write "1" to this file to scroll content of LCD horizontally by 1 character to the right,
                if you want to scroll to the left, write "0" to this file. This scrolling technique will not change internal RAM of
                your display, so "data" file will also keep its content intact. Opposite character which leave display during 
                the scroll will appear on the on the other outermost position, so this scroll always keeps information on the screen.
                This is the internal HD44780 mechanism.
  - uevent    - udev stuff.