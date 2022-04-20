#include <msp430.h>
#include <stdint.h>
#include "epd.h"

static void epd_checkerboard(void)
{
	epd_draw_start();

/*
 * 1 == white, 0 == black
 * draw order is X first (122 across), 8 pixels at a time.
 * can change this with the incrment?
 */
	for(unsigned y = 0 ; y < EPD_HEIGHT ; y++)
		for(unsigned x = 0 ; x < EPD_WIDTH ; x += 8)
		{
			if (x < 64 && y < 32)
				epd_data(0x00);
			else
				epd_data(0xFF);
/*
			if (((x >> 4) & 1) ^ ((y >> 4) & 1))
				epd_data(0xFF);
			else
				epd_data(y);
*/
		}

	epd_draw_end();
}

static const char msg[] = "Hello, world! 0123456789 This is a test.";
static unsigned msg_i = 0;
static uint8_t glyph_x = 0;
static uint8_t msg_x = 0;
extern const uint8_t font[][6];

static uint8_t bitmap(unsigned y, unsigned x)
{
	const char c = msg[msg_i];
	if (c == '\0')
	{
		msg_i = 0;
		return 0xFF;
	}

	if (msg_x != x)
	{
		msg_x = x;
		glyph_x++;
	}

	if (glyph_x == (6 << 2))
	{
		glyph_x = 0;
		msg_i++;
		return 0xFF;
	}

	const uint8_t * const glyph = font[c - 0x20];
	const uint8_t bitmap = glyph[glyph_x >> 2];

	const uint8_t bit = (bitmap >> ((y >> 4) & 7)) & 0x01;

	return bit ? 0x00 : 0xFF;
}


int main(void)
{
	WDTCTL = WDTPW + WDTHOLD; // Stop WDT

	epd_setup();
	epd_reset();
	epd_init();

	if (0)
		while(1) epd_checkerboard();
	else

	// try to draw some text...
	while(1)
	{
		epd_draw_start();
		for(unsigned y = 0 ; y < EPD_HEIGHT ; y++)
			for(unsigned x = 0 ; x < EPD_WIDTH ; x += 8)
			{
				epd_data(bitmap(x,y));
			}
		epd_draw_end();

		//delay(1000);
	}

	epd_shutdown();

	while(1)
		;
}
