CC=msp430-gcc
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

epd: main.o epd.o font.o radio.o
	$(CC) $(CFLAGS) -o $@ $^

-include .*.d

clean:
	$(RM) *.o a.out core epd

