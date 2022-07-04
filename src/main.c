#include <msp430.h>
#include <stdint.h>
#include "epd.h"
#include "flash.h"
#include "radio.h"


// incremented once per second
static volatile uint32_t timer;


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

	epd_display();
}

static char msg[16];
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

void draw_msg(void)
{
	epd_setup();
	epd_reset();
	epd_init();

	// try to draw some text...
	epd_draw_start();
	for(unsigned y = 0 ; y < EPD_HEIGHT ; y++)
		for(unsigned x = 0 ; x < EPD_WIDTH ; x += 8)
		{
			epd_data(bitmap(x,y));
		}
	epd_display();
	epd_shutdown();
}


void draw_flash(uint16_t addr)
{
	epd_setup();
	epd_reset();
	epd_init();

	epd_draw_start();
	for(unsigned y = 0 ; y < EPD_HEIGHT ; y++)
	{
		// 128 bits of data
		uint8_t data[(EPD_WIDTH + 7)/8];

		flash_read(addr, data, sizeof(data));
		addr += sizeof(data);

		for(unsigned x = 0 ; x < (EPD_WIDTH+7)/8 ; x++)
			epd_data(data[x]);
	}
	epd_display();
	epd_shutdown();
}

static char hexdigit(unsigned x)
{
	x &= 0xF;
	if (x <= 9)
		return '0' + x;
	else
		return 'A' + x - 0xa;
}

typedef struct {
	uint8_t controller[4];
	uint8_t id[4];
	uint8_t channel;
} radio_config_t;

static const radio_config_t config = {
	.controller = { 0x55, 0xab, 0xcd, 0xef },
	.id = { 0x50, 0x12, 0x34, 0x56 },
	.channel = 4,
};


int main(void)
{
	WDTCTL = WDTPW + WDTHOLD; // Stop WDT

	// read the image from flash
	flash_init();

	draw_flash(0);

	// configure the radio, then let it turn off again
	radio_init(config.channel);
	radio_sleep();

	static const uint8_t tx_msg[] = "\x40This is a test message";

	while(1)
	{
		radio_tx(config.controller, tx_msg, 64);
		radio_sleep();
		for(volatile int i = 0 ; i < 1000 ; i++)
			for(volatile int j = 0 ; j < 1000 ; j++)
				;
	}


	BCSCTL3 = LFXT1S_2; // select VLO as the ACLK (used by the WDT)
	WDTCTL = WDT_ADLY_1000; // sleep for one second
	IE1 |= WDTIE;
	__enable_interrupt(); // GIE not set in LPM3 bits?

	while(1)
	{
		LPM3;

		// every so often, check in with the head node
		if ((timer & 0x3) != 0)
			continue;

		radio_tx(config.controller, tx_msg, 64);

		short i = 0;
		msg[i++] = '=';
		msg[i++] = hexdigit(timer >> 28);
		msg[i++] = hexdigit(timer >> 24);
		msg[i++] = hexdigit(timer >> 20);
		msg[i++] = hexdigit(timer >> 16);
		msg[i++] = hexdigit(timer >> 12);
		msg[i++] = hexdigit(timer >>  8);
		msg[i++] = hexdigit(timer >>  4);
		msg[i++] = hexdigit(timer >>  0);
		msg[i++] = '!';

		glyph_x = msg_i = msg_x = 0;
		draw_msg();
	}
}


void __attribute__((interrupt(WDT_VECTOR)))
watchdog_timer(void)
{
	timer++;
	LPM3_EXIT;
}

