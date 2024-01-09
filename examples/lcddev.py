#!/usr/bin/env python3

import signal
import sys
import threading
import time

from alphalcd.dlcdi2c import (
    LcdI2C,
    LcdI2CInitError,
    GET_POSITION,
    SET_POSITION,
    SET_CUSTOMCHAR,
    SET_BACKLIGHT,
    SET_BLINK,
    SET_CURSOR,
    RESET,
    HOME,
    CLEAR,
    SCROLL_HZ
)

I2C_BUS_NO = 3
I2C_DEVICE_ADDRESS = 0x27


def get_cpu_usage():
    """
    Get first three lines begin with 'cpu' string and map values for each line
    as list of integers
    """

    with open("/proc/stat", "r") as statf:
        cpus = [' '.join(i.split()).split(" ")[1:] for i in statf.readlines() if i[0:3] == 'cpu']

    return [map(int, filter(None, cpu)) for cpu in cpus[1:6]]


def get_core_delta(interval, cores):
    """
    Calculates difference between two subsequent measurements of time after given
    period.
    """

    usage1 = get_cpu_usage()
    time.sleep(interval)
    usage2 = get_cpu_usage()
    return [[(t2 - t1) for t1, t2 in zip(usage1[core], usage2[core])] for core in range(cores)]


def get_core_load(interval, cores):
    """
    Final function to calculate actual load of a core in range between 0 and 1
    """

    deltas = get_core_delta(interval, cores)
    idles = [float(dt[3]) for dt in deltas]
    totals = [(1 - idles[i] / sum(dt)) for i, dt in enumerate(deltas)]
    return totals


class LcdThread(threading.Thread):
    """
    Base class of all threads
    """
    threadLock = threading.RLock()
    quitEvent = threading.Event()
    barrier = threading.Barrier(2 + len(get_cpu_usage()))

    def __init__(self, name: str, lcd: LcdI2C, col: int, row: int):
        super(LcdThread, self).__init__(name=f"{name}-{col}.{row}")
        self.lcd = lcd
        self.col = col
        self.row = row
        self.buff = range(9)

    def write(self, string):
        """
        Write string at position given by col, row in class constructor
        only one thread is able to write to LCD at time, other threads
        will wait
        """
        with LcdThread.threadLock:
            self.buff = self.lcd.io_read(GET_POSITION)
            self.lcd.io_write(SET_POSITION, (self.col, self.row))
            self.lcd.write(string)
            self.lcd.flush()
            self.lcd.io_write(SET_POSITION, self.buff)

    def finish(self):
        print(f"{self.name}-finished")
        self.barrier.wait()


class TimeThread(LcdThread):
    """
    Prints current local time
    """

    def __init__(self, lcd: LcdI2C, col: int, row: int):
        super(TimeThread, self).__init__("TimeThread", lcd, col, row)

    def run(self):
        while LcdThread.quitEvent.is_set() is False:
            self.write("{0}".format(time.strftime("%H:%M:%S")))
            LcdThread.quitEvent.wait(timeout=1)
        self.finish()


class HeartBeatThread(LcdThread):
    """
    System heart beat as an animated icon
    """

    def __init__(self, lcd, col, row):
        super(HeartBeatThread, self).__init__("HbtThread", lcd, col, row)
        self.heart = [
            (0x06, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0),
            (0x06, 0x0, 0x0, 0x0, 0x4, 0x4, 0x0, 0x0, 0x0),
            (0x06, 0x0, 0x0, 0x0, 0xe, 0xe, 0x4, 0x0, 0x0),
            (0x06, 0x0, 0x0, 0xa, 0x1f, 0x1f, 0xe, 0x4, 0x0), ]
        self.lcd.io_write(SET_CUSTOMCHAR, self.heart[0])

    def run(self):
        self.write("\x06")
        while LcdThread.quitEvent.is_set() is False:
            for i in [0, 1, 2, 3, 2, 3, 2, 1, 0]:
                self.lcd.io_write(SET_CUSTOMCHAR, self.heart[i])
                LcdThread.quitEvent.wait(timeout=0.1)
            LcdThread.quitEvent.wait(timeout=2)
        self.finish()


class SystemLoadThread(LcdThread):
    """
    Current system load. One thread per core
    """

    def __init__(self, cpu, lcd, col, row, corecnt):
        super(SystemLoadThread, self).__init__(f"SlThread:{cpu}", lcd, col, row)
        self.cpu = cpu
        self.corecnt = corecnt
        self.icon = [0x05 - self.cpu, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1f]
        self.old_load = int(round(get_core_load(0.3, self.corecnt)[self.cpu] * 7))

    def run(self):
        self.write(chr(0x05 - self.cpu))
        while LcdThread.quitEvent.is_set() is False:
            load = 7 - int(round(get_core_load(0.3, self.corecnt)[self.cpu] * 7))
            for i in range(7):
                if i >= load:
                    self.icon[i + 1] = 0x1f
                else:
                    self.icon[i + 1] = 0x00
                    self.lcd.io_write(SET_CUSTOMCHAR, self.icon)
                    self.old_load = load
                    LcdThread.quitEvent.wait(timeout=0.6)
        self.finish()


class SystemLoad:
    """
    System load as vertical strip from 1 to 8. It calculates average load of
    one CPU core in range from 0.0 to 1.0 at given period of time. It will
    instantiate one thread per each core, so it's a little bit overdoing,
    but it's just for the show.
    """

    def __init__(self, lcd, col, row):
        self.cores = len(get_cpu_usage())
        self.buffer = range(9)
        self.col = col
        self.row = row
        self.lcd = lcd
        self.threads = []
        self.string = "CPU:"
        self.write(self.string)
        for i in range(1, self.cores):
            self.threads.append(
                SystemLoadThread(i, self.lcd, len(self.string) + self.col + (i - 1), self.row, self.cores))

    def start(self):
        lcd.io_write(SET_POSITION, (self.col, self.row))
        for i in range(self.cores - 1):
            self.threads[i].start()

    def write(self, string):
        with LcdThread.threadLock:
            self.buffer = self.lcd.io_read(GET_POSITION)
            self.lcd.io_write(SET_POSITION, (self.col, self.row))
            self.lcd.write(string)
            self.lcd.flush()
            self.lcd.io_write(SET_POSITION, self.buffer)


class ScrollingThread(LcdThread):
    """
    Fancy scrolling thread, just scroll content of LCD back and forth
    """

    def __init__(self, lcd):
        super(ScrollingThread, self).__init__("ScrollThread", lcd, 0, 0)
        self.scrollby = lcd.columns - 9

    def run(self):
        if self.scrollby > 3:
            while LcdThread.quitEvent.is_set() is False:
                for i in range(self.scrollby):
                    self.lcd.io_write(SCROLL_HZ, '1')
                    LcdThread.quitEvent.wait(timeout=0.3)
                LcdThread.quitEvent.wait(timeout=3)
                for i in range(self.scrollby):
                    self.lcd.io_write(SCROLL_HZ, '0')
                    LcdThread.quitEvent.wait(timeout=0.3)
                LcdThread.quitEvent.wait(timeout=3)
        self.finish()


def stop_threads(signum, frame):
    print(" waiting for threads to finish...")
    LcdThread.quitEvent.set()


if __name__ == "__main__":

    try:
        lcd = LcdI2C()
    except LcdI2CInitError as e:
        print(e)
        sys.exit(1)

    signal.signal(signal.SIGINT, stop_threads)
    with lcd as f:
        lcd.io_write(SET_BACKLIGHT, '0')
        lcd.io_write(RESET, '1')
        lcd.io_write(CLEAR, '1')
        lcd.io_write(HOME, '1')
        lcd.io_write(SET_BACKLIGHT, '1')
        lcd.io_write(SET_CURSOR, '0')
        lcd.io_write(SET_BLINK, '0')
        thread1 = TimeThread(lcd, 0, 0)
        thread2 = HeartBeatThread(lcd, 12, 1)
        # thread3 = ScrollingThread(lcd)
        thread4 = SystemLoad(lcd, 0, 1)

        thread4.start()
        # thread3.start()
        thread2.start()
        thread1.start()

        LcdThread.barrier.wait()

        lcd.io_write(SET_BACKLIGHT, '0')
        lcd.io_write(RESET, '1')
        lcd.io_write(CLEAR, '1')
        lcd.io_write(HOME, '1')
        lcd.io_write(SET_CURSOR, '0')
        lcd.io_write(SET_BLINK, '0')
