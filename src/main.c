#include <msp430.h>
#include <stdint.h>
#include "epd.h"

static void epd_checkerboard(void)
{
	epd_draw_full();

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

	epd_display();
}

static const char msg[] = "Hello, world! 0123456789 This is a test.";
static unsigned msg_i = 0;
static uint8_t glyph_x = 0;
static uint8_t msg_x = 0;
extern const uint8_t font[][6];

static void draw_char(uint8_t x, uint16_t y, uint8_t size, char c)
{
	if (c < 0x20)
		c = '?';

	const uint8_t * const glyph = font[c - 0x20];

	epd_draw_window(x, y, size * 8, size * 6);
	for(uint8_t col = 0 ; col < 6 ; col++)
	{
		const uint8_t bitmap = glyph[col];
		if (size == 1)
			epd_data(bitmap);

/*
		for(uint8_t sy = 0 ; sy < size ; sy++)
		{
			for(uint8_t sx = 0 ; sx < size ; sx++)
			{
			const uint8_t bit = (bitmap >> ((y >> 4) & 7)) & 0x01;
*/
	}
}

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

	epd_draw_full();
	for(unsigned y = 0 ; y < EPD_HEIGHT ; y++)
	{
		for(unsigned x = 0 ; x < EPD_WIDTH ; x += 8)
		{
			if (((x >> 4) & 1) ^ ((y >> 4) & 1))
				epd_data(0xFF);
			else
				epd_data(y);
		}
	}
	epd_display();
	delay(100);

	// try to draw some text...
	int i = 0;

	while(1)
	{
		i++;

		draw_char(16 + ((i & 0x7) << 3), 33, 1, 'A' + (i & 0x1F));
		epd_display();

		//delay(1000);
	}

	epd_shutdown();

	while(1)
		;
}
