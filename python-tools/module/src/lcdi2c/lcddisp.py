#!/usr/bin/env python3
import argparse
import sys

from lcdi2c.alphalcd import (
    AlphaLCD,
    LCDPrint,
    AlphaLCDInitError,
    AlphaLCDIOError)


def scroll_vertical(lcd_print: LCDPrint):
    old_col, old_row = lcd_print.get_position()
    lcd = lcd_print.lcd
    for r in range(1, lcd.rows):
        line = lcd_print.get_line(r)
        lcd_print.print(line, 0, r - 1)
    lcd_print.set_position(old_col, old_row - 1)


def main():
    try:
        lcd = AlphaLCD()
    except AlphaLCDInitError as e:
        print(e)
        sys.exit(1)

    parser = argparse.ArgumentParser(
        prog="lcddisp",
        description="AlphaLCD CLI. $(prog)s allows to display text on the Alphanumeric LCD display from the shell. "
                    "The text can be provided as argument for --print option or read from stdin when called with \"-\" "
                    "argument. Or file read and display if last argument is a path to a file.",
        epilog="For more information about lcdi2c driver visit: https://github.com/lucidm/lcdi2c"
    )

    parser.add_argument(
        "--backlight",
        help="Turn on/off the backlight",
        default=False,
        action=argparse.BooleanOptionalAction,
    )

    parser.add_argument(
        "--blink",
        help="Turn on/off the cursor blink",
        default=False,
        action=argparse.BooleanOptionalAction,
    )

    parser.add_argument(
        "--cursor",
        help="Turn on/off the cursor",
        default=False,
        action=argparse.BooleanOptionalAction,
    )

    parser.add_argument(
        "--clear",
        help="Clear the LCD",
        action="store_true",
    )

    parser.add_argument(
        "--home",
        help="Move the cursor to the home position",
        action="store_true",
    )

    parser.add_argument(
        "--reset",
        help="Reset the LCD",
        action="store_true",
    )

    parser.add_argument("--scroll",
                        help="Scroll the LCD by one character to the left",
                        default=None,
                        type=str,
                        choices=["left", "right"],
                        action="store")

    parser.add_argument(
        "--get-position",
        help="Get the current cursor position as list of two ints [col, row]",
        action="store_true",
    )

    parser.add_argument(
        "--set-position",
        help="Set the current cursor position to list of two ints [col, row]",
        type=int,
        nargs=2,
        metavar=("col", "row"),
        action="store",
        default=[None, None],
        choices=[range(0, lcd.columns), range(0, lcd.rows)],
    )

    parser.add_argument(
        "--get-size",
        help="Get the size of the LCD as list of two ints [cols, rows]",
        action="store_true",
    )

    parser.add_argument(
        "--get-address",
        help="Get the bus number and address of the LCD as list of two hex ints [bus, address]",
        action="store_true",
    )

    parser.add_argument(
        "-p",
        "--print",
        help="Print text to the LCD",
        action="store",
        default="",
        type=str,
    )

    parser.add_argument(
        "--wrap",
        help="Wrap text to the next line if it exceeds the LCD width (default: truncate)",
        action="store_true",
        default=False,
    )

    parser.add_argument(
        "--autoscroll",
        help="Turn on/off autoscroll vertically (default: off). If enabled the text will scroll up when the last line"
             "is full",
        action=argparse.BooleanOptionalAction,
        default=False,
    )

    parser.add_argument('inputfile', nargs='?', type=argparse.FileType('r'))

    args, text = parser.parse_known_args()

    if args:
        with LCDPrint(lcd) as f:

            if args.get_size:
                print(f"{lcd.columns} {lcd.rows}")
                sys.exit(0)

            if args.get_address:
                print(f"0x{lcd.bus:02x} 0x{lcd.address:02x}")
                sys.exit(0)

            try:
                f.backlight = args.backlight
                f.blink = args.blink
                f.cursor = args.cursor

                if args.home:
                    f.home()

                if args.clear:
                    f.clear()

                if args.reset:
                    f.reset()

                if args.set_position:
                    f.position.set(args.set_position[0], args.set_position[1])

                if args.scroll == "left":
                    f.scroll = False

                if args.scroll == "right":
                    f.scroll = True

                if args.inputfile:
                    for line in args.inputfile:
                        f.print(line.strip()[0:lcd.columns - (0 if args.wrap else f.position.column)])
                elif args.print:
                    instr = args.print.strip()
                    arr = instr.split("\\n")
                    for r, line in enumerate(arr):
                        if args.autoscroll and r > lcd.rows - 1:
                            row = lcd.rows - 1
                            scroll_vertical(f)
                        else:
                            row = r
                        trimmed_to = lcd.columns - (0 if args.wrap else f.position.column)
                        f.print(line[:trimmed_to], 0, row)

                if args.get_position:
                    print(f"{f.position.column} {f.position.row}")

            except AlphaLCDIOError as e:
                print(e)
                sys.exit(1)
            except KeyboardInterrupt:
                sys.exit(0)

    else:
        print("No command specified")
        sys.exit(1)
