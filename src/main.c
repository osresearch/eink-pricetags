#include <msp430.h>
#include <stdint.h>
#include "epd.h"

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
