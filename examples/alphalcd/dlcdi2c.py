import os, fcntl, array, struct

RDWR = 3
WRITE = 1
READ = 2
SIZES = {1 : 'c', 2 : 'h', 4 : 'l', 8 : 'd'}

class LcdI2C(object):
 
  def __init__(self, name, mode):
    self.ioctls = {}
    self.name = name
    self.mode = mode
    self.closed = True
    self.softspace = 0
    f = os.open("/sys/class/alphalcd/lcdi2c/meta", os.O_RDONLY)
    if f:
      meta = os.read(f, 512).rstrip().split("\n")
      os.close(f)
      self.columns, self.rows = [int(meta[v].split(":")[1]) for v in range(2,0,-1)]
      self.bufferlength = self.columns * self.rows
      try:
	iioc = meta.index("IOCTLS:") + 1
      except ValueError as e:
	print("No IOCTLS section in meta file")
	raise e      
      self.ioctls = dict(k.split("=") for k in [s.lstrip() for s in meta[iioc:]])
      
    else:
      print("Unable to open meta file for driver")
      
  def __cmdparse(self, cmd):
    return cmd & 0xff, (cmd >> 8) & 0xff, (cmd >> 16) & 0xff, (cmd >> 30) & 0x03
  
  def __sizetype(self, size):
    return SIZES[size]
      
  def __setattr__(self, name, value):
    if name == "ioctls" and name not in self.__dict__:
      object.__setattr__(self, name, value)
      return
    
    if name in self.ioctls.keys():
      cmd = int(self.ioctls[name], base=16)
      nr, typem, size, direction = self.__cmdparse(cmd)
      
      if (direction & WRITE) == 0:
	raise AttributeError("IOCTL {0} can only be READ".format(name))

      
      s = array.array('b')
      if (nr & 2):
	s.extend([ord(i) for i in value])
      else:
	s.extend(value)
      	
      
      result = fcntl.ioctl(self.file, cmd, s)
      return result;
	
      
    else:
      object.__setattr__(self, name, value)
  
  def __getattr__(self, name):
    if name not in self.__dict__:
      if name not in self.ioctls.keys():
	raise AttributeError
      
    if name in self.ioctls.keys():
      cmd = int(self.ioctls[name], base=16)
      nr, typem, size, direction = self.__cmdparse(cmd)
      
      if (direction & READ) == 0:
	raise AttributeError("IOCTL {0} can only be WRITE".format(name))
      
      buffer = struct.pack('bb', 0, 0)
      result = struct.unpack('bb', fcntl.ioctl(self.file, cmd, buffer))
      return result;

  
  def open(self, device, mode):
    self.name = device
    self.mode = mode
    self.file =  open(self.name, self.mode)
    if file:
      self.closed = False
      
  def __enter__(self):
    self.file =  open(self.name, self.mode)
    if file:
      self.closed = False
    return self.file

  def __exit__(self, type, value, traceback):
    if not self.closed:
      if self.file:
	self.file.close()
	self.closed = True
    return isinstance(value, TypeError)
  
  def ioctl(self, cmd, arg):
    if not self.closed:
      return fcntl.ioctl(self.file, cmd, arg, 0)
  
if __name__ == "__main__":
  lcd = LcdI2C("/dev/lcdi2c", "rwb+")
  
  with lcd as f:
    print(f.read(80))
    lcd.SETBACKLIGHT = '0'
    lcd.RESET = '1'
    lcd.HOME = '1'
    lcd.SETPOSITION = (0,0)
    lcd.SETBACKLIGHT = '1'
    lcd.SETCURSOR = '0'
    lcd.SETBLINK = '0'
    c = random.sample(range(lcd.columns * lcd.rows), lcd.columns * lcd.rows)
    for i in c:
      col = i % lcd.columns
      row = i / lcd.columns
      lcd.SETPOSITION = (col, row)
      lcd.SETCHAR = '*'
      time.sleep(0.02)
    

