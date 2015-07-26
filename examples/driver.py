import time, random
from alphalcd.dlcdi2c import LcdI2C
  
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
    

