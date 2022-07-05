#ifndef _epd_flash_h_
#define _epd_flash_h_

#include <stdint.h>

void flash_init(void);
void flash_erase(uint32_t addr);
void flash_read(uint32_t addr, void * buf, uint8_t len);
void flash_write(uint32_t addr, const void * buf, uint8_t len);

#endif
