#pragma once

#include <stdbool.h>
#include <stdint.h>

bool par_lcd_s035_init(void);
void par_lcd_s035_fill(uint16_t rgb565);
void par_lcd_s035_blit_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t *rgb565);
void par_lcd_s035_fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t rgb565);
