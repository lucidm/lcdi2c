TOOLS := /usr/bin
PREFIX := 
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m :=  lcdi2c.o

all:
	$(MAKE) -C $(KDIR) \
		M=$(PWD) \
		CROSS_COMPILE=$(TOOLS)/$(PREFIX) \
		modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

modules_install:
	$(MAKE) -C /lib/modules/`uname -r`/build M=$(PWD) modules_install
