ifneq ($(KERNELRELEASE),)
include Kbuild

else
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$$PWD -I ./ modules

all: default

genbin:
	echo "X" > $$PWD_bin.o_shipped

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

modules_install:
	$(MAKE) -C /lib/modules/`uname -r`/build M=$$PWD modules_install

endif
