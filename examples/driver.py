import time, random, sys
from alphalcd.dlcdi2c import LcdI2C

FONTS = ( (6, 0x0,0x0,0x0,0x4,0x4,0x0,0x0,0x0),
	  (6, 0x0,0x0,0xe,0xa,0xe,0x4,0x0,0x0),
	  (6, 0x0,0x4,0xa,0xa,0x4,0x4,0x4,0x0),
	  (6, 0x0,0xe,0x11,0x11,0xe,0x4,0x4,0x4),
	  (6, 0x0,0xe,0x11,0x11,0xe,0x4,0xa,0xa),
	  (6, 0x0,0xe,0x11,0x11,0xe,0x4,0xa,0x11),
	  )

  
if __name__ == "__main__":
  lcd = LcdI2C(1, 0x27)
  
  with lcd as f:
    print(f.read(20))
    lcd.SETBACKLIGHT = '0'
    lcd.RESET = '1'
    lcd.CLEAR = '1'
    lcd.HOME = '1'
    lcd.SETPOSITION = (0,0)
    lcd.SETBACKLIGHT = '1'
    lcd.SETCURSOR = '0'
    lcd.SETBLINK = '0'
    f.write("Hello\nNew World!")
    
    f.flush()
    time.sleep(10)
    random.seed(int(time.time()))
    
    
    while True:
      lcd.SETCUSTOMCHAR = FONTS[0]
      c = random.sample(range(lcd.columns * lcd.rows), 20)
      lcd.CLEAR = '1'
      for i in c:
	col = i % lcd.columns
	row = i / lcd.columns
	lcd.SETPOSITION = (col, row)
	lcd.SETCHAR = chr(6)
	time.sleep(0.02)
      for i in range(2,5):
	lcd.SETCUSTOMCHAR = FONTS[i]
	time.sleep(0.5)
      for i in range(20):
	lcd.SETCUSTOMCHAR = FONTS[i % 2 + 4]
	lcd.SCROLLHZ = '1'
	time.sleep(0.2)
      for i in range(20):
	lcd.SETCUSTOMCHAR = FONTS[i % 2 + 4]
	lcd.SCROLLHZ = '0'
	time.sleep(0.2)
    

