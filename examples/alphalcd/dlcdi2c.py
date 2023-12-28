import array
import fcntl
import struct

import yaml

GET_CHAR = "GETCHAR"
SET_CHAR = "SETCHAR"
RESET = "RESET"
HOME = "HOME"
GET_BACKLIGHT = "GETBACKLIGHT"
SET_BACKLIGHT = "SETBACKLIGHT"
GET_CURSOR = "GETCURSOR"
SET_CURSOR = "SETCURSOR"
GET_BLINK = "GETBLINK"
SET_BLINK = "SETBLINK"
SCROLL_HZ = "SCROLLHZ"
GET_CUSTOMCHAR = "GETCUSTOMCHAR"
SET_CUSTOMCHAR = "SETCUSTOMCHAR"
CLEAR = "CLEAR"
SET_POSITION = "SETPOSITION"
GET_POSITION = "GETPOSITION"

META_FILE_PATH = "/sys/class/alphalcd/lcdi2c/meta"

RDWR = 3
WRITE = 1
READ = 2


class LcdI2CInitError(Exception):
    pass


class LcdI2C:
    def __init__(self, bus=None, address=None):
        self.file = None
        self.name = None
        self.closed = True
        self.mode = "r+"
        self.ioctls = {}
        self.rows = 0
        self.columns = 0
        self.bufferlength = 0
        self.bus = bus
        self.address = address

        try:
            with open(META_FILE_PATH) as meta:
                p = yaml.safe_load(meta)
                self.columns = p["metadata"]["columns"]
                self.rows = p["metadata"]["rows"]
                self.bufferlength = self.columns * self.rows
                self.ioctls = p["metadata"]["ioctls"]
                if not (bus or address):
                    self.bus = p["metadata"]["busno"]
                    self.address = p["metadata"]["reg"]
        except FileNotFoundError:
            raise LcdI2CInitError(f"Metadata file not found at {META_FILE_PATH}")

        try:
            with open(f"/sys/bus/i2c/devices/{self.bus}-{self.address:04x}/name") as f:
                self.device_path = f"/dev/{f.read().strip()}"
        except FileNotFoundError:
            raise LcdI2CInitError(f"Device not found at bus {self.bus} address {self.address}")

    @staticmethod
    def __cmdparse(cmd):
        return cmd & 0xff, (cmd >> 8) & 0xff, (cmd >> 16) & 0xff, (cmd >> 30) & 0x03

    def io_write(self, ioctl_name: str, value):
        cmd = int(self.ioctls[ioctl_name])
        nr, __, ___, direction = self.__cmdparse(cmd)

        if not (direction & WRITE):
            raise AttributeError("IOCTL {0} can only be READ".format(ioctl_name))

        s = array.array('B')
        if nr & 2:
            s.extend([ord(i) for i in value])
        else:
            s.extend(value)

        fcntl.ioctl(self.file, cmd, s) if not self.closed else -1

    def io_read(self, ioctl_name):
        cmd = int(self.ioctls[ioctl_name])
        _, __, ___, direction = self.__cmdparse(cmd)

        if not (direction & READ):
            raise AttributeError("IOCTL {0} can only be WRITTEN".format(ioctl_name))

        buffer = struct.pack('9B', 0, 0, 0, 0, 0, 0, 0, 0, 0)
        return struct.unpack('9B', fcntl.ioctl(self.file, cmd, buffer)) if not self.closed else -1

    def __enter__(self):
        self.file = open(file=self.device_path, mode=self.mode)
        if self.file:
            self.closed = False
        return self.file

    def __exit__(self, type, value, traceback):
        if not self.closed:
            if self.file:
                self.file.close()
        self.closed = True
        return isinstance(value, TypeError)

    def open(self, device, mode):
        self.name = device
        self.mode = mode
        self.file = open(file=self.name, mode=self.mode)
        if self.file:
            self.closed = False

    def write(self, data):
        return self.file.write(data) if not self.closed else 0

    def flush(self):
        return self.file.flush() if not self.closed else 0
