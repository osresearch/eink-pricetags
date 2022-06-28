/*
 * Cheap price tag E-Ink display.
 * Pinout was determined experimentally, so it may not be accurate.
 * P2.6 appears to go to a transistor that might switch things on or off,
 * P3.1 must be driven low to talk to the display, maybe it also switches power?
 */
#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include "epd.h"
#include "pins.h"

#define EPD_POWER	0x31 // I think this is !power, but not sure
#define EPD_CS		0x34
#define EPD_DC		0x35
#define EPD_RESET	0x36
#define EPD_37		0x37 // not sure
#define EPD_26		0x26 // not sure

#define EPD_BUSY	0x25
#define EPD_CLK		0x23
#define EPD_DATA	0x24

static void delay(unsigned count)
{
	for(volatile unsigned i = 0 ; i < count ; i++)
		for(volatile unsigned j = 0 ; j < 1000 ; j++)
			;
}

static void epd_wait_busy(void)
{
	while(pin_read(EPD_BUSY) != 0)
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

void epd_data(const uint8_t data)
{
	pin_write(EPD_DC, 1);
	epd_write(data);
}

void epd_setup(void)
{
	// P3 1, 4, 5, 7 are output
	pin_ddr(EPD_RESET, 1);
	pin_ddr(EPD_POWER, 1);
	pin_ddr(EPD_CS, 1);
	pin_ddr(EPD_DC, 1);
	//pin_ddr(EPD_37, 1);

	// P2 3 and 4 are output, 5 is input
	pin_ddr(EPD_CLK, 1);
	pin_ddr(EPD_DATA, 1);
	pin_ddr(EPD_BUSY, 0); // input

/*
	// p2.6 is driven to 0 during startup? 0xf90e
	pin_ddr(EPD_26, 1);
	pin_write(EPD_26, 0);
*/
}


void epd_reset(void)
{
	pin_write(EPD_POWER, 0); // switch on transistor
	pin_write(EPD_RESET, 0);
	delay(0x32);
	pin_write(EPD_RESET, 1);
	delay(0x32);

	// sw reset then wait for busy to go low
	epd_command(0x12);
	epd_wait_busy();
}

#if 1
static const uint8_t epd_lut_full[] = {
     0xAA, 0x65, 0x55, 0x8A, 0x16, 0x66, 0x65, 0x18,
     0x88, 0x99, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
     0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//0xAA, 0x89, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t epd_lut_fast[] = {
	//0xAA, 0x65, 0x55, 0x89, 0x00, 0x00, 0x00, 0x00
     0xAA, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
#else


static const unsigned char epd_lut_full[]= {
    0x80,0x60,0x40,0x00,0x00,0x00,0x00,             //LUT0: BB:     VS 0 ~7
    0x10,0x60,0x20,0x00,0x00,0x00,0x00,             //LUT1: BW:     VS 0 ~7
    0x80,0x60,0x40,0x00,0x00,0x00,0x00,             //LUT2: WB:     VS 0 ~7
    0x10,0x60,0x20,0x00,0x00,0x00,0x00,             //LUT3: WW:     VS 0 ~7
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT4: VCOM:   VS 0 ~7

    0x03,0x03,0x00,0x00,0x02,                       // TP0 A~D RP0
    0x09,0x09,0x00,0x00,0x02,                       // TP1 A~D RP1
    0x03,0x03,0x00,0x00,0x02,                       // TP2 A~D RP2
    0x00,0x00,0x00,0x00,0x00,                       // TP3 A~D RP3
    0x00,0x00,0x00,0x00,0x00,                       // TP4 A~D RP4
    0x00,0x00,0x00,0x00,0x00,                       // TP5 A~D RP5
    0x00,0x00,0x00,0x00,0x00,                       // TP6 A~D RP6

    //0x15,0x41,0xA8,0x32,0x30,0x0A,
};

static const unsigned char epd_lut_fast[]= { //20 bytes
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT0: BB:     VS 0 ~7
    0x80,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT1: BW:     VS 0 ~7
    0x40,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT2: WB:     VS 0 ~7
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT3: WW:     VS 0 ~7
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT4: VCOM:   VS 0 ~7

    0x0A,0x00,0x00,0x00,0x00,                       // TP0 A~D RP0
    0x00,0x00,0x00,0x00,0x00,                       // TP1 A~D RP1
    0x00,0x00,0x00,0x00,0x00,                       // TP2 A~D RP2
    0x00,0x00,0x00,0x00,0x00,                       // TP3 A~D RP3
    0x00,0x00,0x00,0x00,0x00,                       // TP4 A~D RP4
    0x00,0x00,0x00,0x00,0x00,                       // TP5 A~D RP5
    0x00,0x00,0x00,0x00,0x00,                       // TP6 A~D RP6

    //0x15,0x41,0xA8,0x32,0x30,0x0A,
};
#endif



void epd_init(void)
{
	// driver output control
	epd_command(0x01);
	epd_data((EPD_HEIGHT >> 0) & 0xFF);
	epd_data((EPD_HEIGHT >> 8) & 0xFF);

	// set dummy line period
	epd_command(0x3a);
	//epd_data(0x06);
	epd_data(0x1a); // 4 dummy lines per gate

	// set gate line width
	epd_command(0x3b);
	//epd_data(0x0b);
	epd_data(0x04); // 2 usec per line

	// data entry mode X+ and Y- , update X direction
	epd_command(0x11);
	epd_data(0x01);

	// write VCOM register?
	epd_command(0x2c);
	epd_data(0x79);

	// booster soft start control?
	epd_command(0x0C);
	epd_data(0xD7);
	epd_data(0xD6);
	epd_data(0x9D);

	// border waveform control? vsh2? lut3?
	epd_command(0x3c);
	epd_data(0x33);

	epd_wait_busy();

	// lut
	epd_command(0x32);
	for(unsigned i = 0 ; i < sizeof(epd_lut_full) ; i++)
		epd_data(epd_lut_full[i]);

	epd_wait_busy();

/*
	epd_command(0x04);
	epd_data(0x41);
	epd_data(0xA8);
	epd_data(0x32);
*/

	// display update control 1?
	epd_command(0x21);
	epd_data(0x83);
}

void epd_set_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	// x and w must be multiples of 8
	uint16_t y_end = y + h - 1;
	uint16_t x_end = x + w - 1;

	// set ram X start and end, 8-pixels per byte, rounded up
	epd_command(0x44);
	epd_data((x >> 3) & 0xFF);
	epd_data((x_end >> 3) & 0xFF);

	// set ram Y start at the high value and ending at 0
	epd_command(0x45);
	epd_data((y_end >> 0) & 0xFF);
	epd_data((y_end >> 8) & 0xFF);
	epd_data((y >> 0) & 0xFF);
	epd_data((y >> 8) & 0xFF);

	// set X address to 0
	epd_command(0x4e);
	epd_data(x >> 3);

	// set y address to max
	epd_command(0x4f);
	epd_data((y_end >> 0) & 0xFF);
	epd_data((y_end >> 8) & 0xFF);

	epd_wait_busy();

	// start the drawing... data follows
	epd_command(0x24);
}

void epd_draw_start(void)
{
	epd_set_frame(0, 0, EPD_WIDTH, EPD_HEIGHT);
}

void epd_display(void)
{
	// display update control 2 (enable clock, analog, display mode 1)
	epd_command(0x22);
	//epd_data(0xc4);
	epd_data(0xc4);

	// activate display update sequence (busy goes high)
	epd_command(0x20);

	epd_wait_busy();

	// switch to the other lut
/*
	static uint8_t switched_lut = 0;
	if (switched_lut)
		return;
	switched_lut = 1;

	epd_command(0x32);
	for(unsigned i = 0 ; i < sizeof(epd_lut_fast) ; i++)
		epd_data(epd_lut_fast[i]);
*/
}


void epd_shutdown(void)
{
	// deep sleep mode 1
	epd_command(0x10);
	epd_data(0x01);

	pin_write(EPD_RESET, 1);
	pin_write(EPD_CS, 1);

	// power off the e-ink display?
	pin_write(EPD_POWER, 1);
/*
	// sequence in their code
	P3DIR |= 0xF2;
	P2DIR |= 0x18;
	P2DIR &= ~0x20;
	P2OUT |= 0x08;
	P2OUT |= 0x10;
	P3OUT |= 0x10;
	P3OUT |= 0x20;
	P3OUT |= 0x40;
	P3OUT |= 0x80;
	P3OUT &= ~0x02;
*/
}
