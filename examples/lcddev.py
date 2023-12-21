#!/usr/bin/env python3

import time, random, sys, threading, signal
from os import lstat
from alphalcd.dlcdi2c import (
	LcdI2C, 
  GET_POSITION, 
  SET_POSITION,
  GET_CUSTOMCHAR,
  SET_CUSTOMCHAR,
  GET_BACKLIGHT,
  SET_BACKLIGHT,
  GET_BLINK,
  SET_BLINK,
  GET_CHAR,
  SET_CHAR,
  GET_CURSOR,
  SET_CURSOR,
  RESET,
  HOME,
  CLEAR,
  SCROLL_HZ
  )

I2C_BUSNO = 3
I2C_DEVICE_ADDRESS = 0x27

class lcdThread(threading.Thread):
  '''
    Base class of all threads
  '''
  threadLock = threading.RLock()
  quitEvent = threading.Event()
  def __init__(self, name, lcd, col, row):
    super(lcdThread, self).__init__(name=f"{name}-{col}.{row}")
    self.lcd = lcd 
    self.col = col
    self.row = row
    self.buff = range(9)
    
  def write(self, string):
    '''
      Write string at position given by col, row in class constructor
      only one thread is able to write to LCD at time, other threads
      will wait
    '''
    with lcdThread.threadLock:
      self.buff = self.lcd.ioread(GET_POSITION)
      self.lcd.iowrite(SET_POSITION, (self.col, self.row))
      self.lcd.write(string)
      self.lcd.flush()
      self.lcd.iowrite(SET_POSITION, self.buff)
    
  
class timeThread(lcdThread):
  '''
    Prints current local time
  '''
  def __init__(self, lcd, col, row):
    super(timeThread, self).__init__("TimeThread", lcd, col, row)
  
  def run(self):
    while(1):
      self.write("{0}".format(time.strftime("%H:%M:%S")))
      if lcdThread.quitEvent.is_set():
        return
      time.sleep(1)
      
class heartbeatThread(lcdThread):
  '''
    System heart beat as animated icon
  '''
  def __init__(self, lcd, col, row):
    super(heartbeatThread, self).__init__("HbThread", lcd, col, row)
    self.heart = [ 
      (0x06, 0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0),
		  (0x06, 0x0,0x0,0x0,0x4,0x4,0x0,0x0,0x0),
		  (0x06, 0x0,0x0,0x0,0xe,0xe,0x4,0x0,0x0),
		  (0x06, 0x0,0x0,0xa,0x1f,0x1f,0xe,0x4,0x0),
      ]
    self.lcd.iowrite(SET_CUSTOMCHAR, self.heart[0])
  
  def run(self):
    self.write("\x06")
    while(1):
      for i in [0,1,2,3,2,3,2,1,0]:
        self.lcd.iowrite(SET_CUSTOMCHAR, self.heart[i])
        time.sleep(0.1)

      if lcdThread.quitEvent.is_set():
        return
      time.sleep(2)
      
class systemloadThread(lcdThread):
  '''
    Current system load. One thread per core
  '''
  def __init__(self, cpu, lcd, col, row, corecnt):
    super(systemloadThread, self).__init__("SlThread", lcd, col, row)
    self.cpu = cpu
    self.corecnt = corecnt
    self.icon = [0x05 - self.cpu, 0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1f]
    self.oldload = int(round(getCoreLoad(0.3, self.corecnt)[self.cpu] * 7))
    
  def run(self):
    self.write(chr(0x05 - self.cpu))
    while(1):
      load = 7 - int(round(getCoreLoad(0.3, self.corecnt)[self.cpu] * 7))
      for i in range(7):
        if i >= load:
          self.icon[i + 1] = 0x1f
        else:
          self.icon[i + 1] = 0x00
          self.lcd.iowrite(SET_CUSTOMCHAR, self.icon)
          self.oldload = load
          time.sleep(0.6)
      if lcdThread.quitEvent.is_set():
        return
      
class systemLoad(object):
  '''
    System load as vertical strip from 1 to 8. It calculates average load of 
    one CPU core in range from 0.0 to 1.0 at given period of time. It will
    instantiate one thread per each core, so it's a little bit overdoing,
    but it's just for the show.
  '''
  def __init__(self, lcd, col, row):
    self.cpus = len(getCPUUsage())
    self.buffer = range(9)
    self.col = col
    self.row = row
    self.lcd = lcd
    self.threads = []
    self.string = ":"
    self.write(self.string)
    for i in range(1, self.cpus):
      self.threads.append(systemloadThread(i, self.lcd, len(self.string) + self.col + (i - 1), self.row, self.cpus))
      
  def start(self):
    lcd.iowrite(SET_POSITION, (self.col, self.row))
    for i in range(self.cpus - 1):
      self.threads[i].start()
      
  def write(self, string):
    with lcdThread.threadLock:
      self.buffer = self.lcd.ioread(GET_POSITION)
      self.lcd.iowrite(SET_POSITION, (self.col, self.row))
      self.lcd.write(string)
      self.lcd.flush()
      self.lcd.iowrite(SET_POSITION, self.buffer)
      
      
class scrollThread(lcdThread):
  '''
    Fancy scrolling thread, just scroll content of LCD back and forth
  '''
  def __init__(self, lcd):
    super(scrollThread, self).__init__("ScrollThread", lcd, 0, 0)
    self.scrollby = lcd.columns - 9

  def run(self):
    if self.scrollby > 3:
      while(1):
        for i in range(self.scrollby):
          self.lcd.iowrite(SCROLL_HZ, '1')
          time.sleep(0.3)
        time.sleep(3)
        for i in range(self.scrollby):
          self.lcd.iowrite(SCROLL_HZ, '0')
          time.sleep(0.3)
        time.sleep(3)
        if lcdThread.quitEvent.is_set():
          return

    
def threadsstop(signum, frame):
  print(" waiting for threads to finish...")
  lcdThread.quitEvent.set()
  time.sleep(2)
  sys.exit(0)
  
def getCPUUsage():
  '''
    Get first three lines that starts from 'cpu' string and map values for each line
    as list of integers
  '''
  statf = open("/proc/stat", "r")
  cpus = [' '.join(i.split()).split(" ")[1:] for i in statf.readlines() if i[0:3]=='cpu']
  statf.close()
  return [map(int, filter(None, cpu)) for cpu in cpus[:3]]

def getCoreDelta(interval, cores):
  '''
    Calculates difference between two subsequent measurements of time after given
    period.
  '''
  usage1 = getCPUUsage()
  time.sleep(interval)
  usage2 = getCPUUsage()
  return [[(t2 - t1) for t1, t2 in zip(usage1[core], usage2[core])] for core in range(cores)]

def getCoreLoad(interval, cores):
  '''
    Final function to calculate actual load of a core in terms between 0 and 1
  '''
  deltas = getCoreDelta(interval, cores)
  idles =  [float(dt[3]) for dt in deltas]
  totals = [(1 - idles[i]/sum(dt)) for i,dt in enumerate(deltas)]
  return totals
  

if __name__ == "__main__":
 
  lcd = LcdI2C(I2C_BUSNO, I2C_DEVICE_ADDRESS)
  signal.signal(signal.SIGINT, threadsstop)
  with lcd as f:
    lcd.iowrite(SET_BACKLIGHT, '0')
    lcd.iowrite(RESET,  '1')
    lcd.iowrite(CLEAR, '1')
    lcd.iowrite(HOME, '1')
    lcd.iowrite(SET_BACKLIGHT, '1')
    lcd.iowrite(SET_CURSOR, '0')
    lcd.iowrite(SET_BLINK, '0')
    thread1 = timeThread(lcd, 0, 0)
    thread2 = heartbeatThread(lcd, 12, 1)
    thread3 = scrollThread(lcd)
    thread4 = systemLoad(lcd, 0, 1)
        
    thread4.start()
    thread3.start()
    thread2.start()
    thread1.start()
    while(1):
      time.sleep(5)

    
