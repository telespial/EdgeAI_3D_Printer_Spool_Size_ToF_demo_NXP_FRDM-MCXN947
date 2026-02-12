#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"

#include "platform/display_hal.h"
#include "tmf8828_quick.h"

#define TOF_GRID_W 8
#define TOF_GRID_H 8

#define TOF_CELL_PX   34
#define TOF_CELL_GAP  4
#define TOF_CELL_INSET 2
#define TOF_STATUS_H 9
#define TOF_FRAME_US  12000u
#define TOF_LOCKED_NEAR_MM 40u
#define TOF_LOCKED_FAR_MM  140u
#define TOF_STALE_LIMIT_FRAMES 120u
#define TOF_RESTART_LIMIT_FRAMES 300u
#define TOF_REINIT_LIMIT_FRAMES 600u
#define TOF_ZERO_FRAME_RESTART_FRAMES 120u
#define TOF_ZERO_FRAME_RESTART_COOLDOWN_FRAMES 60u
#define TOF_ZERO_FRAME_REINIT_FRAMES 600u
#define TOF_INVALID_HOLD_FRAMES 48u
#define TOF_MEAN_FILL_MIN_VALID 6u
#define TOF_DISPLAY_HOLD_FRAMES 120u
#define TOF_FLAT_VALID_MIN      20u
#define TOF_FLAT_SPREAD_MM      220u
#define TOF_FLAT_CLAMP_MM       40u

typedef struct
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    int16_t ix0;
    int16_t iy0;
    int16_t ix1;
    int16_t iy1;
} tof_cell_rect_t;

static tof_cell_rect_t s_cells[64];
static uint16_t s_filtered_mm[64];
static uint16_t s_display_mm[64];
static uint16_t s_last_cell_color[64];
static bool s_cell_drawn[64];
static uint8_t s_invalid_age[64];
static uint8_t s_display_age[64];

static uint16_t s_ui_bg;
static uint16_t s_ui_border;
static uint16_t s_ui_status_live;
static uint16_t s_ui_status_fallback;
static uint16_t s_ui_hot;
static uint16_t s_ui_invalid;
static uint16_t s_ui_below_range;
static uint16_t s_ui_above_range;
static uint16_t s_last_status_color;
static bool s_status_drawn = false;
static int32_t s_hot_idx = -1;

static uint16_t s_range_near_mm = TOF_LOCKED_NEAR_MM;
static uint16_t s_range_far_mm = TOF_LOCKED_FAR_MM;

static bool tof_mm_valid(uint16_t mm)
{
    return (mm > 0u && mm < 12000u);
}

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

static uint16_t tof_color_from_mm(uint16_t mm, uint16_t near_mm, uint16_t far_mm)
{
    if (!tof_mm_valid(mm))
    {
        return s_ui_invalid;
    }

    if (far_mm <= near_mm)
    {
        far_mm = (uint16_t)(near_mm + 1u);
    }

    if (mm < near_mm)
    {
        return s_ui_below_range;
    }

    if (mm > far_mm)
    {
        return s_ui_above_range;
    }

    const uint32_t near = near_mm;
    const uint32_t far = far_mm;
    uint32_t t;
    if (mm <= near)
    {
        t = 0u;
    }
    else if (mm >= far)
    {
        t = 255u;
    }
    else
    {
        t = (uint32_t)(((uint32_t)(mm - near) * 255u) / (far - near));
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

static void tof_build_layout(void)
{
    const int32_t grid_w_px = (TOF_GRID_W * TOF_CELL_PX) + ((TOF_GRID_W - 1) * TOF_CELL_GAP);
    const int32_t grid_h_px = (TOF_GRID_H * TOF_CELL_PX) + ((TOF_GRID_H - 1) * TOF_CELL_GAP);
    const int32_t x0 = (TOF_LCD_W - grid_w_px) / 2;
    const int32_t y0 = (TOF_LCD_H - grid_h_px) / 2;

    for (int32_t gy = 0; gy < TOF_GRID_H; gy++)
    {
        for (int32_t gx = 0; gx < TOF_GRID_W; gx++)
        {
            const int32_t idx = gy * TOF_GRID_W + gx;
            const int32_t cell_x = x0 + gx * (TOF_CELL_PX + TOF_CELL_GAP);
            const int32_t cell_y = y0 + gy * (TOF_CELL_PX + TOF_CELL_GAP);

            s_cells[idx].x0 = (int16_t)cell_x;
            s_cells[idx].y0 = (int16_t)cell_y;
            s_cells[idx].x1 = (int16_t)(cell_x + TOF_CELL_PX - 1);
            s_cells[idx].y1 = (int16_t)(cell_y + TOF_CELL_PX - 1);
            s_cells[idx].ix0 = (int16_t)(cell_x + TOF_CELL_INSET);
            s_cells[idx].iy0 = (int16_t)(cell_y + TOF_CELL_INSET);
            s_cells[idx].ix1 = (int16_t)(cell_x + TOF_CELL_PX - TOF_CELL_INSET - 1);
            s_cells[idx].iy1 = (int16_t)(cell_y + TOF_CELL_PX - TOF_CELL_INSET - 1);
        }
    }
}

static void tof_draw_outline(const tof_cell_rect_t *c, uint16_t color)
{
    const int32_t x0 = c->ix0 - 1;
    const int32_t y0 = c->iy0 - 1;
    const int32_t x1 = c->ix1 + 1;
    const int32_t y1 = c->iy1 + 1;

    display_hal_fill_rect(x0, y0, x1, y0, color);
    display_hal_fill_rect(x0, y1, x1, y1, color);
    display_hal_fill_rect(x0, y0, x0, y1, color);
    display_hal_fill_rect(x1, y0, x1, y1, color);
}

static void tof_ui_init(void)
{
    s_ui_bg = pack_rgb565(3u, 4u, 6u);
    s_ui_border = pack_rgb565(16u, 18u, 22u);
    s_ui_status_live = pack_rgb565(8u, 120u, 20u);
    s_ui_status_fallback = pack_rgb565(180u, 18u, 12u);
    s_ui_hot = pack_rgb565(255u, 255u, 255u);
    s_ui_invalid = pack_rgb565(14u, 14u, 18u);
    s_ui_below_range = pack_rgb565(0u, 28u, 120u);
    s_ui_above_range = pack_rgb565(56u, 20u, 0u);

    tof_build_layout();
    display_hal_fill(s_ui_bg);
    display_hal_fill_rect(0, TOF_STATUS_H, TOF_LCD_W - 1, TOF_STATUS_H, pack_rgb565(36u, 36u, 40u));

    for (uint32_t i = 0; i < 64u; i++)
    {
        const tof_cell_rect_t *c = &s_cells[i];
        display_hal_fill_rect(c->x0, c->y0, c->x1, c->y1, s_ui_border);
        display_hal_fill_rect(c->ix0, c->iy0, c->ix1, c->iy1, s_ui_bg);
    }

    memset(s_filtered_mm, 0, sizeof(s_filtered_mm));
    memset(s_display_mm, 0, sizeof(s_display_mm));
    memset(s_last_cell_color, 0, sizeof(s_last_cell_color));
    memset(s_cell_drawn, 0, sizeof(s_cell_drawn));
    memset(s_invalid_age, 0, sizeof(s_invalid_age));
    memset(s_display_age, 0, sizeof(s_display_age));
    s_status_drawn = false;
    s_hot_idx = -1;
    s_range_near_mm = TOF_LOCKED_NEAR_MM;
    s_range_far_mm = TOF_LOCKED_FAR_MM;
}

static void tof_update_status_bar(bool live_data)
{
    const uint16_t status = live_data ? s_ui_status_live : s_ui_status_fallback;
    if (!s_status_drawn || status != s_last_status_color)
    {
        display_hal_fill_rect(0, 0, TOF_LCD_W - 1, TOF_STATUS_H - 1, status);
        s_last_status_color = status;
        s_status_drawn = true;
    }
}

static void tof_filter_frame(const uint16_t in_mm[64], bool live_data)
{
    for (uint32_t i = 0; i < 64u; i++)
    {
        uint16_t sample = in_mm[i];

        if (!live_data)
        {
            s_filtered_mm[i] = sample;
            s_invalid_age[i] = 0u;
            continue;
        }

        if (!tof_mm_valid(sample))
        {
            if (s_filtered_mm[i] > 0u)
            {
                if (s_invalid_age[i] < TOF_INVALID_HOLD_FRAMES)
                {
                    s_invalid_age[i]++;
                }
                else
                {
                    s_filtered_mm[i] = (uint16_t)(((uint32_t)s_filtered_mm[i] * 31u) / 32u);
                }
            }
            continue;
        }

        s_invalid_age[i] = 0u;
        if (s_filtered_mm[i] == 0u)
        {
            s_filtered_mm[i] = sample;
        }
        else
        {
            s_filtered_mm[i] = (uint16_t)(((uint32_t)s_filtered_mm[i] + sample + 1u) / 2u);
        }
    }
}

static void tof_spatial_postprocess(uint16_t mm[64], bool live_data)
{
    if (!live_data)
    {
        return;
    }

    uint16_t src[64];
    memcpy(src, mm, sizeof(src));

    for (uint32_t y = 0u; y < TOF_GRID_H; y++)
    {
        for (uint32_t x = 0u; x < TOF_GRID_W; x++)
        {
            const uint32_t idx = (y * TOF_GRID_W) + x;
            if (tof_mm_valid(src[idx]))
            {
                continue;
            }

            uint32_t sum = 0u;
            uint32_t count = 0u;
            for (int32_t dy = -1; dy <= 1; dy++)
            {
                for (int32_t dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0)
                    {
                        continue;
                    }

                    const int32_t nx = (int32_t)x + dx;
                    const int32_t ny = (int32_t)y + dy;
                    if (nx < 0 || nx >= TOF_GRID_W || ny < 0 || ny >= TOF_GRID_H)
                    {
                        continue;
                    }

                    const uint16_t v = src[(uint32_t)ny * TOF_GRID_W + (uint32_t)nx];
                    if (tof_mm_valid(v))
                    {
                        sum += v;
                        count++;
                    }
                }
            }

            if (count >= 1u)
            {
                mm[idx] = (uint16_t)(sum / count);
            }
        }
    }

    uint32_t valid_count = 0u;
    uint32_t sum = 0u;
    uint16_t min_mm = 0xFFFFu;
    uint16_t max_mm = 0u;
    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t v = mm[i];
        if (tof_mm_valid(v))
        {
            valid_count++;
            sum += v;
            if (v < min_mm)
            {
                min_mm = v;
            }
            if (v > max_mm)
            {
                max_mm = v;
            }
        }
    }

    if (valid_count >= TOF_FLAT_VALID_MIN)
    {
        const uint16_t mean_mm = (uint16_t)(sum / valid_count);
        const uint16_t spread_mm = (uint16_t)(max_mm - min_mm);
        if (spread_mm <= TOF_FLAT_SPREAD_MM)
        {
            const uint16_t lo = (mean_mm > TOF_FLAT_CLAMP_MM) ? (uint16_t)(mean_mm - TOF_FLAT_CLAMP_MM) : 0u;
            const uint16_t hi = (uint16_t)(mean_mm + TOF_FLAT_CLAMP_MM);
            for (uint32_t i = 0u; i < 64u; i++)
            {
                uint16_t v = mm[i];
                if (!tof_mm_valid(v))
                {
                    mm[i] = mean_mm;
                    continue;
                }

                if (v < lo)
                {
                    v = lo;
                }
                else if (v > hi)
                {
                    v = hi;
                }
                mm[i] = v;
            }
        }
    }
}

static void tof_compose_display_frame(bool live_data)
{
    if (!live_data)
    {
        memcpy(s_display_mm, s_filtered_mm, sizeof(s_display_mm));
        memset(s_display_age, 0, sizeof(s_display_age));
        return;
    }

    uint16_t src[64];
    memcpy(src, s_filtered_mm, sizeof(src));

    uint32_t valid_count = 0u;
    uint32_t sum = 0u;
    uint16_t min_mm = 0xFFFFu;
    uint16_t max_mm = 0u;
    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t v = src[i];
        if (tof_mm_valid(v))
        {
            valid_count++;
            sum += v;
            if (v < min_mm)
            {
                min_mm = v;
            }
            if (v > max_mm)
            {
                max_mm = v;
            }
        }
    }

    const uint16_t mean_mm = (valid_count > 0u) ? (uint16_t)(sum / valid_count) : 0u;

    for (uint32_t y = 0u; y < TOF_GRID_H; y++)
    {
        for (uint32_t x = 0u; x < TOF_GRID_W; x++)
        {
            const uint32_t idx = (y * TOF_GRID_W) + x;
            if (tof_mm_valid(src[idx]))
            {
                continue;
            }

            uint32_t nsum = 0u;
            uint32_t ncount = 0u;
            for (int32_t dy = -1; dy <= 1; dy++)
            {
                for (int32_t dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0)
                    {
                        continue;
                    }

                    const int32_t nx = (int32_t)x + dx;
                    const int32_t ny = (int32_t)y + dy;
                    if (nx < 0 || nx >= TOF_GRID_W || ny < 0 || ny >= TOF_GRID_H)
                    {
                        continue;
                    }

                    const uint16_t v = src[(uint32_t)ny * TOF_GRID_W + (uint32_t)nx];
                    if (tof_mm_valid(v))
                    {
                        nsum += v;
                        ncount++;
                    }
                }
            }

            if (ncount > 0u)
            {
                src[idx] = (uint16_t)(nsum / ncount);
            }
            else if (valid_count >= TOF_MEAN_FILL_MIN_VALID)
            {
                src[idx] = mean_mm;
            }
        }
    }

    valid_count = 0u;
    sum = 0u;
    min_mm = 0xFFFFu;
    max_mm = 0u;
    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t v = src[i];
        if (tof_mm_valid(v))
        {
            valid_count++;
            sum += v;
            if (v < min_mm)
            {
                min_mm = v;
            }
            if (v > max_mm)
            {
                max_mm = v;
            }
        }
    }

    if (valid_count > 0u)
    {
        const uint16_t flat_mean_mm = (uint16_t)(sum / valid_count);
        const uint16_t spread_mm = (uint16_t)(max_mm - min_mm);
        if (valid_count >= TOF_FLAT_VALID_MIN && spread_mm <= TOF_FLAT_SPREAD_MM)
        {
            const uint16_t lo = (flat_mean_mm > TOF_FLAT_CLAMP_MM) ? (uint16_t)(flat_mean_mm - TOF_FLAT_CLAMP_MM) : 0u;
            const uint16_t hi = (uint16_t)(flat_mean_mm + TOF_FLAT_CLAMP_MM);

            for (uint32_t i = 0u; i < 64u; i++)
            {
                uint16_t v = src[i];
                if (!tof_mm_valid(v))
                {
                    src[i] = flat_mean_mm;
                    continue;
                }

                if (v < lo)
                {
                    v = lo;
                }
                else if (v > hi)
                {
                    v = hi;
                }

                /* Pull each valid cell toward the frame mean for flatter surfaces. */
                src[i] = (uint16_t)((((uint32_t)v * 2u) + flat_mean_mm + 1u) / 3u);
            }
        }
    }

    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t v = src[i];
        if (tof_mm_valid(v))
        {
            if (tof_mm_valid(s_display_mm[i]))
            {
                s_display_mm[i] = (uint16_t)((((uint32_t)s_display_mm[i] * 2u) + v + 1u) / 3u);
            }
            else
            {
                s_display_mm[i] = v;
            }
            s_display_age[i] = 0u;
            continue;
        }

        if (tof_mm_valid(s_display_mm[i]) && s_display_age[i] < TOF_DISPLAY_HOLD_FRAMES)
        {
            s_display_age[i]++;
            continue;
        }

        if (tof_mm_valid(s_display_mm[i]))
        {
            s_display_mm[i] = (uint16_t)(((uint32_t)s_display_mm[i] * 63u) / 64u);
            if (s_display_mm[i] < 8u)
            {
                s_display_mm[i] = 0u;
            }
        }
    }

    uint16_t smooth[64];
    memcpy(smooth, s_display_mm, sizeof(smooth));
    for (uint32_t y = 0u; y < TOF_GRID_H; y++)
    {
        for (uint32_t x = 0u; x < TOF_GRID_W; x++)
        {
            const uint32_t idx = (y * TOF_GRID_W) + x;
            uint32_t ssum = 0u;
            uint32_t scount = 0u;
            for (int32_t dy = -1; dy <= 1; dy++)
            {
                for (int32_t dx = -1; dx <= 1; dx++)
                {
                    const int32_t nx = (int32_t)x + dx;
                    const int32_t ny = (int32_t)y + dy;
                    if (nx < 0 || nx >= TOF_GRID_W || ny < 0 || ny >= TOF_GRID_H)
                    {
                        continue;
                    }

                    const uint16_t v = s_display_mm[(uint32_t)ny * TOF_GRID_W + (uint32_t)nx];
                    if (tof_mm_valid(v))
                    {
                        ssum += v;
                        scount++;
                    }
                }
            }

            if (scount >= 2u)
            {
                smooth[idx] = (uint16_t)(ssum / scount);
            }
        }
    }
    memcpy(s_display_mm, smooth, sizeof(s_display_mm));
}

static uint32_t tof_count_valid(const uint16_t mm[64])
{
    uint32_t valid = 0u;
    for (uint32_t i = 0; i < 64u; i++)
    {
        const uint16_t v = mm[i];
        if (tof_mm_valid(v))
        {
            valid++;
        }
    }
    return valid;
}

static void tof_decay_frame(uint16_t mm[64])
{
    for (uint32_t i = 0; i < 64u; i++)
    {
        mm[i] = (uint16_t)(((uint32_t)mm[i] * 7u) / 8u);
    }
}

static void tof_update_range(const uint16_t mm[64], bool live_data)
{
    (void)mm;
    (void)live_data;
    s_range_near_mm = TOF_LOCKED_NEAR_MM;
    s_range_far_mm = TOF_LOCKED_FAR_MM;
}

static int32_t tof_find_closest_idx(const uint16_t mm[64])
{
    int32_t best_idx = -1;
    uint16_t best_mm = 0xFFFFu;

    for (uint32_t i = 0; i < 64u; i++)
    {
        const uint16_t v = mm[i];
        if (v >= TOF_LOCKED_NEAR_MM && v <= TOF_LOCKED_FAR_MM && v < best_mm)
        {
            best_mm = v;
            best_idx = (int32_t)i;
        }
    }

    return best_idx;
}

static void tof_update_hotspot(const uint16_t mm[64], bool live_data)
{
    const int32_t new_hot = live_data ? tof_find_closest_idx(mm) : -1;
    if (new_hot == s_hot_idx)
    {
        return;
    }

    if (s_hot_idx >= 0)
    {
        tof_draw_outline(&s_cells[s_hot_idx], s_ui_border);
    }

    s_hot_idx = new_hot;
    if (s_hot_idx >= 0)
    {
        tof_draw_outline(&s_cells[s_hot_idx], s_ui_hot);
    }
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

static void tof_draw_heatmap_incremental(const uint16_t mm[64], bool live_data)
{
    tof_update_status_bar(live_data);
    tof_filter_frame(mm, live_data);
    tof_spatial_postprocess(s_filtered_mm, live_data);
    tof_compose_display_frame(live_data);
    tof_update_range(s_display_mm, live_data);

    for (uint32_t idx = 0; idx < 64u; idx++)
    {
        const tof_cell_rect_t *c = &s_cells[idx];
        const uint16_t color = tof_color_from_mm(s_display_mm[idx], s_range_near_mm, s_range_far_mm);

        if (!s_cell_drawn[idx] || s_last_cell_color[idx] != color)
        {
            display_hal_fill_rect(c->ix0, c->iy0, c->ix1, c->iy1, color);
            s_last_cell_color[idx] = color;
            s_cell_drawn[idx] = true;
        }
    }

    tof_update_hotspot(s_display_mm, live_data);
}

int main(void)
{
    BOARD_InitHardware();

    if (!display_hal_init())
    {
        for (;;) {}
    }

    bool tof_ok = tmf8828_quick_init();
    PRINTF("TOF demo: TMF8828 %s\r\n", tof_ok ? "ready" : "fallback mode");

    tof_ui_init();

    uint16_t frame_mm[64] = {0};
    uint32_t tick = 0u;
    bool have_live = false;
    uint32_t stale_frames = 0u;
    bool printed_live_once = false;
    bool printed_timeout_once = false;
    bool restart_attempted = false;
    uint32_t zero_live_frames = 0u;
    uint32_t zero_restart_cooldown = 0u;
    bool had_nonzero_frame = false;

    for (;;)
    {
        bool got_live = false;
        bool got_complete = false;
        if (tof_ok)
        {
            got_live = tmf8828_quick_read_8x8(frame_mm, &got_complete);
        }

        if (got_live)
        {
            const uint32_t valid_count = tof_count_valid(frame_mm);
            have_live = true;
            stale_frames = 0u;
            printed_timeout_once = false;
            restart_attempted = false;

            if (valid_count == 0u)
            {
                if (had_nonzero_frame)
                {
                    if (zero_live_frames < 1000000u)
                    {
                        zero_live_frames++;
                    }
                }
                else
                {
                    zero_live_frames = 0u;
                }

                if (zero_restart_cooldown > 0u)
                {
                    zero_restart_cooldown--;
                }
            }
            else
            {
                had_nonzero_frame = true;
                zero_live_frames = 0u;
                zero_restart_cooldown = 0u;
            }

            if (!printed_live_once)
            {
                PRINTF("TOF demo: live 8x8 frames detected\r\n");
                printed_live_once = true;
            }

            if (tof_ok &&
                zero_live_frames >= TOF_ZERO_FRAME_RESTART_FRAMES &&
                zero_restart_cooldown == 0u)
            {
                PRINTF("TOF demo: zero-valid frame streak, restarting stream\r\n");
                if (!tmf8828_quick_restart_measurement())
                {
                    PRINTF("TOF demo: zero-valid restart failed\r\n");
                }
                zero_restart_cooldown = TOF_ZERO_FRAME_RESTART_COOLDOWN_FRAMES;
            }

            if (tof_ok &&
                zero_live_frames >= TOF_ZERO_FRAME_REINIT_FRAMES)
            {
                PRINTF("TOF demo: persistent zero-valid frames, reinitializing sensor\r\n");
                tof_ok = tmf8828_quick_init();
                PRINTF("TOF demo: TMF8828 %s\r\n", tof_ok ? "ready" : "fallback mode");
                stale_frames = 0u;
                printed_live_once = false;
                have_live = false;
                restart_attempted = false;
                zero_live_frames = 0u;
                zero_restart_cooldown = 0u;
                had_nonzero_frame = false;
            }
        }
        else
        {
            if (stale_frames < TOF_REINIT_LIMIT_FRAMES)
            {
                stale_frames++;
            }

            if (stale_frames >= TOF_STALE_LIMIT_FRAMES)
            {
                if (have_live)
                {
                    PRINTF("TOF demo: live stream timeout, waiting for stream\r\n");
                }
                have_live = false;
                if (!printed_timeout_once)
                {
                    printed_timeout_once = true;
                }
            }

            if (tof_ok && stale_frames >= TOF_RESTART_LIMIT_FRAMES && !restart_attempted)
            {
                restart_attempted = true;
                if (!tmf8828_quick_restart_measurement())
                {
                    PRINTF("TOF demo: stream restart failed\r\n");
                }
            }
        }

        bool draw_now = false;

        if (!tof_ok)
        {
            tof_make_fallback_frame(frame_mm, tick);
            draw_now = true;
        }
        else if (!have_live)
        {
            tof_decay_frame(frame_mm);
            draw_now = true;
        }
        else if (got_live || got_complete)
        {
            draw_now = true;
        }

        if (tof_ok && stale_frames >= TOF_REINIT_LIMIT_FRAMES)
        {
            PRINTF("TOF demo: prolonged timeout, reinitializing sensor\r\n");
            tof_ok = tmf8828_quick_init();
            PRINTF("TOF demo: TMF8828 %s\r\n", tof_ok ? "ready" : "fallback mode");
            stale_frames = 0u;
            printed_live_once = false;
            have_live = false;
            restart_attempted = false;
            zero_live_frames = 0u;
            zero_restart_cooldown = 0u;
            had_nonzero_frame = false;
        }

        if (draw_now)
        {
            tof_draw_heatmap_incremental(frame_mm, have_live);
        }

        tick++;
        SDK_DelayAtLeastUs(TOF_FRAME_US, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }
}
