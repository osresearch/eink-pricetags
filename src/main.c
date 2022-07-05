#include <msp430.h>
#include <stdint.h>
#include <string.h>
#include "epd.h"
#include "flash.h"
#include "radio.h"


// incremented once per second
static volatile uint32_t timer;

extern const uint8_t boot_screen[];
static const
#include "provision.h"

#if 0

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

static char hexdigit(unsigned x)
{
	x &= 0xF;
	if (x <= 9)
		return '0' + x;
	else
		return 'A' + x - 0xa;
}

#endif

void draw_image(const uint8_t * image, uint16_t len)
{
	epd_setup();
	epd_reset();
	epd_init();

	epd_draw_start();

	// decompress the RLE encoded value
	uint8_t byte = 0;
	uint8_t bits = 0;

	for(uint16_t i = 0 ; i < len ; i++)
	{
		uint8_t enc = image[i];
		uint8_t val = (enc >> 7) & 1; // is this a white or black pixel
		uint8_t bit_len = (enc & 0x7F);
		for(unsigned j = 0 ; j < bit_len ; j++)
		{
			byte = (byte << 1) | val;
			bits = (bits + 1) & 7;
			if (bits == 0)
				epd_data(byte);
		}
	}

	// clean up at the end
	if (bits != 0)
		epd_data(byte);

	epd_display();
	epd_shutdown();
}

static uint32_t img_id = 0;
static uint8_t img_map[16];


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

typedef struct {
	uint32_t tag_type;
	uint32_t tag_id;
	uint32_t githash;
	uint32_t install_date;
	uint32_t reserved;
	uint32_t img_id;
	uint8_t img_map[16];
}
__attribute__((__packed__))
msg_hello_t;

typedef struct {
	uint32_t img_id;
	uint16_t offset;
	uint16_t reserved;
	uint8_t data[32];
}
__attribute__((__packed__))
msg_data_t;

static uint8_t msg_buf[40];

int check_for_updates(uint32_t flash_addr)
{
	msg_hello_t * const hello = (void*) msg_buf;
	hello->tag_type = tag_type;
	hello->tag_id = macaddr;
	hello->githash = githash;
	hello->install_date = install_date;
	hello->img_id = img_id;

	memcpy(hello->img_map, img_map, sizeof(hello->img_map));

	// send a ping?
	radio_tx(gateway, (const void*) hello, 40); // sizeof(msg));

	// todo: wait for some number of reply
	msg_data_t * const reply = (void *) msg_buf;
	if (radio_rx(macaddr, (void*) reply, 40 /*sizeof(reply)*/, 10000) != 1)
		return -1;

	// we have data!
	if (reply->img_id != img_id)
	{
		// starting a new image
		img_id = reply->img_id;
		memset(img_map, 0, sizeof(img_map));
		flash_erase(flash_addr);
	}

	// the img_map stores based on 32-byte packets,
	// so shift by 5 to find packet number, then by 3 to get byte into map
	const uint16_t img_offset = reply->offset;
	const uint8_t byte_num = (img_offset >> 5) >> 3;
	const uint8_t bit_num = 1 << ((img_offset >> 5) & 7);

	// todo: validate byte_num, img_offset, etc
	// todo: write image map into flash
	img_map[byte_num] |= bit_num;

	flash_write(flash_addr + img_offset, reply->data, sizeof(reply->data));

	// check to see if we have all the parts
	for(unsigned i = 0 ; i < 125 ; i++)
	{
		const uint8_t b = img_map[i >> 3];
		const uint8_t bit = 1 << (i & 7);
		if ((b & bit) == 0)
			return 0;
	}

	// all the fields are filled in!
	return 1;
}

int main(void)
{
	const uint32_t flash_addr = 0;

	WDTCTL = WDTPW + WDTHOLD; // Stop WDT

	// draw the boot screen, not the flash image for the first second
	draw_image(bootscreen, bootscreen_len);

	flash_init();

	// configure the radio, then let it turn off again
	radio_init(channel);
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

