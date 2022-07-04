/*
 * SPI flash driver for the MSP430 eink tag.
 *
 * TODO: use the flash controller hardware to do DMA and stuff.
 */
#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
#include "flash.h"
#include "pins.h"

#define SPI_FLASH_CS	0x30
#define SPI_FLASH_CLK	0x15
#define SPI_FLASH_DI	0x17 // data into the spi flash chip
#define SPI_FLASH_DO	0x16 // data out of the spi flash chip

void flash_init(void)
{
	pin_ddr(SPI_FLASH_CS, 1);
	pin_ddr(SPI_FLASH_CLK, 1);
	pin_ddr(SPI_FLASH_DI, 1);
	pin_ddr(SPI_FLASH_DO, 0);

	pin_write(SPI_FLASH_CS, 1);
}


static void flash_write_byte(uint8_t x)
{
	spi_write(SPI_FLASH_CLK, SPI_FLASH_DI, 0, x);
}

static uint8_t flash_read_byte(void)
{
	return spi_write(SPI_FLASH_CLK, 0, SPI_FLASH_DO, 0);
}

void flash_read(uint32_t addr, uint8_t * buf, uint8_t len)
{
	pin_write(SPI_FLASH_CS, 0);
	flash_write_byte(0x03);
	flash_write_byte((addr >> 16) & 0xFF);
	flash_write_byte((addr >>  8) & 0xFF);
	flash_write_byte((addr >>  0) & 0xFF);

	for(uint8_t i = 0 ; i < len ; i++)
		buf[i] = flash_read_byte();

	pin_write(SPI_FLASH_CS, 1);
}

