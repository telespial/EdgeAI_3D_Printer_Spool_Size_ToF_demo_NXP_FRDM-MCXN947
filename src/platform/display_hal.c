#include "platform/display_hal.h"

#include "par_lcd_s035.h"

bool display_hal_init(void)
{
    return par_lcd_s035_init();
}

void display_hal_fill(uint16_t rgb565)
{
    par_lcd_s035_fill(rgb565);
}

void display_hal_blit_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t *rgb565)
{
    par_lcd_s035_blit_rect(x0, y0, x1, y1, rgb565);
}

void display_hal_fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t rgb565)
{
    par_lcd_s035_fill_rect(x0, y0, x1, y1, rgb565);
}
