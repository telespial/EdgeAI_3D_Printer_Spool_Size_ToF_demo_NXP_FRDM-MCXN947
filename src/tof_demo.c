#include <stdbool.h>
#include <stdint.h>

#include "app.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"

#include "platform/display_hal.h"
#include "tmf8828_quick.h"

#define TOF_GRID_W 8
#define TOF_GRID_H 8

#define TOF_CELL_PX   34
#define TOF_CELL_GAP  4
#define TOF_FRAME_US  100000u

static inline uint16_t pack_rgb565(uint32_t r8, uint32_t g8, uint32_t b8)
{
    if (r8 > 255u) r8 = 255u;
    if (g8 > 255u) g8 = 255u;
    if (b8 > 255u) b8 = 255u;
    uint16_t r = (uint16_t)(r8 >> 3);
    uint16_t g = (uint16_t)(g8 >> 2);
    uint16_t b = (uint16_t)(b8 >> 3);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint16_t tof_color_from_mm(uint16_t mm)
{
    const uint32_t near_mm = 120u;
    const uint32_t far_mm = 2200u;

    uint32_t t;
    if (mm <= near_mm)
    {
        t = 0u;
    }
    else if (mm >= far_mm)
    {
        t = 255u;
    }
    else
    {
        t = (uint32_t)(((uint32_t)(mm - near_mm) * 255u) / (far_mm - near_mm));
    }

    uint32_t r;
    uint32_t g;
    uint32_t b;

    if (t < 64u)
    {
        uint32_t u = t * 4u;
        r = 0u;
        g = u;
        b = 255u;
    }
    else if (t < 128u)
    {
        uint32_t u = (t - 64u) * 4u;
        r = 0u;
        g = 255u;
        b = 255u - u;
    }
    else if (t < 192u)
    {
        uint32_t u = (t - 128u) * 4u;
        r = u;
        g = 255u;
        b = 0u;
    }
    else
    {
        uint32_t u = (t - 192u) * 4u;
        r = 255u;
        g = 255u - u;
        b = 0u;
    }

    return pack_rgb565(r, g, b);
}

static void tof_make_fallback_frame(uint16_t out_mm[64], uint32_t tick)
{
    for (uint32_t y = 0; y < TOF_GRID_H; y++)
    {
        for (uint32_t x = 0; x < TOF_GRID_W; x++)
        {
            uint32_t idx = y * TOF_GRID_W + x;
            uint32_t v = (x * 211u) + (y * 149u) + (tick * 47u);
            v = (v % 1800u) + 200u;
            out_mm[idx] = (uint16_t)v;
        }
    }
}

static void tof_draw_heatmap(const uint16_t mm[64], bool live_data)
{
    const int32_t grid_w_px = (TOF_GRID_W * TOF_CELL_PX) + ((TOF_GRID_W - 1) * TOF_CELL_GAP);
    const int32_t grid_h_px = (TOF_GRID_H * TOF_CELL_PX) + ((TOF_GRID_H - 1) * TOF_CELL_GAP);
    const int32_t x0 = (TOF_LCD_W - grid_w_px) / 2;
    const int32_t y0 = (TOF_LCD_H - grid_h_px) / 2;

    const uint16_t bg = pack_rgb565(2u, 2u, 3u);
    const uint16_t border = live_data ? pack_rgb565(8u, 10u, 12u) : pack_rgb565(30u, 6u, 6u);

    display_hal_fill(bg);

    for (int32_t gy = 0; gy < TOF_GRID_H; gy++)
    {
        for (int32_t gx = 0; gx < TOF_GRID_W; gx++)
        {
            const int32_t cell_x = x0 + gx * (TOF_CELL_PX + TOF_CELL_GAP);
            const int32_t cell_y = y0 + gy * (TOF_CELL_PX + TOF_CELL_GAP);
            const uint32_t idx = (uint32_t)(gy * TOF_GRID_W + gx);
            const uint16_t c = tof_color_from_mm(mm[idx]);

            display_hal_fill_rect(cell_x, cell_y,
                                  cell_x + TOF_CELL_PX - 1,
                                  cell_y + TOF_CELL_PX - 1,
                                  border);

            display_hal_fill_rect(cell_x + 2, cell_y + 2,
                                  cell_x + TOF_CELL_PX - 3,
                                  cell_y + TOF_CELL_PX - 3,
                                  c);
        }
    }
}

int main(void)
{
    BOARD_InitHardware();

    if (!display_hal_init())
    {
        for (;;) {}
    }

    display_hal_fill(pack_rgb565(0u, 0u, 0u));

    const bool tof_ok = tmf8828_quick_init();
    PRINTF("TOF demo: TMF8828 %s\r\n", tof_ok ? "ready" : "fallback mode");

    uint16_t frame_mm[64] = {0};
    uint32_t tick = 0u;

    for (;;)
    {
        bool got_live = false;
        if (tof_ok)
        {
            got_live = tmf8828_quick_read_8x8(frame_mm);
        }

        if (!got_live)
        {
            tof_make_fallback_frame(frame_mm, tick);
        }

        tof_draw_heatmap(frame_mm, got_live);

        tick++;
        SDK_DelayAtLeastUs(TOF_FRAME_US, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }
}
