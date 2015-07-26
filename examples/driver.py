import time, random
from alphalcd.dlcdi2c import LcdI2C
  
if __name__ == "__main__":
  lcd = LcdI2C("/dev/lcdi2c", "rwb+")
  
  with lcd as f:
    print(f.read(80))
    lcd.SETBACKLIGHT = '0'
    lcd.RESET = '1'
    lcd.CLEAR = '1'
    lcd.HOME = '1'
    lcd.SETPOSITION = (0,0)
    lcd.SETBACKLIGHT = '1'
    lcd.SETCURSOR = '0'
    lcd.SETBLINK = '0'
    fonts = ( (6, 0x0,0x0,0x0,0x4,0x4,0x0,0x0,0x0),
	      (6, 0x0,0x0,0xe,0xa,0xe,0x4,0x0,0x0),
	      (6, 0x0,0x4,0xa,0xa,0x4,0x4,0x4,0x0),
	      (6, 0x0,0xe,0x11,0x11,0xe,0x4,0x4,0x4),
	      (6, 0x0,0xe,0x11,0x11,0xe,0x4,0xa,0xa),
	      (6, 0x0,0xe,0x11,0x11,0xe,0x4,0xa,0x11),
	      )
    
    #f.seek(1);
    f.write("Hello\nNew World!")
    f.flush()
    time.sleep(10)
    random.seed(int(time.time()))
    
    
    while True:
      lcd.SETCUSTOMCHAR = fonts[0]
      c = random.sample(range(lcd.columns * lcd.rows), 20)
      lcd.CLEAR = '1'
      for i in c:
	col = i % lcd.columns
	row = i / lcd.columns
	lcd.SETPOSITION = (col, row)
	lcd.SETCHAR = chr(6)
	time.sleep(0.02)
      for i in range(2,5):
	lcd.SETCUSTOMCHAR = fonts[i]
	time.sleep(0.5)
      for i in range(20):
	lcd.SETCUSTOMCHAR = fonts[i % 2 + 4]
	lcd.SCROLLHZ = '1'
	time.sleep(0.1)
    

