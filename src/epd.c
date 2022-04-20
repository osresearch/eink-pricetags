/*
 * Cheap price tag E-Ink display.
 */
#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include "pins.h"

#define EPD_WIDTH	122
#define EPD_HEIGHT	250

#define EPD_POWER	0x31
#define EPD_CS		0x34
#define EPD_DC		0x35
#define EPD_RESET	0x36
#define EPD_37		0x37

#define EPD_BUSY	0x25
#define EPD_CLK		0x23
#define EPD_DATA	0x24

void delay(unsigned count)
{
	for(volatile unsigned i = 0 ; i < count ; i++)
		for(volatile unsigned j = 0 ; j < 1000 ; j++)
			;
}


static void epd_write(const uint8_t value)
{
	pin_write(EPD_CS, 0);
	spi_write(EPD_CLK, EPD_DATA, 0, value);
	pin_write(EPD_CS, 1);
}

static void epd_command(const uint8_t cmd)
{
	pin_write(EPD_DC, 0);
	epd_write(cmd);
}

static void epd_data(const uint8_t data)
{
	pin_write(EPD_DC, 1);
	epd_write(data);
}

void epd_setup(void)
{
	// copied from 0xecc6
	pin_ddr(EPD_RESET, 1);
	pin_write(EPD_RESET, 0);
	delay(0x32);
	pin_write(EPD_RESET, 1);

	// P3 1, 4, 5, 7 are output
	pin_ddr(EPD_POWER, 1);
	pin_ddr(EPD_CS, 1);
	pin_ddr(EPD_DC, 1);
	pin_ddr(EPD_37, 1);

	pin_write(EPD_POWER, 0);

	// P2 3 and 4 are output, 5 is input
	pin_ddr(EPD_CLK, 1);
	pin_ddr(EPD_DATA, 1);
	pin_ddr(EPD_BUSY, 0); // input
}


void epd_reset(void)
{
	// copied from 0xd628
	pin_write(EPD_POWER, 0);
	pin_write(EPD_RESET, 0);
	delay(0x32);
	pin_write(EPD_RESET, 1);
	delay(0x32);

	// sw reset then wait for busy to go low
	epd_command(0x12);
	while(pin_read(EPD_BUSY) != 0)
		;
}

static const uint8_t epd_lut[] = {
     0xAA, 0x65, 0x55, 0x8A, 0x16, 0x66, 0x65, 0x18, 0x88, 0x99,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
     0x00, 0x00, 0x00, 0x02,
};


void epd_init(void)
{
	// driver output control
	epd_command(0x01);
	epd_data(0xF9); // 250 y values?
	epd_data(0x00);

	// set dummy line period
	epd_command(0x3a);
	epd_data(0x06);

	// set gate line width
	epd_command(0x3b);
	epd_data(0x0b);

	// data entry mode X and Y increment, update X direction
	epd_command(0x11);
	epd_data(0x03);

	// set ram X start and end?
	epd_command(0x44);
	epd_data(0x00);
	epd_data(0x0f); // 0..15, 8 pixels per byte == ~128 pixels

	// set ram Y start and end
	epd_command(0x45);
	epd_data(0x00);
	epd_data(0xf9); // 0..249

	// write VCOM register?
	epd_command(0x2c);
	epd_data(0x79);

	// border waveform control? vsh2? lut3?
	epd_command(0x3c);
	epd_data(0x33);

	// lut
	epd_command(0x32);
	for(unsigned i = 0 ; i < sizeof(epd_lut) ; i++)
		epd_data(epd_lut[i]);

	// display update control 1?
	epd_command(0x21);
	epd_data(0x83);
}

void epd_draw_start(void)
{
	// set X address to 0
	epd_command(0x4e);
	epd_data(0x00);
	// set y address to 0
	epd_command(0x4f);
	epd_data(0x00);

	epd_command(0x24);
	// ... data follows
}

void epd_draw_end(void)
{
	// display update control 2 (enable clock, analog, display mode 1)
	epd_command(0x22);
	epd_data(0xc7);

	// activate display update sequence (busy goes high)
	epd_command(0x20);

	while(pin_read(EPD_BUSY) != 0)
		;
}


void epd_shutdown(void)
{
	// deep sleep mode 1
	epd_command(0x10);
	epd_data(0x01);

	pin_write(EPD_RESET, 1);
	pin_write(EPD_CS, 1);
}

int main(void)
{
	WDTCTL = WDTPW + WDTHOLD; // Stop WDT

	epd_setup();
	epd_reset();
	epd_init();
	epd_draw_start();

/*
 * 1 == white, 0 == black
 * draw order is X first (122 across), 8 pixels at a time.
 * can change this with the incrment?
 */
	for(unsigned y = 0 ; y < EPD_HEIGHT ; y++)
		for(unsigned x = 0 ; x < EPD_WIDTH ; x += 8)
		{
			if (((x >> 4) & 1) ^ ((y >> 4) & 1))
				epd_data(0xFF);
			else
				epd_data(y);
		}

	epd_draw_end();

	epd_shutdown();

	while(1)
		;
}
