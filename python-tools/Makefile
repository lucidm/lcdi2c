ifneq ($(KERNELRELEASE),)
include Kbuild

else
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

module:
	$(MAKE) -C $(KDIR) M=$$PWD -I ./ modules

all: module dtbo sub-make

sub-make:
	$(MAKE) -C python-tools -I ./ all

genbin: module install
	echo "X" > $$PWD_bin.o_shipped

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
	$(MAKE) -C python-tools clean
	rm -f *.o_shipped *.dtbo

install: module dtbo
	$(MAKE) -C /lib/modules/`uname -r`/build M=$$PWD modules_install
	depmod -a

orangepi_install: install
	sudo orangepi-add-overlay lcdi2c.dts


dtbo: lcdi2c.dts
	dtc -@ -I dts -O dtb -o lcdi2c.dtbo lcdi2c.dts

endif
