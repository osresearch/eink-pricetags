#ifndef _epd_epd_h_
#define _epd_epd_h_

#include <stdint.h>

#define EPD_WIDTH	122
#define EPD_HEIGHT	250

void epd_setup(void);
void epd_reset(void);
void epd_init(void);
void epd_draw_start(void);
void epd_set_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void epd_data(const uint8_t data);
void epd_display(void);
void epd_shutdown(void);

#endif
