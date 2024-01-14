#!/usr/bin/env python3

import array
import fcntl
import yaml

from ctypes import c_char, c_bool, c_uint8, c_uint32, Structure
from enum import Enum
from typing import Tuple, Iterable

META_FILE_PATH = "/sys/class/alphalcd/lcdi2c/meta"


class LCDMisc(Enum):
    LCD_LINE_LEN = 40
    LCD_BUFFER_LEN = 20 * 4 + 4


class LCDCharArgs(Structure):
    _fields_ = [
        ("value", c_uint8),
    ]

    def __init__(self, value: int = None):
        super().__init__()
        if value is not None:
            self.value = value


class LCDBoolArgs(Structure):
    _fields_ = [
        ("value", c_bool),
    ]

    def __init__(self, value: bool = None):
        super().__init__()
        if value is not None:
            self.value = value


class LCDLineArgs(Structure):
    _fields_ = [
        ("line", c_char * LCDMisc.LCD_LINE_LEN.value),
    ]

    def __init__(self, line: str = None):
        super().__init__()
        if line:
            self.line = (line + " " * (LCDMisc.LCD_LINE_LEN.value - len(line))).encode("ascii")


class LCDScrollArgs(Structure):
    _fields_ = [
        ("direction", c_uint32),
        ("line", c_char * LCDMisc.LCD_LINE_LEN.value),
    ]

    def __init__(self, line: str = None, direction: bool = None):
        super().__init__()
        if line:
            self.line = (line + " " * (LCDMisc.LCD_LINE_LEN.value - len(line))).encode("ascii")
        if direction is not None:
            self.direction = direction


class LCDBufferArgs(Structure):
    _fields_ = [
        ("buffer", c_char * LCDMisc.LCD_BUFFER_LEN.value),
    ]

    def __init__(self, buffer: str = None):
        super().__init__()
        if buffer:
            self.buffer = (buffer + " " * (LCDMisc.LCD_BUFFER_LEN.value - len(buffer))).encode("ascii")


class LCDPositionArgs(Structure):
    _fields_ = [
        ("column", c_uint8),
        ("row", c_uint8),
    ]

    def __init__(self, column: int = None, row: int = None):
        super().__init__()
        if column is not None:
            self.column = column
        if row is not None:
            self.row = row


class LCDCustomCharArgs(Structure):
    _fields_ = [
        ("index", c_uint8),
        ("data", c_char * 8),
    ]

    def __init__(self, index: int = None, data: Iterable[int] = None):
        super().__init__()
        if index is not None:
            self.index = index
        if data is not None:
            self.data = array.array("B", data).tobytes()


class LCDCommand(Enum):
    GET_VERSION = "GETVERSION"
    GET_CHAR = "GETCHAR"
    SET_CHAR = "SETCHAR"
    GET_LINE = "GETLINE"
    SET_LINE = "SETLINE"
    GET_BUFFER = "GETBUFFER"
    SET_BUFFER = "SETBUFFER"
    RESET = "RESET"
    HOME = "HOME"
    GET_BACKLIGHT = "GETBACKLIGHT"
    SET_BACKLIGHT = "SETBACKLIGHT"
    GET_CURSOR = "GETCURSOR"
    SET_CURSOR = "SETCURSOR"
    GET_BLINK = "GETBLINK"
    SET_BLINK = "SETBLINK"
    SCROLL_HZ = "SCROLLHZ"
    SCROLL_VERT = "SCROLLVERT"
    GET_CUSTOMCHAR = "GETCUSTOMCHAR"
    SET_CUSTOMCHAR = "SETCUSTOMCHAR"
    CLEAR = "CLEAR"
    SET_POSITION = "SETPOSITION"
    GET_POSITION = "GETPOSITION"

    def __init__(self, ioctl_name):
        self.ioctl_name = ioctl_name

    def __str__(self):
        return self.ioctl_name


class LCDDirection(Enum):
    NONE = 0
    WRITE = 1
    READ = 2
    WRITEREAD = 3


class IOCTLDataType(Enum):
    CHAR = 1
    STRING = 2


IOCTLFMT = {
    LCDCommand.GET_CHAR: ("1B", LCDCharArgs),
    LCDCommand.SET_CHAR: ("1B", LCDCharArgs),
    LCDCommand.GET_LINE: (f"{LCDMisc.LCD_LINE_LEN.value}B", LCDLineArgs),
    LCDCommand.SET_LINE: (f"{LCDMisc.LCD_LINE_LEN.value}B", LCDLineArgs),
    LCDCommand.GET_BUFFER: (f"{LCDMisc.LCD_BUFFER_LEN.value}B", LCDBufferArgs),
    LCDCommand.SET_BUFFER: (f"{LCDMisc.LCD_BUFFER_LEN.value}B", LCDBufferArgs),
    LCDCommand.GET_BACKLIGHT: ("1B", LCDBoolArgs),
    LCDCommand.SET_BACKLIGHT: ("1B", LCDBoolArgs),
    LCDCommand.GET_CURSOR: ("1B", LCDBoolArgs),
    LCDCommand.SET_CURSOR: ("1B", LCDBoolArgs),
    LCDCommand.GET_BLINK: ("1B", LCDBoolArgs),
    LCDCommand.SET_BLINK: ("1B", LCDBoolArgs),
    LCDCommand.SCROLL_HZ: ("1B", LCDBoolArgs),
    LCDCommand.SET_POSITION: ("2B", LCDPositionArgs),
    LCDCommand.GET_POSITION: ("2B", LCDPositionArgs),
    LCDCommand.SET_CUSTOMCHAR: ("1B8B", LCDCustomCharArgs),
    LCDCommand.GET_CUSTOMCHAR: ("1B8B", LCDCustomCharArgs),
    LCDCommand.SCROLL_VERT: (f"1L{LCDMisc.LCD_LINE_LEN.value}B", LCDScrollArgs),
    LCDCommand.RESET: ("0B", None),
    LCDCommand.HOME: ("0B", None),
    LCDCommand.CLEAR: ("0B", None),
    LCDCommand.GET_VERSION: ("0B", None),
}


class AlphaLCDInitError(Exception):
    pass


class AlphaLCDIOError(Exception):
    pass


class IOCTLBase:
    """
    Base class for IOCTL calls.
    Derived classes must implement __call__ method with no arguments.
    Constructor arguments:
        ioctl_name: IOCTL name
        ioctl_value: IOCTL value
        nr: IOCTL number
        base: IOCTL base
        length: IOCTL length
        direction: IOCTL direction (LCDDirection instance)
        fmt: struct.pack format string (optional) - if not specified, the default bytes format is used
    set_value() method arguments:
        file: file descriptor
        value: IOCTL name (one of LCDCommand values)
    """
    def __init__(self, ioctl_name: str, ioctl_value: int, nr: int, base: int, length: int, direction: LCDDirection, fmt: str = None):
        self.fmt = fmt
        self.ioctl_name = ioctl_name
        self.ioctl_value = ioctl_value
        self.nr = nr
        self.base = base
        self.length = length
        self.direction: LCDDirection = direction

    def __str__(self):
        """
        String representation of IOCTL callable.
        :return:
        """
        return (
            f"IOCTL Name:{self.ioctl_name} Base:{self.base:#02x} SEQ:{self.nr:#02x} Direction:{self.direction.name} "
            f"Length:{self.length}")


class IOCTL(IOCTLBase):
    """
    IOCTL call with no arguments and no value returned.
    """
    def __call__(self, file, **__):
        return fcntl.ioctl(file, self.ioctl_value)


class IOCTLWrite(IOCTLBase):
    """
    IOCTL call with value to write.
    IOCTLs usually have a fixed length, so the value is padded with spaces or truncated to fit.
    """
    def __call__(self, file, **kwargs):
        ioctl_arg = self.fmt[1](**kwargs)
        a = array.array("B", bytearray(ioctl_arg))
        fcntl.ioctl(file, self.ioctl_value, a, True)
        return self.fmt[1].from_buffer_copy(a)


class IOCTLRead(IOCTLBase):
    """
    IOCTL call with no value to write and value to read.
    IOCTLs usually have a fixed length, so the value returned is not necessarily the same as the default IOCTL length
    like for GET_LINE which returns the whole line (40 bytes) instead of the default actual line length of bytes
    depending on the LCD width. For example in a case of GET_LINE the actual length can be determined from number
    of columns.
    """
    def __call__(self, file, **kwargs):
        ioctl_arg = self.fmt[1](**kwargs)
        a = array.array("B", bytearray(ioctl_arg))
        fcntl.ioctl(file, self.ioctl_value, a, True)
        return self.fmt[1].from_buffer_copy(a)


class IOCTLWriteRead(IOCTLBase):
    """
    IOCTL call with value to write and value to read.
    Function expects a tuple of ints or a string which is converted to array of bytes.
    """
    def __call__(self, file, **kwargs):
        ioctl_arg = self.fmt[1](**kwargs)
        a = array.array("B", bytearray(ioctl_arg))
        fcntl.ioctl(file, self.ioctl_value, a, True)
        return self.fmt[1].from_buffer_copy(a)


class IOCTLDirToIOCTLDatatype(Enum):
    NONE = IOCTL
    WRITE = IOCTLWrite
    READ = IOCTLRead
    WRITEREAD = IOCTLWriteRead


class IOCTLManager(IOCTLBase):
    """
    IOCTL manager class creates a dictionary of IOCTL callable objects from a dictionary of IOCTL names and values.
    Provides simple interface to call IOCTLs by name and call IOCTL with a proper direction encoded in the IOCTL value.
    """
    def __init__(self, ioctls: dict):
        self.ioctls = {}
        for ioctl, value in ioctls.items():
            nr, ioc_type, length, direction = self.__cmdparse(value)
            call = IOCTLDirToIOCTLDatatype[LCDDirection(direction).name].value(ioctl,
                                                                               value,
                                                                               nr,
                                                                               ioc_type,
                                                                               length,
                                                                               LCDDirection(direction),
                                                                               IOCTLFMT.get(LCDCommand(ioctl)))
            self.ioctls.update({ioctl: call})

    def __call__(self, ioctl_name: str, file, **kwargs):
        """
        Call IOCTL by name.
        :param ioctl_name:
        :param file:
        :param value:
        :return: returns value from IOCTL call if any
        """
        if ioctl_name not in self.ioctls:
            raise AlphaLCDIOError(f"IOCTL {ioctl_name} not found")

        if not kwargs and self.ioctls[ioctl_name].direction in (LCDDirection.WRITEREAD,
                                                                LCDDirection.WRITE):
            raise AlphaLCDIOError(f"IOCTL {ioctl_name} requires a value")
        return self.ioctls[ioctl_name](file, **kwargs)

    def __str__(self):
        """
        String representation of all IOCTLs registered.
        :return:
        """
        return "\n".join([str(i) for i in self.ioctls.values()])

    @staticmethod
    def __cmdparse(cmd):
        """
        Splits IOCTL value into IOCTL number, IOCTL base, IOCTL length and IOCTL direction.
        :param cmd:
        :return:
        """
        return cmd & 0xff, (cmd >> 8) & 0xff, (cmd >> 16) & 0xff, (cmd >> 30) & 0x03


class AlphaLCD:
    def __init__(self, bus: int = None, address: int = None):
        """
        AlphaLCD class provides a context manager for LCDPrint class.
        :param bus: I2C bus number, if not specified, will be read from metadata file
        :param address: Device address, if not specified, will be read from metadata file
        """
        self.file = None
        self.name = None
        self.closed = True
        self.mode = "r+"
        self.rows = 0
        self.columns = 0
        self.calculated_buffer_length = 0
        self.bus = bus
        self.address = address

        try:
            with open(META_FILE_PATH) as meta:
                p = yaml.safe_load(meta)
                self.columns = p["metadata"]["columns"]
                self.rows = p["metadata"]["rows"]
                self.ioctl_manager = IOCTLManager(p["metadata"]["ioctls"])
                self.buffer_length = p["metadata"]["raw_data-len"]
                if not (bus or address):
                    self.bus = p["metadata"]["busno"]
                    self.address = p["metadata"]["reg"]
                self.calculated_buffer_length = self.columns * self.rows
        except FileNotFoundError:
            raise AlphaLCDInitError(f"Metadata file not found at {META_FILE_PATH} (is the lcdi2c module loaded?)")

        try:
            with open(f"/sys/bus/i2c/devices/{self.bus}-{self.address:04x}/name") as f:
                self.device_path = f"/dev/{f.read().strip()}"
        except FileNotFoundError:
            raise AlphaLCDInitError(f"Device not found at bus {self.bus} address {self.address}")

    def __call__(self, ioctl_name: str, **kwargs):
        return self.ioctl_manager(ioctl_name, self.file, **kwargs)

    def __enter__(self):
        self.open(file=self.device_path, mode=self.mode)
        return self

    def __exit__(self, type, value, traceback):
        self.close()
        return isinstance(value, TypeError)

    def open(self):
        self.file = open(file=self.device_path, mode=self.mode)
        if self.file:
            self.closed = False
        return self.file

    def close(self):
        if not self.closed:
            if self.file:
                self.flush()
                self.file.close()
        self.closed = True

    def write(self, data):
        return 0 if self.closed else self.file.write(data)

    def flush(self):
        return 0 if self.closed else self.file.flush()


class LCDCursor:
    """
    LCD cursor class provides a simple interface to get and set cursor position.
    """
    def __init__(self, lcd: AlphaLCD):
        self.lcd = lcd
        self._col = 0
        self._row = 0

    @property
    def column(self):
        return self._col

    @column.setter
    def column(self, col: int):
        self.set(col, None)

    @property
    def row(self):
        return self._row

    @row.setter
    def row(self, row: int):
        self.set(None, row)

    def get(self):
        ret = self.lcd(LCDCommand.GET_POSITION.value)
        self._col, self._row = ret.column, ret.row
        return self._col, self._row

    def set(self, column: int, row: int):
        if (column is None) and (row is None):
            return self.get()

        if column is not None:
            if (column < 0) or (column > self.lcd.columns):
                column = self.lcd.columns - 1
            self._col = column

        if row is not None:
            if (row < 0) or (row > self.lcd.rows):
                row = self.lcd.rows - 1
            self._row = row

        self.lcd(LCDCommand.SET_POSITION.value, column=self._col, row=self._row)


class LCDPrint:
    """
    LCDPrint class provides a context manager for LCDPrint class.
    All operations are performed on opened file descriptor, so LCDPrint is also a context manager that can be called
    with "with" statement.
    """
    def __init__(self, lcd: AlphaLCD):
        self.lcd = lcd
        self.position = LCDCursor(self.lcd)

    def reset(self) -> None:
        """
        Reset the LCD to initial state.
        :return:
        """
        self.lcd(LCDCommand.RESET.value)

    def clear(self) -> None:
        """
        Clear the LCD.
        :return:
        """
        self.lcd(LCDCommand.CLEAR.value)

    def home(self) -> None:
        """
        Move the cursor to the home position (0, 0).
        :return:
        """
        self.lcd(LCDCommand.HOME.value)

    def print(self, string: str, col: int = None, row: int = None) -> Tuple[int, int]:
        """
        Print string at specified position or at the current position.
        :param string:
        :param col: if None is given the current column is used
        :param row: if None is given the current row is used
        :return: current cursor position (a tuple), marking the end of the text printed
        """
        self.position.set(column=col, row=row)
        self.lcd.write(string)
        self.lcd.flush()
        self.position.get()

    def get_position(self) -> Tuple[int, int]:
        """
        Get the current cursor position.
        :return:
        """
        return self.position.get()

    def set_position(self, col: int, row: int):
        """
        Set the current cursor position.
        :param col:
        :param row:
        :return:
        """
        old_col, old_row = self.position.get()
        self.position.set(col, row)
        return old_col, old_row

    @property
    def blink(self) -> bool:
        """
        Get the blink state.
        :return: bool
        """
        return True if self.lcd(LCDCommand.GET_BLINK.value) else False

    @blink.setter
    def blink(self, blink: bool) -> None:
        """
        Set the blink state.
        :param blink:
        :return: None
        """
        self.lcd(LCDCommand.SET_BLINK.value, value=1 if blink else 0)

    @property
    def cursor(self) -> bool:
        """
        Get the show cursor state.
        :return: bool
        """
        return True if self.lcd(LCDCommand.GET_CURSOR.value) else False

    @cursor.setter
    def cursor(self, cursor: bool) -> None:
        """
        Set the show cursor state.
        :param cursor:
        :return: None
        """
        self.lcd(LCDCommand.SET_CURSOR.value, value=1 if cursor else 0)

    def scroll(self, direction: bool) -> None:
        """
        Set the scroll direction and scrolls the screen horizontally by one character each call.
        :param direction: True to scroll left, False to scroll right
        :return: None
        """
        self.lcd(LCDCommand.SCROLL_HZ.value, value=1 if direction else 0)

    def scroll_vert(self, line: str, direction: bool) -> None:
        """
        Set the scroll direction and scrolls the screen vertically by one row each call.
        :param line: Replace free line with this string
        :param direction: True to scroll up (last line become empty), False to scroll down (first line become empty)
        :return: None
        """
        self.lcd(LCDCommand.SCROLL_VERT.value, line=line, direction=1 if direction else 0)


    @property
    def backlight(self) -> bool:
        """
        Get the backlight state. False if backlight is off, otherwise True.
        :return:
        """
        return True if self.lcd(LCDCommand.GET_BACKLIGHT.value) else False

    @backlight.setter
    def backlight(self, backlight: bool) -> None:
        """
        Set the backlight state.
        :param backlight:
        :return:
        """
        self.lcd(LCDCommand.SET_BACKLIGHT.value, value=1 if backlight else 0)

    def get_char(self, col: int = None, row: int = None) -> int:
        """
        Get the character at the specified position or at the current position.
        :param col: if None is given the current column is used
        :param row: if None is given the current row is used
        :return: int - ascii value of the character
        """
        self.position.set(col, row)
        return self.lcd(LCDCommand.GET_CHAR.value)

    def set_char(self, char: int, col: int = None, row: int = None) -> None:
        """
        Set the character at the specified position or at the current position.
        :param char: ord(char) - ascii value of the character
        :param col: if None is given the current column is used
        :param row: if None is given the current row is used
        :return: None
        """
        self.position.set(col, row)
        self.lcd(LCDCommand.SET_CHAR.value, value=char)

    def get_line(self, row: int) -> str:
        """
        Get the whole line at the specified row.
        :param row: row number or None to use the current row
        :return: str - line trimmed to the LCD width
        """
        self.position.set(None, row)
        ret = self.lcd(LCDCommand.GET_LINE.value)
        return ret.line.decode("ascii").strip()

    def set_line(self, string: str, row: int) -> None:
        """
        Set the whole line at the specified row.
        :param string:
        :param row:
        :return:
        """
        self.position.set(None, row)
        self.lcd(LCDCommand.SET_LINE.value, line=string)

    def get_custom_char_bin(self, char: int) -> list:
        """
        Get the custom character data.
        :param char: number of custom character (0-7)
        :return: a list of 8 bytes representing bitmap of the custom character
        """
        if char < 0 or char > 7:
            raise ValueError("Custom character number must be between 0 and 7")
        return self.lcd(LCDCommand.GET_CUSTOMCHAR.value, index=char)

    def set_custom_char_bin(self, char: int, data: list) -> None:
        """
        Set the custom character data.
        :param char: Number of custom character (0-7) to set the bitmap for
        :param data: a list of 8 bytes representing bitmap of the custom character
        :return:
        """
        if char < 0 or char > 7:
            raise ValueError("Custom character number must be between 0 and 7")
        if len(data) != 8:
            raise ValueError("Custom character data must be 8 bytes long")
        self.lcd(LCDCommand.SET_CUSTOMCHAR.value, index=char, data=data)

    def get_buffer(self) -> str:
        """
        Get the whole LCD raw_data and return it as a string.
        :return: str - raw_data trimmed to the LCD width and height
        """
        return self.lcd(LCDCommand.GET_BUFFER.value).buffer.decode("ascii")

    def set_buffer(self, data: str) -> None:
        """
        Set the whole LCD raw_data. Buffer will be trimmed to the LCD width and height.
        :param data:
        :return:
        """
        self.lcd(LCDCommand.SET_BUFFER.value, buffer=data)

    def __enter__(self) -> "LCDPrint":
        self.lcd.open()
        return self

    def __exit__(self, exc_type, exc_value, exc_traceback) -> bool:
        self.lcd.close()
        return isinstance(exc_value, TypeError)


if __name__ == "__main__":
    import time
    lcd = AlphaLCD()
    with LCDPrint(lcd) as f:
        try:
            print(lcd.ioctl_manager)
            f.clear()
            f.backlight = True
            f.cursor = True
            f.blink = True

            f.print("0123456789ABCDEFGHIJ", 0, 0)
            print(f"LINE 0:\"{f.get_line(0)}\"")
            f.set_line("FEEDFACEDEADBEEF", 1)

            f.print("Foo Bar!", 0, 2)
            print(f"LINE 2:\"{f.get_line(2)}\"")
            print(f"LINE 1:\"{f.get_line(1)}\"")
            print(f"CURSOR: {f.get_position()}")
            print(f"Buffer:\n\t\"{f.get_buffer()}\"")

            f.clear()
            f.set_buffer("~~~~~~~~~~~~~~~~fedcba9876543210----------------****************")
            print(f"Buffer:\n\t\"{f.get_buffer()}\"")
            print(f"CURSOR: {f.get_position()}")
            f.set_position(2, 1)
            print(f"LINE 1: {f.get_line(1)}")
            print(f"CURSOR: {f.get_position()}")
            for i in range(50):
                f.scroll_vert(f"Line {i}", True)
            for i in range(50):
                f.scroll_vert(f"Line {i}", False)
        except Exception as e:
            print(e)


