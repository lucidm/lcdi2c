ifneq ($(KERNELRELEASE),)
include Kbuild

else
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$$PWD -I ./ modules

all: default

genbin: default install
	echo "X" > $$PWD_bin.o_shipped

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

install:
	$(MAKE) -C /lib/modules/`uname -r`/build M=$$PWD modules_install
	depmod -a

dtbo: lcdi2c.dts
	dtc -@ -I dts -O dtb -o lcdi2c.dtbo lcdi2c.dts

endif
