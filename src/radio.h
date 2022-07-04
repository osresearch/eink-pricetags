#ifndef _epd_radio_h_
#define _epd_radio_h_

#include <stdint.h>

void radio_init(uint8_t channel);
void radio_sleep(void);

void radio_tx(const uint8_t * dest, const uint8_t * buf, uint8_t len);

int8_t radio_rx(const uint8_t * id, uint8_t * buf, uint8_t max_len, uint8_t timeout);

#endif
