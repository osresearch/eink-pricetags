#ifndef _epd_radio_h_
#define _epd_radio_h_

#include <stdint.h>

void radio_init(uint8_t channel);
void radio_sleep(void);

void radio_tx(uint32_t dest, const uint8_t * buf, uint8_t len);

int8_t radio_rx(uint32_t my_id, uint8_t * buf, uint8_t max_len, uint16_t timeout);

#endif
