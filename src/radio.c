/*
 * Amiccom A7106 radio interface.
 *
 * Currently we just turn it off.
 */

#include "pins.h"

#define RADIO_SDIO	0x12
#define RADIO_SCK	0x14
#define RADIO_SCS	0x13

#define RADIO_CMD_STBY	0xA0
#define RADIO_CMD_SLEEP	0x80

static void radio_write(uint8_t byte)
{
	spi_write(RADIO_SCK, RADIO_SDIO, 0, byte);
}

static void radio_cs(uint8_t selected)
{
	pin_write(RADIO_SCS, !selected);
}

void radio_init(void)
{
	pin_ddr(RADIO_SDIO, 1);
	pin_ddr(RADIO_SCK, 1);
	pin_ddr(RADIO_SCS, 1);

	pin_write(RADIO_SCS, 1);

	
	radio_cs(1);
	radio_write(RADIO_CMD_SLEEP);
	radio_cs(0);
}
