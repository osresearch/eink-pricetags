#include <msp430.h>
#include <stdint.h>
#include <string.h>
#include "epd.h"
#include "flash.h"
#include "radio.h"


// incremented once per second
static volatile uint16_t timer;

// read the battery voltage via the Vcc/2 input to the ADC
uint16_t battery_voltage(void)
{
	// measure ADC channel 11 (Vcc/2) with 2.5V reference
	// this is good for Vcc up to 5V, and the scale factor is 5.0/1024
	// https://www.ti.com/lit/an/slaa828b/slaa828b.pdf
	ADC10CTL0 = 0
		| SREF_1	// Vr+ = Vref, Vr = Vss
		| REF2_5V	// Vref = 2.5V
		| REFON		// Turn on the reference generator
		| ADC10SHT_2	// 16 ADC clocks
		| ADC10SR	// "reduces curennt consumption of buffer"
		| ADC10ON	// turns on the ADC
		;

	ADC10CTL1 = 0
		| INCH_11	// (Vcc-Vss)/2
		| SHS_0		// Sample-and-hold source ADC10SC bit
		| ADC10DIV_0	// no ADC clock divider
		//| ADC10SSEL_0	// ADC clock is ADC10OSC (0.131 mA)
		| ADC10SSEL_2	// ADC clock is MCLK is lower power (0.026mA)
		;

	// enable conversion and start conversion
	ADC10CTL0 |= ENC | ADC10SC;

	// busy wait for conversion to complete
	while (ADC10CTL1 & ADC10BUSY)
		;

	// turn off the ADC and reference voltage
	ADC10CTL0 &= ~(ENC | ADC10IFG | ADC10ON | REFON);

	return ADC10MEM;
}

// re-generated during the build process
#include "provision.h"


// draws an RLE compressed image
void draw_image(const uint8_t * image, uint16_t len, uint8_t invert)
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

			if (bits != 0)
				continue;

			if (invert)
				byte ^= 0xFF;

			epd_data(byte);
		}
	}

	// clean up at the end
	if (bits != 0)
		epd_data(byte);

	epd_display();
	epd_shutdown();
}

typedef struct {
	uint32_t id;
	uint8_t not_ready;
	uint8_t need_draw;
	uint8_t resv1;
	uint8_t resv2;
	uint32_t resv3;
	uint32_t resv4;
	uint8_t map[16]; // offset 16
	uint8_t data[]; // offset 32
} flash_img_t;

static flash_img_t img;
#define img_addr 0x0
#define img_map_offset 16
#define img_data_offset 32

// check to see if we have all the parts
// note that a 1 means we have not yet received it, due flash polarity
// todo: don't hardcode the number of parts
static int img_check_complete(void)
{
	for(unsigned i = 0 ; i < 126 ; i++)
	{
		const uint8_t b = img.map[i >> 3];
		const uint8_t bit = 1 << (i & 7);

		if ((b & bit) == 1)
		{
			img.not_ready = 1;
			return 0;
		}
	}

	img.not_ready = 0;
	return 1;
}


void img_init(const uint16_t flash_addr)
{
	// the image is stored on a full flash page,
	// along with some meta data that is fetched during the boot
	flash_read(flash_addr, &img, sizeof(img));

	img_check_complete();
}


void img_draw(const uint16_t flash_addr)
{
	epd_setup();
	epd_reset();
	epd_init();

	uint16_t addr = flash_addr + img_data_offset;

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

	img.need_draw = 0;
}

typedef struct {
	uint32_t tag_type;
	uint32_t tag_id;
	uint32_t githash;
	uint32_t install_date;
	uint16_t voltage;
	uint16_t reserved;
	uint32_t img_id;
	uint8_t img_map[16];
}
__attribute__((__packed__))
msg_hello_t;

typedef struct {
	uint32_t img_id;
	uint16_t offset;
	uint16_t flags;
	uint8_t data[32];
}
__attribute__((__packed__))
msg_data_t;

#define REPLY_FLAG_OK 1

static uint8_t msg_buf[40];

int check_for_updates(uint32_t flash_addr)
{
	msg_hello_t * const hello = (void*) msg_buf;
	hello->tag_type = tag_type;
	hello->tag_id = macaddr;
	hello->githash = githash;
	hello->install_date = install_date;
	hello->voltage = battery_voltage();
	hello->img_id = img.id;

	memcpy(hello->img_map, img.map, sizeof(hello->img_map));

	// send a ping?
	radio_tx(gateway, (const void*) hello, 40); // sizeof(msg));

	// todo: wait for some number of reply
	msg_data_t * const reply = (void *) msg_buf;
	if (radio_rx(macaddr, (void*) reply, 40 /*sizeof(reply)*/, 7500) != 1)
		return 0;

	// if they say everything is ok, then we go back to deep sleep
	if (reply->flags & REPLY_FLAG_OK)
		return 0;

	// we have data!
	if (reply->img_id != img.id)
	{
		// starting a new image
		memset(&img, 0xFF, sizeof(img));
		img.id = reply->img_id;

		// erase the old one, write in the new metadata
		flash_erase(flash_addr);
		flash_write(flash_addr, &img, sizeof(img));
	}

	// the img_map stores based on 32-byte packets,
	// so shift by 5 to find packet number, then by 3 to get byte into map
	const uint16_t img_offset = reply->offset;
	const uint8_t byte_num = (img_offset >> 5) >> 3;
	const uint8_t bit_num = 1 << ((img_offset >> 5) & 7);

	// todo: validate byte_num, img_offset, etc
	// note that this is negative logic, since 
	img.map[byte_num] &= ~bit_num;

	flash_write(flash_addr + img_data_offset + img_offset, reply->data, sizeof(reply->data));

	img_check_complete();
	flash_write(flash_addr + img_map_offset + byte_num, &img.map[byte_num], 1);

	return 1;
}

int main(void)
{
	WDTCTL = WDTPW + WDTHOLD; // Stop WDT

	// init the flash and then load the image meta data
	flash_init();
	img_init(img_addr);

	// draw the boot screen, not the flash image for the first second
	draw_image(bootscreen, bootscreen_len, !img.not_ready);

	// configure the radio, then let it turn off again
	radio_init(channel);
	radio_sleep();

	// setup the watchdog to trigger every three seconds or so
	// not sure why this isn't once per second
	BCSCTL3 = LFXT1S_2; // select VLO as the ACLK (used by the WDT)
	WDTCTL = WDT_ADLY_1000;
	IE1 |= WDTIE;
	__enable_interrupt(); // GIE not set in LPM3 bits?

	// let's do one check in before we sleep
	while(check_for_updates(img_addr))
		;

	while(1)
	{
		// go to sleep, to be woken up by the WDT interrupt in 3 seconds
		LPM3;

		// if the image is ready, then draw it and go back to sleep
		if (!img.not_ready)
		{
			if (img.need_draw)
				img_draw(img_addr);

			// every so often, check in with the head node
			// do so more often if we do not have a complete image
			// watchdog triggers every 3s, so this is about
			// once every 768 seconds
			if ((timer & 0xff) != 0)
				continue;
		}

		// the image is not ready or we need to do a period check in
		while(check_for_updates(img_addr))
			;

		// turn the radio off before we go back to bed
		radio_sleep();
	}
}


void __attribute__((interrupt(WDT_VECTOR)))
watchdog_timer(void)
{
	timer++;
	LPM3_EXIT;
}
