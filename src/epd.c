/*
 * Cheap price tag E-Ink display.
 */
#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include "epd.h"
#include "pins.h"

#define EPD_POWER	0x31
#define EPD_CS		0x34
#define EPD_DC		0x35
#define EPD_RESET	0x36
#define EPD_37		0x37

#define EPD_BUSY	0x25
#define EPD_CLK		0x23
#define EPD_DATA	0x24


static uint8_t epd_mode = 0;

void delay(unsigned count)
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



static void epd_init_full(void)
{
	epd_mode = 0;

	// driver output control
	epd_command(0x01);
	epd_data((EPD_HEIGHT >> 0) & 0xFF);
	epd_data((EPD_HEIGHT >> 8) & 0xFF);

	// set dummy line period
	epd_command(0x3a);
	epd_data(0x06);

	// set gate line width
	epd_command(0x3b);
	epd_data(0x0b);

	// data entry mode X+ and Y- , update X direction
	epd_command(0x11);
	epd_data(0x01);

	// write VCOM register?
	epd_command(0x2c);
	epd_data(0x79);

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
	epd_wait_busy();

	epd_init_full();
}


static void epd_init_window(void)
{
	epd_mode = 1;

	// write register for display option (enable ping pong?)
	epd_command(0x37);
	epd_data(0x00);
	epd_data(0x00);
	epd_data(0x00);
	epd_data(0x00);
	epd_data(0x40);
	epd_data(0x00);
	epd_data(0x00);

	// copy the full into the partial?
	epd_command(0x22);
	epd_data(0xc0);
	epd_command(0x20);
	epd_wait_busy();

	// border waveform
	epd_command(0x3c);
	epd_data(0x01);
}

static void epd_enable_window(uint8_t x, uint16_t y, uint8_t w, uint16_t h)
{
	// set ram X start and end, 8-pixels per byte, rounded up by display
	epd_command(0x44);
	epd_data(x >> 3);
	epd_data((x + w) >> 3);

	// set ram Y start at the high value and ending at 0
	epd_command(0x45);
	const uint16_t end_y = y + h;
	epd_data((end_y >> 0) & 0xFF);
	epd_data((end_y >> 8) & 0xFF);
	epd_data((y >> 0) & 0xFF);
	epd_data((y >> 8) & 0xFF);

	// set X address to start
	epd_command(0x4e);
	epd_data(x >> 3);

	// set y address to ending address
	epd_command(0x4f);
	epd_data((end_y >> 0) & 0xFF);
	epd_data((end_y >> 8) & 0xFF);

	epd_command(0x24);
	// ... data follows
}

void epd_draw_full(void)
{
	if (epd_mode != 0)
		epd_init_full();
	epd_enable_window(0, 0, EPD_WIDTH, EPD_HEIGHT);
}

void epd_draw_window(uint8_t x, uint16_t y, uint8_t w, uint16_t h)
{
	if (epd_mode != 1)
		epd_init_window();
	epd_enable_window(x, y, w, h);
}

void epd_display(void)
{
	// display update control 2
	// mode 0 == full update (enable clock, analog, display mode 1)
	// mode 1 == partial update
	epd_command(0x22);
	epd_data(epd_mode == 0 ? 0xc7 : 0x0c);

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
}
