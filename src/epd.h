#ifndef _epd_epd_h_
#define _epd_epd_h_

#include <stdint.h>

#define EPD_WIDTH	122
#define EPD_HEIGHT	250

void epd_setup(void);
void epd_reset(void);
void epd_init(void);
void epd_draw_start(void);
void epd_data(const uint8_t data);
void epd_draw_end(void);
void epd_shutdown(void);

#endif
