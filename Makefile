TARGET := lcdi2c
TOOLS := /usr/bin
PREFIX := 
KDIR := ../../linux/linux
PWD := $(shell pwd)

obj-m :=  $(TARGET).o
$(TARGET)-objs := i2clcd8574_lib.o i2clcd8574.o



.PHONY: all clean cleanprf
	

all: proof
	$(MAKE) -C $(KDIR) \
		M=$(PWD) \
		ARCH=arm CXX=$(TOOLS)/$(PREFIX)g++$(SUFFIX) CC=$(TOOLS)/$(PREFIX)gcc$(SUFFIX) \
		modules

		
	
prooflib.o: i2clcd8574_lib.c
	$(CC) -c -o $@ $<

proof: CC = $(TOOLS)/$(PREFIX)gcc$(SUFFIX)
proof: prooflib.o proof.c 
	$(CC) -o $@ prooflib.o proof.c

cleanprf:
	rm -rf prooflib.o poc

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -rf *.o
	rm -rf prooflib.o proof
	




