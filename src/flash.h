#ifndef _epd_flash_h_
#define _epd_flash_h_

#include <stdint.h>

void flash_init(void);
void flash_read(uint32_t addr, uint8_t * buf, uint8_t len);

#endif
