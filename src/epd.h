#ifndef _epd_epd_h_
#define _epd_epd_h_

#include <stdint.h>

#define EPD_WIDTH	122
#define EPD_HEIGHT	250

void epd_setup(void);
void epd_reset(void);

void epd_draw_full(void);

// windows must start and end with on a multiple of 8
void epd_draw_window(uint8_t x, uint16_t y, uint8_t w, uint16_t h);
void epd_data(const uint8_t data);
void epd_draw_end(void);

void epd_display(void);
void epd_shutdown(void);


#endif
