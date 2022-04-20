#ifndef _epd_pins_h_
#define _epd_pins_h_

#include <msp430.h>
#include <stdint.h>

typedef struct {
	volatile uint8_t * const ddr;
	volatile const uint8_t * const in;
	volatile uint8_t * const out;
} port_t;

static const port_t ports[] = {
	{ NULL, NULL, NULL },
	{ &P1DIR, &P1IN, &P1OUT, },
	{ &P2DIR, &P2IN, &P2OUT, },
	{ &P3DIR, &P3IN, &P3OUT, },
};

/* When called with compile time constants this is a single instruction */
static inline void pin_ddr(const uint8_t port, const int value)
{
	const port_t * const p = &ports[(port >> 4) & 0x3];
	const uint8_t mask = 1 << (port & 0x7);
	if (value)
		*p->ddr |= mask;
	else
		*p->ddr &= ~mask;
}

static inline void pin_write(const uint8_t port, const int value)
{
	const port_t * const p = &ports[(port >> 4) & 0x3];
	const uint8_t mask = 1 << (port & 0x7);
	if (value)
		*p->out |= mask;
	else
		*p->out &= ~mask;
}

static inline uint8_t pin_read(const uint8_t port)
{
	const port_t * const p = &ports[(port >> 4) & 0x3];
	const uint8_t mask = 1 << (port & 0x7);
	return *p->in & mask;
}

/*
 * Create a spi_write function for a specific pin and clock.
 * Should be called with compile time constants for the data port
 * and clock port.  If in_port is zero, no value will be read.
 */
static inline uint8_t spi_write(
	const uint8_t clk_port,
	const uint8_t out_port,
	const uint8_t in_port,
	uint8_t out_value
)
{
	uint8_t in_value = 0;

	for(uint8_t mask = 0x80 ; mask != 0 ; mask >>= 1)
	{
		pin_write(out_port, out_value & mask);
		pin_write(clk_port, 1);

		if (in_port)
			in_value = (in_value << 1) | pin_read(in_port);

		pin_write(clk_port, 0);
	}

	return in_value;
}

#endif
