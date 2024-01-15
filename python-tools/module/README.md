# Installation of Python module and lcdi2c kernel module with Device Tree overlay 

First, make sure you have the latest version of setuptools installed:
```bash
$ pip3 install setuptools
```

Go to python-tools/module/ and run:
```bash
$ cd python-tools/module/
$ python -m build
* Creating virtualenv isolated environment...
* Installing packages in isolated environment... (setuptools>=59.6.0, wheel>=0.37.0)
* Getting build dependencies for sdist...
running egg_info
writing src/alphalcd.egg-info/PKG-INFO
writing dependency_links to src/alphalcd.egg-info/dependency_links.txt
writing entry points to src/alphalcd.egg-info/entry_points.txt
writing top-level names to src/alphalcd.egg-info/top_level.txt
reading manifest file 'src/alphalcd.egg-info/SOURCES.txt'
adding license file 'LICENSE'
writing manifest file 'src/alphalcd.egg-info/SOURCES.txt'
* Building sdist...

...

adding 'lcdi2c/__init__.py'
adding 'lcdi2c/alphalcd.py'
adding 'lcdi2c/lcddisp.py'
adding 'alphalcd-0.2.4.dist-info/LICENSE'
adding 'alphalcd-0.2.4.dist-info/METADATA'
adding 'alphalcd-0.2.4.dist-info/WHEEL'
adding 'alphalcd-0.2.4.dist-info/entry_points.txt'
adding 'alphalcd-0.2.4.dist-info/top_level.txt'
adding 'alphalcd-0.2.4.dist-info/RECORD'
removing build/bdist.linux-x86_64/wheel
Successfully built alphalcd-0.2.4.tar.gz and alphalcd-0.2.4-py3-none-any.whl
```
After successful build, install the module with:
```bash
$ cd python-tools/module/
$ pip3 install --force-reinstall dist/alphalcd-0.2.4-py3-none-any.whl
```
When pip3 was run as unprivileged user default installation path is ```~/.local/lib/<python_version>/site-packages/alphalcd-0.2.4-py3.7.egg/alphalcd```
Module installed this way is accessible only for user who installed the module. If you want to make it available system-wide, run pip3 install with sudo.

## Usage

Simple usage example:
```python
from lcdi2c.alphalcd import AlphaLCD, LCDPrint

lcd = AlphaLCD()
with LCDPrint(lcd) as p:
    p.clear()
    p.home()
    p.print("Hello World!")
```

Both AlphaLCD and LCDPrint classes are context managers, The difference is that AlphaLCD class is the lower level implementation. It communicates with the kernel module directly using IOCTL calls and writes to character device installed by kernel module.
LCDPrint class on the othe hand, implements higher level API with use of AlphaLCD class to implement standard python method calls and accessors.

Here is a list of a few examples of how to use the module:

1. Clearing the LCD screen:
```python
from lcdi2c.alphalcd import AlphaLCD, LCDPrint

lcd = AlphaLCD()
with LCDPrint(lcd) as p:
    p.clear()
```

2. Printing a message on the LCD screen:
```python
from lcdi2c.alphalcd import AlphaLCD, LCDPrint

lcd = AlphaLCD()
with LCDPrint(lcd) as p:
    p.print("Hello, World!")
```

3. Setting the cursor position and printing a message:
```python
from lcdi2c.alphalcd import AlphaLCD, LCDPrint

lcd = AlphaLCD()
with LCDPrint(lcd) as p:
    p.set_position(5, 2)  # Set cursor to column 5, row 2
    p.print("Hello, World!")
```

```python
from lcdi2c.alphalcd import AlphaLCD, LCDPrint

lcd = AlphaLCD()
with LCDPrint(lcd) as p:
    p.print("Hello, World!", 5, 2) # Set cursor to column 5, row 2 and print message
```

4. Turning on the backlight:
```python
from lcdi2c.alphalcd import AlphaLCD, LCDPrint

lcd = AlphaLCD()
with LCDPrint(lcd) as p:
    p.backlight = True
```

5. Getting the current cursor position:
```python
from lcdi2c.alphalcd import AlphaLCD, LCDPrint

lcd = AlphaLCD()
with LCDPrint(lcd) as p:
    position = p.get_position()
    print(f"Current cursor position: {position}")
```

Both AlphaLCD and LCDPrint classes are context managers, The difference is that AlphaLCD class is the lower level implementation that communicates with the kernel module using IOCTLS and character device the module creates.
LCDPrint class on the other hand, is build on top of AlphaLCD class and implements higher level API with less "dense" method calls and accessors.


Below is example of the code producing the same result using both AlphaLCD and LCDPrint classes: 
```python
with AlphaLCD() as a:
     print(f"Topology:{a.columns}x{a.rows}")
     a(LCDCommand.CLEAR.value)
     a(LCDCommand.HOME.value)
     a(LCDCommand.SET_BLINK.value, value = 1)
     a(LCDCommand.SET_CURSOR.value, value = 1)
     position = a(LCDCommand.GET_POSITION.value)
     print(f"Position:{position.column}, {position.row}")
     a(LCDCommand.SET_BUFFER.value, buffer="Hello World!    0123456789abcdef")
     buffer = a(LCDCommand.GET_BUFFER.value)
     print(f"Buffer:{buffer.buffer}")
     a(LCDCommand.SET_POSITION.value, column=0, row=1)
     line = a(LCDCommand.GET_LINE.value)
     print(f"Line 1:{line.line}")
     a(LCDCommand.SCROLL_VERT.value, direction=0, line="Bottom Line")
```
the result:
```
    ...
Topology:16x4
Position:0, 0
Buffer:b'Hello World!    0123456789abcdef                                                    '
Line 1:b'0123456789abcdef'

```

```python
with LCDPrint(lcd) as f:
     print(f"Topology:{lcd.columns}x{lcd.rows}")
     f.clear()
     f.home()
     f.blink = True
     f.cursor = True
     column, row = f.get_position()
     print(f"Position: {column}, {row}")
     f.set_buffer("Hello World!    0123456789abcdef")
     print(f"Buffer:{f.get_buffer()}")
     f.set_position(0, 1)
     print(f"Line 1:{f.get_line(1)}")
     f.scroll_vert("Bottom Line", False)
```
```
     ...
Topology:16x4
Position: 0, 0
Buffer:Hello World!    0123456789abcdef                                                    
Line 1:0123456789abcdef

```
You can also spot the difference with returning values, AlphaLCD does not process output of the IOCTL calls, the value returned is represent as bytes object, while LCDPrint converts the output to Python native types. 

