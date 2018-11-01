# class for communication with the device. Will try to configure itself based on
# /sys/class/alphalcd/lcdi2c/meta file. All ioctls will be accessible through
# class attributes
#
# (c) Francesc Oller 2018
#
# install dlcdi2c.rb as a gem:
#
#   1.- set and create/copy following dirs/files
#   .
#   ├── dlcdi2c.gemspec
#   └── lib
#       └── dlcdi2c.rb
#
#   2.- edit dlcdi2c.gemspec as:
#
#   Gem::Specification.new do |s|
#     s.name        = 'dlcdi2c'
#     s.version     = '1.0.0'
#     s.date        = '2018-10-29'
#     s.summary     = "dlcdi2c"
#     s.description = "A lucidm/lcdi2c kernel driver ruby wrapper"
#     s.authors     = ["Francesc Oller"]
#     s.files       = ["lib/dlcdi2c.rb"]
#     s.license       = 'MIT'
#   end
#
#   3.- ...$ gem build dlcdi2c.gemspec
#
#   4.- ...$ sudo gem install dlcdi2c-1.0.0.gem
#   ( on RPi Raspbian Stretch will be installed at /var/lib/gems/2.3.0/gems/ )
#
# usage examples:
#
#   require 'dlcdi2c'
#
#   lcd = LcdI2C.new(1, 0x27)
#   # reset to defaults
#   lcd.reset
#
#   # go to home [0, 0]
#   lcd.home
#
#   # clear display and go to home
#   lcd.clear
#
#   # go to column 3 row 2 (zero-based coordinates)
#   lcd.position = [3, 2]
#
#   # get cursor coordinates
#   column, row = lcd.position
#
#   # write char 'a' at cursor coordinates
#   lcd.char = 'a'
#
#   # get char at cursor coordinates
#   ch = lcd.char
#
#   # make cursor visible (true), invisible (false)
#   lcd.cursor = true; lcd.cursor = false
#
#   # make cursor blink (true), not blink (false)
#   lcd.blink = true; lcd.blink = false
#
#   # turn on backlight (true), off (false)
#   lcd.backlight = true; lcd.backlight = false
#
#   # get cursor, blink, backlight state
#   cursor = lcd.cursor; blink = lcd.blink; backlight = lcd.backlight
#
#   # scroll one column to the right (true), left (false)
#   lcd.scrollhz = true; lcd.scrollhz = false
#
#   # define custom char at position 4
#   lcd.customchar = [0x04, 0x0, 0x0, 0xa, 0x1f, 0x1f, 0xe, 0x4, 0x0]
#
#   # get custom char definition at position 6
#   position, *ch = lcd.customchar(6)[0..8]
#
#   # display 4 rows in a 20x4 lcd
#   lcd.display("Welcome PBE Project!\nUse the source Luck!\nMay the source...\n     ...be with you!\n")

class LcdI2C
  
  def initialize(bus, address)
    File.open("/sys/bus/i2c/devices/#{bus}-%04x/name" % [address], "r") do |fd|
      @name = "/dev/#{fd.read().strip()}"
    end
    
    parse("/sys/class/alphalcd/lcdi2c/meta")
    @file = File.new(@name, 'w')
  end
      
  def method_missing(method, *args)
    ioctl_name = ioctl_cmd(method.to_s)
    raise NoMethodError if ioctl_name.nil?
    ioctl_value = @ioctls[ioctl_name].to_i(16)
    nr, typem, size, direction = cmdparse(ioctl_value)
    if direction & 1 != 0                                          # write command
      if nr & 2 != 0                                                 # char argument
        if args.empty?                                                 # no-arg command: RESET, HOME, CLEAR
          arg = '1'
        elsif args[0] == !!args[0]                                      # args[0] is boolean, SETXXX
          arg = args.map { |b| if b then '1' else '0' end }.join
        else                                                           # args[0] is char, SETCHAR
          arg = args.join
        end
      else                                                           # numeric argument: SETPOSITION, SETCUSTOMCHAR
        arg = args[0].pack('C*')
      end
    else                                                           # read command, GETXXX
      # prepare 9-bytes array: GETCUSTOMCHAR
      arg = ' ' * 9
      if ! args.empty?
        arg = args.pack('C*')
        #arg[0] = args[0]
      end
    end
    @file.ioctl(ioctl_value, arg)
    if direction & 2 != 0                                          # read command
      if nr & 2 != 0                                                 # char argument
        if ioctl_name == "GETCHAR"                                     # GETCHAR: return char
          arg[0]
        else                                                           # GETXXX: return true if '1' false if '0'
          arg[0] == '1' ? true : false
        end
      else                                                           # numeric argument: GETPOSITION, GETCUSTOMCHAR
        arg.unpack('C*')                                             #   return array, when destructuring trailing ints are discarded
      end
    end
  end
  
  def respond_to_missing?(method, include_private = false)
    ! ioctl_cmd(method.to_s).nil?
  end

  def write(line)
    @file.write(line)
    @file.flush
  end
  
  def display(multi)
    multi.each_line.with_index do |line, index|
      self.position = [0, index]
      write(line.chomp)
    end
  end

private

  def ioctl_cmd(method)
    if method[-1] == '='
      ioctl = "SET#{method[0...-1].upcase}"
      if @ioctls.has_key? ioctl
        ioctl
      elsif @ioctls.has_key? method[0...-1].upcase # SCROLLHZ
        method[0...-1].upcase
      end
    else
      ioctl = "GET#{method.upcase}"
      if @ioctls.has_key? ioctl
        ioctl
      elsif @ioctls.has_key? method.upcase
        method.upcase
      end
    end
  end
  
  def cmdparse(value)
    return value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff, (value >> 30) & 0x03
  end
  
  def parse(meta)
    File.open(meta, "r") do |fd|
      @topo = fd.gets.strip!.split(':')[1]
      @rows = fd.gets.strip!.split(':')[1].to_i
      @columns = fd.gets.strip!.split(':')[1].to_i
      @rows_addresses = fd.gets.strip!.split(':')[1]
      @pins = fd.gets.strip!.split(':')[1]
      # ioctls
      fd.gets
      @ioctls = {}
      fd.each_line { |line|
        key, value = line.strip!.split("=", 2)
        @ioctls[key] = value
      }
    end
  end

end
