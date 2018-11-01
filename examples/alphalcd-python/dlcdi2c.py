import os, fcntl, array, struct
import errno

RDWR = 3
WRITE = 1
READ = 2

class LcdI2C(object):
  '''
    Class for communication with the device. Will try to configure itself based on
    /sys/class/alphalcd/lcdi2c/meta file. All ioctls will be accessible through
    class attributes
  '''
  def __init__(self, bus, address):
    '''
    '''
    self.ioctls = {}
    self.mode = 'rwb+'
    self.closed = True

    try:
      f = open("/sys/bus/i2c/devices/{0}-{1:04x}/name".format(bus, address))
      self.name = "/dev/{0}".format(f.read().strip())
      f.close()
    except OSError as e:
      raise OSError("Cannot figure out device path. Is lcdi2c module loaded?")
    
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
       
  def __setattr__(self, name, value):
    if name == "ioctls" and name not in self.__dict__:
      object.__setattr__(self, name, value)
      return
    
    if name in self.ioctls.keys():
      cmd = int(self.ioctls[name], base=16)
      nr, typem, size, direction = self.__cmdparse(cmd)
      
      if (direction & WRITE) == 0:
	raise AttributeError("IOCTL {0} can only be READ".format(name))

      s = array.array('B')
      if (nr & 2):
	s.extend([ord(i) for i in value])
      else:
	s.extend(value)
      	
      
      result = fcntl.ioctl(self.file, cmd, s) if not self.closed else -1
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
	raise AttributeError("IOCTL {0} can only be WRITTEN".format(name))
      
      buffer = struct.pack('9B', 0, 0, 0, 0, 0, 0, 0, 0, 0)
      result = struct.unpack('9B', fcntl.ioctl(self.file, cmd, buffer)) if not self.closed else -1
      return result;

  
  def open(self, device, mode):
    self.name = device
    self.mode = mode
    self.file =  open(self.name, self.mode)
    if file:
      self.closed = False
      
  def write(self, data):
    return self.file.write(data) if not self.closed else 0
  
  def flush(self):
    return self.file.flush() if not self.closed else 0
      
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

    

