CROSS=msp430-
CC=$(CROSS)gcc
SIZE=$(CROSS)size

CFLAGS=\
	-g \
	-W \
	-Wall \
	-O3 \
	-std=c99 \
	-mmcu=msp430g2553 \
	-MMD \
	-MF .$(notdir $@).d \

all: epd

provision.h: FORCE
	( cd ../server ; python3 ./provision.py ) > $@

main.o: provision.h

epd: main.o epd.o radio.o flash.o
	$(CC) $(CFLAGS) -o $@ $^
	$(SIZE) $@

-include .*.d

clean:
	$(RM) *.o a.out core epd


FORCE:
