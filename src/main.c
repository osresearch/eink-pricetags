#include <msp430.h>
#include <stdint.h>
#include "epd.h"
#include "flash.h"
#include "radio.h"


// incremented once per second
static volatile uint32_t timer;


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
	uint8_t len;
	uint32_t gw_id;
	uint32_t tag_id;
	uint8_t channel;
} radio_config_t;

const radio_config_t config = {
	.gw_id = 0x55abcdef,
	.tag_id = 0x50123456,
	.channel = 4,
};

typedef struct {
	uint8_t len;
	uint32_t tag_id;
	uint32_t img_id;
	uint16_t offset; // in byte multiples
}
__attribute__((__packed__))
msg_hello_t;

typedef struct {
	uint32_t img_id;
	uint16_t offset;
	uint8_t data[58];
}
__attribute__((__packed__))
msg_data_t;


static uint32_t img_id = 0;
static uint32_t img_offset = 0;

static uint8_t msg_buf[64];

int check_for_updates(uint32_t flash_addr)
{
	msg_hello_t * const hello = (void*) msg_buf;
	hello->len = sizeof(*hello);
	hello->tag_id = config.tag_id;
	hello->img_id = img_id;
	hello->offset = img_offset;

	// send a ping?
	radio_tx(config.gw_id, (const void*) hello, 64); // sizeof(msg));

	// wait for a reply
	msg_data_t * const reply = (void *) msg_buf;
	if (radio_rx(config.tag_id, (void*) reply, 64 /*sizeof(reply)*/, 10000) != 1)
		return -1;

	// we have data!
	if (reply->img_id != img_id)
	{
		// starting a new image
		img_id = reply->img_id;
		img_offset = 0;
		flash_erase(flash_addr);
	}

	// unexpected packet? discard it
	if (reply->offset != img_offset)
		return 0;

	flash_write(flash_addr + img_offset, reply->data, sizeof(reply->data));
	img_offset += sizeof(reply->data);

#define IMG_SIZE 4096

	if (img_offset >= IMG_SIZE)
		return 1;

	return 0;
}

int main(void)
{
	const uint32_t flash_addr = 0;

	WDTCTL = WDTPW + WDTHOLD; // Stop WDT

	// read the image from flash
	flash_init();
	draw_flash(flash_addr);

	// configure the radio, then let it turn off again
	radio_init(config.channel);
	radio_sleep();

	// setup the watchdog to trigger every second
	BCSCTL3 = LFXT1S_2; // select VLO as the ACLK (used by the WDT)
	WDTCTL = WDT_ADLY_1000; // sleep for one second
	IE1 |= WDTIE;
	__enable_interrupt(); // GIE not set in LPM3 bits?

	while(1)
	{
		LPM3;

/*
		// every so often, check in with the head node
		if ((timer & 0x3) != 0)
			continue;
*/

		while(1)
		{
			int status = check_for_updates(flash_addr);

			if (status == -1)
				break;

			if (status == 1)
				draw_flash(flash_addr);
		}

		radio_sleep();
/*

		short i = 0;
		msg[i++] = '=';
		msg[i++] = hexdigit(img_id >> 28);
		msg[i++] = hexdigit(img_id >> 24);
		msg[i++] = hexdigit(img_id >> 20);
		msg[i++] = hexdigit(img_id >> 16);
		msg[i++] = hexdigit(img_id >> 12);
		msg[i++] = hexdigit(img_id >>  8);
		msg[i++] = hexdigit(img_id >>  4);
		msg[i++] = hexdigit(img_id >>  0);
		msg[i++] = '!';

		glyph_x = msg_i = msg_x = 0;
		draw_msg();
*/
	}
}


void __attribute__((interrupt(WDT_VECTOR)))
watchdog_timer(void)
{
	timer++;
	LPM3_EXIT;
}

