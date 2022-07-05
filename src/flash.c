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

void flash_read(uint32_t addr, void * buf_ptr, uint8_t len)
{
	uint8_t * buf = buf_ptr;

	pin_write(SPI_FLASH_CS, 0);
	flash_write_byte(0x03);
	flash_write_byte((addr >> 16) & 0xFF);
	flash_write_byte((addr >>  8) & 0xFF);
	flash_write_byte((addr >>  0) & 0xFF);

	for(uint8_t i = 0 ; i < len ; i++)
		buf[i] = flash_read_byte();

	pin_write(SPI_FLASH_CS, 1);
}

uint8_t flash_status(void)
{
	pin_write(SPI_FLASH_CS, 0);
	flash_write_byte(0x05);
	uint8_t sr = flash_read_byte();
	pin_write(SPI_FLASH_CS, 1);

	return sr;
}


void flash_wren()
{
	(void) flash_status();
	pin_write(SPI_FLASH_CS, 0);
	flash_write_byte(0x06);
	pin_write(SPI_FLASH_CS, 1);
}

void flash_erase(uint32_t addr)
{
	flash_wren();

	pin_write(SPI_FLASH_CS, 0);
	flash_write_byte(0x20);
	flash_write_byte((addr >> 16) & 0xFF);
	flash_write_byte((addr >>  8) & 0xFF);
	flash_write_byte((addr >>  0) & 0xFF);
	pin_write(SPI_FLASH_CS, 1);

#define SPI_WIP 0x01
#define SPI_WEL 0x02

	while (flash_status() & SPI_WIP)
		;
}

void flash_write(uint32_t addr, const void * buf_ptr, uint8_t len)
{
	const uint8_t * buf = buf_ptr;

	while (flash_status() & SPI_WIP)
		;

	flash_wren();

	pin_write(SPI_FLASH_CS, 0);
	flash_write_byte(0x02);
	flash_write_byte((addr >> 16) & 0xFF);
	flash_write_byte((addr >>  8) & 0xFF);
	flash_write_byte((addr >>  0) & 0xFF);

	for(uint8_t i = 0 ; i < len ; i++)
		flash_write_byte(buf[i]);

	pin_write(SPI_FLASH_CS, 1);
}

