#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "app.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"

#include "platform/display_hal.h"
#include "tmf8828_quick.h"

#define TOF_GRID_W 8
#define TOF_GRID_H 8

#define TOF_CELL_PX   9
#define TOF_CELL_GAP  1
#define TOF_CELL_INSET 1
#define TOF_STATUS_H 0
#define TOF_Q_W (TOF_LCD_W / 3)
#define TOF_Q1_X0 0
#define TOF_Q1_X1 (TOF_Q_W - 1)
#define TOF_Q1_TOP_Y0 0
#define TOF_Q1_TOP_Y1 ((TOF_LCD_H / 2) - 1)
#define TOF_Q1_BOT_Y0 (TOF_Q1_TOP_Y1 + 1)
#define TOF_Q1_BOT_Y1 (TOF_LCD_H - 1)
#define TOF_TP_X0 (TOF_Q_W + 1)
#define TOF_TP_X1 (TOF_LCD_W - 1)
#define TOF_TP_Y0 0
#define TOF_TP_Y1 (TOF_LCD_H - 1)
#define TOF_LOCKED_NEAR_MM 50u
#define TOF_LOCKED_FAR_MM  150u
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

#ifndef TOF_DEBUG_RAW_DRAW
#define TOF_DEBUG_RAW_DRAW 1
#endif

#if TOF_DEBUG_RAW_DRAW
#define TOF_FRAME_US  2000u
#else
#define TOF_FRAME_US  12000u
#endif

#if TOF_DEBUG_RAW_DRAW
#define TOF_ENABLE_AUTO_RECOVERY 0u
#else
#define TOF_ENABLE_AUTO_RECOVERY 1u
#endif

#define TOF_READ_BURST_MAX 8u
#define TOF_DRAW_ON_COMPLETE_ONLY 1u
#define TOF_USE_DYNAMIC_RANGE 0u
#define TOF_RESPONSE_TARGET_US 500000u
#define TOF_DEBUG_UPDATE_US 500000u
#define TOF_DEBUG_UPDATE_TICKS_RAW ((TOF_DEBUG_UPDATE_US + TOF_FRAME_US - 1u) / TOF_FRAME_US)
#define TOF_DEBUG_UPDATE_TICKS ((TOF_DEBUG_UPDATE_TICKS_RAW > 0u) ? TOF_DEBUG_UPDATE_TICKS_RAW : 1u)
#define TOF_MAX_DRAW_GAP_TICKS_RAW ((TOF_RESPONSE_TARGET_US + TOF_FRAME_US - 1u) / TOF_FRAME_US)
#define TOF_MAX_DRAW_GAP_TICKS ((TOF_MAX_DRAW_GAP_TICKS_RAW > 0u) ? TOF_MAX_DRAW_GAP_TICKS_RAW : 1u)
#define TOF_TP_UPDATE_US 100000u
#define TOF_TP_UPDATE_TICKS_RAW ((TOF_TP_UPDATE_US + TOF_FRAME_US - 1u) / TOF_FRAME_US)
#define TOF_TP_UPDATE_TICKS ((TOF_TP_UPDATE_TICKS_RAW > 0u) ? TOF_TP_UPDATE_TICKS_RAW : 1u)

#define TOF_DBG_SCALE 2
#define TOF_DBG_CHAR_W 3
#define TOF_DBG_CHAR_H 5
#define TOF_DBG_CHAR_ADV ((TOF_DBG_CHAR_W * TOF_DBG_SCALE) + 2)
#define TOF_DBG_LINE_H ((TOF_DBG_CHAR_H * TOF_DBG_SCALE) + 2)
#define TOF_DBG_LINES 6u
#define TOF_DBG_COLS 19u

#define TOF_TP_MM_FULL_NEAR 50u
#define TOF_TP_MM_EMPTY_FAR 100u

#ifndef TOF_AI_DATA_LOG_ENABLE
#define TOF_AI_DATA_LOG_ENABLE 0u
#endif
#ifndef TOF_AI_DATA_LOG_FULL_FRAME
#define TOF_AI_DATA_LOG_FULL_FRAME 0u
#endif
#define TOF_AI_DATA_LOG_INTERVAL_TICKS TOF_TP_UPDATE_TICKS

#define TOF_INPUT_MODE_LIVE          0u
#define TOF_INPUT_MODE_SYNTH_FIXED   1u
#define TOF_INPUT_MODE_SYNTH_SUBCAP  2u

#ifndef TOF_DEBUG_INPUT_MODE
#define TOF_DEBUG_INPUT_MODE TOF_INPUT_MODE_LIVE
#endif

#ifndef TOF_SYNTH_MAP_MODE
#define TOF_SYNTH_MAP_MODE 0u
#endif

#ifndef TOF_SYNTH_TRACE_EVERY_COMPLETE
#define TOF_SYNTH_TRACE_EVERY_COMPLETE 12u
#endif

#if defined(__GNUC__)
#define TOF_UNUSED __attribute__((unused))
#else
#define TOF_UNUSED
#endif

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
static uint16_t s_ui_hot;
static uint16_t s_ui_invalid;
static uint16_t s_ui_below_range;
static uint16_t s_ui_above_range;
static int32_t s_hot_idx = -1;
static uint16_t s_ui_dbg_bg;
static uint16_t s_ui_dbg_fg;
static uint16_t s_ui_dbg_dim;
static int16_t s_dbg_x0;
static int16_t s_dbg_y0;
static int16_t s_dbg_x1;
static int16_t s_dbg_y1;
static bool s_dbg_force_redraw = true;
static char s_dbg_prev[TOF_DBG_LINES][TOF_DBG_COLS + 1u];
static uint32_t s_dbg_last_tick = 0u;
static bool s_tp_force_redraw = true;
static uint32_t s_tp_last_tick = 0u;
static uint16_t s_tp_last_outer_ry = 0u;
static uint16_t s_tp_last_fullness_q10 = 0u;
static uint32_t s_tp_mm_q8 = 0u;
static bool s_tp_last_live = false;
static bool s_tp_prev_rect_valid = false;
static int16_t s_tp_prev_x0 = 0;
static int16_t s_tp_prev_y0 = 0;
static int16_t s_tp_prev_x1 = 0;
static int16_t s_tp_prev_y1 = 0;
static uint32_t s_ai_log_last_tick = 0u;

static uint16_t s_range_near_mm = TOF_LOCKED_NEAR_MM;
static uint16_t s_range_far_mm = TOF_LOCKED_FAR_MM;
static uint16_t s_synth_subcap_frame[64];
static uint8_t s_synth_subcap_capture = 0u;

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

static void tof_fill_display_holes(const uint16_t in_mm[64], uint16_t out_mm[64])
{
    memcpy(out_mm, in_mm, sizeof(uint16_t) * 64u);

    for (uint32_t pass = 0u; pass < 2u; pass++)
    {
        uint16_t src[64];
        memcpy(src, out_mm, sizeof(src));

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

                if (count > 0u)
                {
                    out_mm[idx] = (uint16_t)(sum / count);
                }
            }
        }
    }
}

static int32_t tof_clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo)
    {
        return lo;
    }
    if (v > hi)
    {
        return hi;
    }
    return v;
}

static void tof_dbg_get_glyph(char ch, uint8_t rows[TOF_DBG_CHAR_H])
{
    if (ch >= 'a' && ch <= 'z')
    {
        ch = (char)(ch - ('a' - 'A'));
    }

    switch (ch)
    {
        case '0': rows[0] = 0x7; rows[1] = 0x5; rows[2] = 0x5; rows[3] = 0x5; rows[4] = 0x7; break;
        case '1': rows[0] = 0x2; rows[1] = 0x6; rows[2] = 0x2; rows[3] = 0x2; rows[4] = 0x7; break;
        case '2': rows[0] = 0x7; rows[1] = 0x1; rows[2] = 0x7; rows[3] = 0x4; rows[4] = 0x7; break;
        case '3': rows[0] = 0x7; rows[1] = 0x1; rows[2] = 0x7; rows[3] = 0x1; rows[4] = 0x7; break;
        case '4': rows[0] = 0x5; rows[1] = 0x5; rows[2] = 0x7; rows[3] = 0x1; rows[4] = 0x1; break;
        case '5': rows[0] = 0x7; rows[1] = 0x4; rows[2] = 0x7; rows[3] = 0x1; rows[4] = 0x7; break;
        case '6': rows[0] = 0x7; rows[1] = 0x4; rows[2] = 0x7; rows[3] = 0x5; rows[4] = 0x7; break;
        case '7': rows[0] = 0x7; rows[1] = 0x1; rows[2] = 0x2; rows[3] = 0x2; rows[4] = 0x2; break;
        case '8': rows[0] = 0x7; rows[1] = 0x5; rows[2] = 0x7; rows[3] = 0x5; rows[4] = 0x7; break;
        case '9': rows[0] = 0x7; rows[1] = 0x5; rows[2] = 0x7; rows[3] = 0x1; rows[4] = 0x7; break;
        case 'A': rows[0] = 0x2; rows[1] = 0x5; rows[2] = 0x7; rows[3] = 0x5; rows[4] = 0x5; break;
        case 'B': rows[0] = 0x6; rows[1] = 0x5; rows[2] = 0x6; rows[3] = 0x5; rows[4] = 0x6; break;
        case 'C': rows[0] = 0x3; rows[1] = 0x4; rows[2] = 0x4; rows[3] = 0x4; rows[4] = 0x3; break;
        case 'D': rows[0] = 0x6; rows[1] = 0x5; rows[2] = 0x5; rows[3] = 0x5; rows[4] = 0x6; break;
        case 'E': rows[0] = 0x7; rows[1] = 0x4; rows[2] = 0x6; rows[3] = 0x4; rows[4] = 0x7; break;
        case 'F': rows[0] = 0x7; rows[1] = 0x4; rows[2] = 0x6; rows[3] = 0x4; rows[4] = 0x4; break;
        case 'G': rows[0] = 0x3; rows[1] = 0x4; rows[2] = 0x5; rows[3] = 0x5; rows[4] = 0x3; break;
        case 'H': rows[0] = 0x5; rows[1] = 0x5; rows[2] = 0x7; rows[3] = 0x5; rows[4] = 0x5; break;
        case 'I': rows[0] = 0x7; rows[1] = 0x2; rows[2] = 0x2; rows[3] = 0x2; rows[4] = 0x7; break;
        case 'J': rows[0] = 0x1; rows[1] = 0x1; rows[2] = 0x1; rows[3] = 0x5; rows[4] = 0x2; break;
        case 'K': rows[0] = 0x5; rows[1] = 0x5; rows[2] = 0x6; rows[3] = 0x5; rows[4] = 0x5; break;
        case 'L': rows[0] = 0x4; rows[1] = 0x4; rows[2] = 0x4; rows[3] = 0x4; rows[4] = 0x7; break;
        case 'M': rows[0] = 0x5; rows[1] = 0x7; rows[2] = 0x7; rows[3] = 0x5; rows[4] = 0x5; break;
        case 'N': rows[0] = 0x5; rows[1] = 0x7; rows[2] = 0x7; rows[3] = 0x7; rows[4] = 0x5; break;
        case 'O': rows[0] = 0x2; rows[1] = 0x5; rows[2] = 0x5; rows[3] = 0x5; rows[4] = 0x2; break;
        case 'P': rows[0] = 0x6; rows[1] = 0x5; rows[2] = 0x6; rows[3] = 0x4; rows[4] = 0x4; break;
        case 'Q': rows[0] = 0x2; rows[1] = 0x5; rows[2] = 0x5; rows[3] = 0x2; rows[4] = 0x1; break;
        case 'R': rows[0] = 0x6; rows[1] = 0x5; rows[2] = 0x6; rows[3] = 0x5; rows[4] = 0x5; break;
        case 'S': rows[0] = 0x3; rows[1] = 0x4; rows[2] = 0x2; rows[3] = 0x1; rows[4] = 0x6; break;
        case 'T': rows[0] = 0x7; rows[1] = 0x2; rows[2] = 0x2; rows[3] = 0x2; rows[4] = 0x2; break;
        case 'U': rows[0] = 0x5; rows[1] = 0x5; rows[2] = 0x5; rows[3] = 0x5; rows[4] = 0x7; break;
        case 'V': rows[0] = 0x5; rows[1] = 0x5; rows[2] = 0x5; rows[3] = 0x5; rows[4] = 0x2; break;
        case 'W': rows[0] = 0x5; rows[1] = 0x5; rows[2] = 0x7; rows[3] = 0x7; rows[4] = 0x5; break;
        case 'X': rows[0] = 0x5; rows[1] = 0x5; rows[2] = 0x2; rows[3] = 0x5; rows[4] = 0x5; break;
        case 'Y': rows[0] = 0x5; rows[1] = 0x5; rows[2] = 0x2; rows[3] = 0x2; rows[4] = 0x2; break;
        case 'Z': rows[0] = 0x7; rows[1] = 0x1; rows[2] = 0x2; rows[3] = 0x4; rows[4] = 0x7; break;
        case ':': rows[0] = 0x0; rows[1] = 0x2; rows[2] = 0x0; rows[3] = 0x2; rows[4] = 0x0; break;
        case '.': rows[0] = 0x0; rows[1] = 0x0; rows[2] = 0x0; rows[3] = 0x0; rows[4] = 0x2; break;
        case '-': rows[0] = 0x0; rows[1] = 0x0; rows[2] = 0x7; rows[3] = 0x0; rows[4] = 0x0; break;
        case '/': rows[0] = 0x1; rows[1] = 0x1; rows[2] = 0x2; rows[3] = 0x4; rows[4] = 0x4; break;
        case '=': rows[0] = 0x0; rows[1] = 0x7; rows[2] = 0x0; rows[3] = 0x7; rows[4] = 0x0; break;
        case ' ': rows[0] = 0x0; rows[1] = 0x0; rows[2] = 0x0; rows[3] = 0x0; rows[4] = 0x0; break;
        default:  rows[0] = 0x7; rows[1] = 0x1; rows[2] = 0x2; rows[3] = 0x0; rows[4] = 0x2; break;
    }
}

static void tof_dbg_draw_char(int32_t x, int32_t y, char ch, uint16_t color)
{
    uint8_t rows[TOF_DBG_CHAR_H];
    tof_dbg_get_glyph(ch, rows);

    for (uint32_t ry = 0u; ry < TOF_DBG_CHAR_H; ry++)
    {
        for (uint32_t rx = 0u; rx < TOF_DBG_CHAR_W; rx++)
        {
            if (((rows[ry] >> (TOF_DBG_CHAR_W - 1u - rx)) & 0x1u) == 0u)
            {
                continue;
            }

            const int32_t px0 = x + (int32_t)(rx * TOF_DBG_SCALE);
            const int32_t py0 = y + (int32_t)(ry * TOF_DBG_SCALE);
            const int32_t px1 = px0 + TOF_DBG_SCALE - 1;
            const int32_t py1 = py0 + TOF_DBG_SCALE - 1;
            if (px1 < s_dbg_x0 || px0 > s_dbg_x1 || py1 < s_dbg_y0 || py0 > s_dbg_y1)
            {
                continue;
            }
            display_hal_fill_rect(px0, py0, px1, py1, color);
        }
    }
}

static void tof_dbg_draw_line(uint32_t line_idx, const char *text, uint16_t color)
{
    if (line_idx >= TOF_DBG_LINES)
    {
        return;
    }

    char clipped[TOF_DBG_COLS + 1u];
    size_t n = strlen(text);
    if (n > TOF_DBG_COLS)
    {
        n = TOF_DBG_COLS;
    }
    memcpy(clipped, text, n);
    clipped[n] = '\0';

    if (!s_dbg_force_redraw && strcmp(clipped, s_dbg_prev[line_idx]) == 0)
    {
        return;
    }

    strcpy(s_dbg_prev[line_idx], clipped);

    const int32_t y = s_dbg_y0 + 3 + (int32_t)(line_idx * TOF_DBG_LINE_H);
    const int32_t line_h = (TOF_DBG_CHAR_H * TOF_DBG_SCALE);
    display_hal_fill_rect(s_dbg_x0 + 2, y, s_dbg_x1 - 2, y + line_h, s_ui_dbg_bg);

    int32_t x = s_dbg_x0 + 4;
    for (size_t i = 0u; i < n; i++)
    {
        if ((x + (TOF_DBG_CHAR_W * TOF_DBG_SCALE)) > (s_dbg_x1 - 2))
        {
            break;
        }
        tof_dbg_draw_char(x, y, clipped[i], color);
        x += TOF_DBG_CHAR_ADV;
    }
}

static void tof_calc_frame_stats(const uint16_t mm[64],
                                 uint32_t *valid_count,
                                 uint16_t *min_mm,
                                 uint16_t *max_mm,
                                 uint16_t *avg_mm)
{
    uint32_t valid = 0u;
    uint32_t sum = 0u;
    uint16_t min_v = 0xFFFFu;
    uint16_t max_v = 0u;

    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t v = mm[i];
        if (!tof_mm_valid(v))
        {
            continue;
        }
        valid++;
        sum += v;
        if (v < min_v)
        {
            min_v = v;
        }
        if (v > max_v)
        {
            max_v = v;
        }
    }

    if (valid_count) *valid_count = valid;
    if (min_mm) *min_mm = (valid > 0u) ? min_v : 0u;
    if (max_mm) *max_mm = (valid > 0u) ? max_v : 0u;
    if (avg_mm) *avg_mm = (valid > 0u) ? (uint16_t)(sum / valid) : 0u;
}

static uint16_t tof_calc_actual_distance_mm(const uint16_t mm[64])
{
    uint16_t closest_in_range = 0xFFFFu;
    uint16_t closest_any = 0xFFFFu;

    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t v = mm[i];
        if (!tof_mm_valid(v))
        {
            continue;
        }

        if (v < closest_any)
        {
            closest_any = v;
        }

        if (v >= s_range_near_mm && v <= s_range_far_mm && v < closest_in_range)
        {
            closest_in_range = v;
        }
    }

    if (closest_in_range != 0xFFFFu)
    {
        return closest_in_range;
    }

    if (closest_any != 0xFFFFu)
    {
        return closest_any;
    }

    return 0u;
}

static TOF_UNUSED void tof_ai_region_means(const uint16_t mm[64], uint16_t *center_avg, uint16_t *edge_avg)
{
    uint32_t center_sum = 0u;
    uint32_t center_count = 0u;
    uint32_t edge_sum = 0u;
    uint32_t edge_count = 0u;

    for (uint32_t y = 0u; y < TOF_GRID_H; y++)
    {
        for (uint32_t x = 0u; x < TOF_GRID_W; x++)
        {
            const uint16_t v = mm[(y * TOF_GRID_W) + x];
            if (!tof_mm_valid(v))
            {
                continue;
            }

            if ((x >= 2u && x <= 5u) && (y >= 2u && y <= 5u))
            {
                center_sum += v;
                center_count++;
            }

            if (x == 0u || x == (TOF_GRID_W - 1u) || y == 0u || y == (TOF_GRID_H - 1u))
            {
                edge_sum += v;
                edge_count++;
            }
        }
    }

    if (center_avg)
    {
        *center_avg = (center_count > 0u) ? (uint16_t)(center_sum / center_count) : 0u;
    }
    if (edge_avg)
    {
        *edge_avg = (edge_count > 0u) ? (uint16_t)(edge_sum / edge_count) : 0u;
    }
}

static void tof_ai_log_frame(const uint16_t mm[64], bool live_data, uint32_t tick, uint32_t fullness_q10)
{
#if TOF_AI_DATA_LOG_ENABLE
    if (!live_data)
    {
        return;
    }

    if ((uint32_t)(tick - s_ai_log_last_tick) < TOF_AI_DATA_LOG_INTERVAL_TICKS)
    {
        return;
    }
    s_ai_log_last_tick = tick;

    uint32_t valid = 0u;
    uint16_t min_mm = 0u;
    uint16_t max_mm = 0u;
    uint16_t avg_mm = 0u;
    uint16_t center_avg = 0u;
    uint16_t edge_avg = 0u;
    const uint16_t actual_mm = tof_calc_actual_distance_mm(mm);

    tof_calc_frame_stats(mm, &valid, &min_mm, &max_mm, &avg_mm);
    tof_ai_region_means(mm, &center_avg, &edge_avg);

    PRINTF("AI_CSV,t=%lu,live=%u,valid=%lu,min=%u,max=%u,avg=%u,act=%u,center=%u,edge=%u,full_q10=%lu\r\n",
           (unsigned long)tick,
           1u,
           (unsigned long)valid,
           (unsigned)min_mm,
           (unsigned)max_mm,
           (unsigned)avg_mm,
           (unsigned)actual_mm,
           (unsigned)center_avg,
           (unsigned)edge_avg,
           (unsigned long)fullness_q10);

#if TOF_AI_DATA_LOG_FULL_FRAME
    PRINTF("AI_F64,t=%lu", (unsigned long)tick);
    for (uint32_t i = 0u; i < 64u; i++)
    {
        PRINTF(",%u", (unsigned)mm[i]);
    }
    PRINTF("\r\n");
#endif
#else
    (void)mm;
    (void)live_data;
    (void)tick;
    (void)fullness_q10;
#endif
}

static uint32_t tof_isqrt_u32(uint32_t n)
{
    uint32_t op = n;
    uint32_t res = 0u;
    uint32_t one = (uint32_t)1u << 30;

    while (one > op)
    {
        one >>= 2;
    }

    while (one != 0u)
    {
        if (op >= (res + one))
        {
            op -= (res + one);
            res = (res >> 1) + one;
        }
        else
        {
            res >>= 1;
        }
        one >>= 2;
    }

    return res;
}

static int32_t tof_ellipse_half_width(int32_t rx, int32_t ry, int32_t y_abs)
{
    if (rx <= 0 || ry <= 0 || y_abs >= ry)
    {
        return 0;
    }

    const uint32_t ry_u = (uint32_t)ry;
    const uint32_t y_u = (uint32_t)y_abs;
    const uint32_t term = (ry_u * ry_u) - (y_u * y_u);
    const uint32_t root = tof_isqrt_u32(term);
    return (int32_t)(((uint64_t)(uint32_t)rx * (uint64_t)root) / (uint64_t)ry_u);
}

static void tof_draw_filled_ellipse(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t color)
{
    if (rx <= 0 || ry <= 0)
    {
        return;
    }

    for (int32_t y = -ry; y <= ry; y++)
    {
        const int32_t y_abs = (y < 0) ? -y : y;
        const int32_t xw = tof_ellipse_half_width(rx, ry, y_abs);
        const int32_t py = cy + y;
        if (py < TOF_TP_Y0 || py > TOF_TP_Y1)
        {
            continue;
        }

        int32_t x0 = cx - xw;
        int32_t x1 = cx + xw;
        x0 = tof_clamp_i32(x0, TOF_TP_X0, TOF_TP_X1);
        x1 = tof_clamp_i32(x1, TOF_TP_X0, TOF_TP_X1);
        if (x1 >= x0)
        {
            display_hal_fill_rect(x0, py, x1, py, color);
        }
    }
}

static uint32_t tof_tp_fullness_q10_from_mm_q8(uint32_t mm_q8)
{
    if (mm_q8 == 0u)
    {
        return 640u;
    }

    const uint32_t near_q8 = ((uint32_t)TOF_TP_MM_FULL_NEAR << 8);
    const uint32_t far_q8 = ((uint32_t)TOF_TP_MM_EMPTY_FAR << 8);

    if (mm_q8 <= near_q8)
    {
        return 1024u;
    }

    if (mm_q8 >= far_q8)
    {
        return 0u;
    }

    return ((far_q8 - mm_q8) * 1024u) / (far_q8 - near_q8);
}

static uint16_t tof_tp_bg_color(uint32_t t, bool live_data)
{
    uint32_t r = 8u + ((t * 12u) / 255u);
    uint32_t g = 12u + ((t * 18u) / 255u);
    uint32_t b = 20u + ((t * 28u) / 255u);
    if (!live_data)
    {
        r = (r * 3u) / 4u;
        g = (g * 3u) / 4u;
        b = (b * 3u) / 4u;
    }
    return pack_rgb565(r, g, b);
}

static void tof_tp_fill_bg_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, bool live_data)
{
    x0 = tof_clamp_i32(x0, TOF_TP_X0, TOF_TP_X1);
    x1 = tof_clamp_i32(x1, TOF_TP_X0, TOF_TP_X1);
    y0 = tof_clamp_i32(y0, TOF_TP_Y0, TOF_TP_Y1);
    y1 = tof_clamp_i32(y1, TOF_TP_Y0, TOF_TP_Y1);
    if (x1 < x0 || y1 < y0)
    {
        return;
    }

    const int32_t area_h = (TOF_TP_Y1 - TOF_TP_Y0) + 1;
    const int32_t bands = 6;
    for (int32_t band = 0; band < bands; band++)
    {
        const int32_t by0 = TOF_TP_Y0 + ((band * area_h) / bands);
        int32_t by1 = TOF_TP_Y0 + (((band + 1) * area_h) / bands) - 1;
        if (band == (bands - 1))
        {
            by1 = TOF_TP_Y1;
        }

        int32_t iy0 = (y0 > by0) ? y0 : by0;
        int32_t iy1 = (y1 < by1) ? y1 : by1;
        if (iy1 < iy0)
        {
            continue;
        }

        const uint32_t t = (uint32_t)((band * 255) / (bands - 1));
        display_hal_fill_rect(x0, iy0, x1, iy1, tof_tp_bg_color(t, live_data));
    }
}

static uint16_t tof_tp_paper_color(int32_t tone, bool live_data)
{
    tone = tof_clamp_i32(tone, 0, 255);
    int32_t r = 176 + ((tone * 70) / 255);
    int32_t g = 170 + ((tone * 68) / 255);
    int32_t b = 156 + ((tone * 62) / 255);
    if (!live_data)
    {
        r = (r * 3) / 4;
        g = (g * 3) / 4;
        b = (b * 3) / 4;
    }
    return pack_rgb565((uint32_t)r, (uint32_t)g, (uint32_t)b);
}

static uint16_t tof_tp_core_color(int32_t tone, bool live_data)
{
    tone = tof_clamp_i32(tone, 0, 255);
    int32_t r = 90 + ((tone * 82) / 255);
    int32_t g = 70 + ((tone * 64) / 255);
    int32_t b = 46 + ((tone * 44) / 255);
    if (!live_data)
    {
        r = (r * 3) / 4;
        g = (g * 3) / 4;
        b = (b * 3) / 4;
    }
    return pack_rgb565((uint32_t)r, (uint32_t)g, (uint32_t)b);
}

static void tof_draw_ellipse_ring(int32_t cx,
                                  int32_t cy,
                                  int32_t outer_rx,
                                  int32_t outer_ry,
                                  int32_t thickness,
                                  uint16_t ring_color,
                                  uint16_t fill_color)
{
    if (outer_rx <= 0 || outer_ry <= 0 || thickness <= 0)
    {
        return;
    }

    tof_draw_filled_ellipse(cx, cy, outer_rx, outer_ry, ring_color);
    const int32_t inner_rx = outer_rx - thickness;
    const int32_t inner_ry = outer_ry - thickness;
    if (inner_rx > 0 && inner_ry > 0)
    {
        tof_draw_filled_ellipse(cx, cy, inner_rx, inner_ry, fill_color);
    }
}

static uint16_t tof_tp_bar_color(uint32_t fullness_q10, bool live_data)
{
    if (fullness_q10 > 1024u)
    {
        fullness_q10 = 1024u;
    }

    const uint32_t full8 = ((fullness_q10 * 255u) + 512u) / 1024u;
    uint32_t r = 24u + (((255u - full8) * 220u) / 255u);
    uint32_t g = 24u + ((full8 * 220u) / 255u);
    uint32_t b = 16u;
    if (!live_data)
    {
        r = (r * 3u) / 4u;
        g = (g * 3u) / 4u;
    }
    return pack_rgb565(r, g, b);
}

static void tof_draw_fullness_bar(uint32_t fullness_q10, uint32_t mm_q8, bool live_data)
{
    const int32_t bar_margin_x = 12;
    const int32_t bar_h = 18;
    const int32_t bar_x0 = TOF_TP_X0 + bar_margin_x;
    const int32_t bar_x1 = TOF_TP_X1 - bar_margin_x;
    const int32_t bar_y1 = TOF_TP_Y1 - 8;
    const int32_t bar_y0 = bar_y1 - bar_h;
    if (bar_x1 <= bar_x0 || bar_y1 <= bar_y0)
    {
        return;
    }

    const uint16_t frame = pack_rgb565(40u, 46u, 52u);
    const uint16_t tray = pack_rgb565(10u, 12u, 14u);
    const uint16_t bg = pack_rgb565(28u, 20u, 18u);
    display_hal_fill_rect(bar_x0, bar_y0, bar_x1, bar_y1, tray);
    display_hal_fill_rect(bar_x0, bar_y0, bar_x1, bar_y0, frame);
    display_hal_fill_rect(bar_x0, bar_y1, bar_x1, bar_y1, frame);
    display_hal_fill_rect(bar_x0, bar_y0, bar_x0, bar_y1, frame);
    display_hal_fill_rect(bar_x1, bar_y0, bar_x1, bar_y1, frame);

    const int32_t ix0 = bar_x0 + 2;
    const int32_t ix1 = bar_x1 - 2;
    const int32_t iy0 = bar_y0 + 2;
    const int32_t iy1 = bar_y1 - 2;
    const int32_t iw = ix1 - ix0 + 1;
    if (iw <= 0 || iy1 < iy0)
    {
        return;
    }

    display_hal_fill_rect(ix0, iy0, ix1, iy1, bg);
    if (fullness_q10 > 1024u)
    {
        fullness_q10 = 1024u;
    }

    int32_t fill_w = (int32_t)(((fullness_q10 * (uint32_t)iw) + 512u) / 1024u);
    if (fill_w > iw)
    {
        fill_w = iw;
    }
    if (fill_w > 0)
    {
        display_hal_fill_rect(ix0, iy0, ix0 + fill_w - 1, iy1, tof_tp_bar_color(fullness_q10, live_data));
    }
    (void)mm_q8;
}

static void tof_update_spool_model(const uint16_t mm[64], bool live_data, uint32_t tick)
{
    if (!s_tp_force_redraw && (uint32_t)(tick - s_tp_last_tick) < TOF_TP_UPDATE_TICKS)
    {
        return;
    }

    const uint16_t actual_mm = tof_calc_actual_distance_mm(mm);
    const uint32_t mm_q8 = (actual_mm > 0u) ? ((uint32_t)actual_mm << 8) : 0u;
    if (mm_q8 > 0u)
    {
        if (s_tp_mm_q8 == 0u)
        {
            s_tp_mm_q8 = mm_q8;
        }
        else
        {
            /* Faster low-pass response for ~100ms roll/bar updates. */
            s_tp_mm_q8 = ((s_tp_mm_q8 * 1u) + mm_q8 + 1u) / 2u;
        }
    }
    /* Keep last valid distance persistent across live gaps. */

    const uint32_t fullness_q10 = tof_tp_fullness_q10_from_mm_q8(s_tp_mm_q8);

    const int32_t area_w = (TOF_TP_X1 - TOF_TP_X0) + 1;
    const int32_t bar_y1 = TOF_TP_Y1 - 8;
    const int32_t bar_y0 = bar_y1 - 18;
    const int32_t outer_ry_min = 52;
    int32_t outer_ry_max = ((bar_y0 - TOF_TP_Y0) / 2) - 10;
    outer_ry_max = tof_clamp_i32(outer_ry_max, outer_ry_min, 112);
    if (outer_ry_max < outer_ry_min)
    {
        outer_ry_max = outer_ry_min;
    }

    const int32_t outer_ry = outer_ry_min +
        (int32_t)(((fullness_q10 * (uint32_t)(outer_ry_max - outer_ry_min)) + 512u) / 1024u);

    const bool render_live = live_data || (s_tp_mm_q8 > 0u);
    tof_ai_log_frame(mm, render_live, tick, fullness_q10);

    if (!s_tp_force_redraw &&
        render_live == s_tp_last_live &&
        (uint16_t)outer_ry == s_tp_last_outer_ry &&
        (uint16_t)fullness_q10 == s_tp_last_fullness_q10)
    {
        s_tp_last_tick = tick;
        return;
    }

    const int32_t outer_rx = tof_clamp_i32((outer_ry * 76) / 100, 28, (area_w / 2) - 18);
    const int32_t core_ry_nominal = 42;
    const int32_t core_ry = tof_clamp_i32(core_ry_nominal, 12, outer_ry - 6);
    const int32_t core_rx = tof_clamp_i32((core_ry * 76) / 100, 10, outer_rx - 8);
    const int32_t hole_ry = tof_clamp_i32((core_ry * 64) / 100, 8, core_ry - 2);
    const int32_t hole_rx = tof_clamp_i32((core_rx * 64) / 100, 8, core_rx - 2);
    const int32_t depth = tof_clamp_i32(24 + (outer_rx / 2), 24, area_w / 3);
    const int32_t cy = TOF_TP_Y0 + ((bar_y0 - TOF_TP_Y0) / 2);
    int32_t back_cx = TOF_TP_X0 + (area_w / 2) - (depth / 2);
    int32_t front_cx = back_cx + depth;

    int32_t min_x = back_cx - outer_rx;
    int32_t max_x = front_cx + outer_rx;
    if (min_x < (TOF_TP_X0 + 2))
    {
        const int32_t shift = (TOF_TP_X0 + 2) - min_x;
        back_cx += shift;
        front_cx += shift;
    }
    min_x = back_cx - outer_rx;
    max_x = front_cx + outer_rx;
    if (max_x > (TOF_TP_X1 - 2))
    {
        const int32_t shift = max_x - (TOF_TP_X1 - 2);
        back_cx -= shift;
        front_cx -= shift;
    }

    int32_t roll_x0 = back_cx - outer_rx - 2;
    int32_t roll_x1 = front_cx + outer_rx + (depth / 2) + 2;
    int32_t roll_y0 = cy - outer_ry - 2;
    int32_t roll_y1 = cy + outer_ry + 14;

    if (s_tp_prev_rect_valid)
    {
        roll_x0 = (roll_x0 < s_tp_prev_x0) ? roll_x0 : s_tp_prev_x0;
        roll_y0 = (roll_y0 < s_tp_prev_y0) ? roll_y0 : s_tp_prev_y0;
        roll_x1 = (roll_x1 > s_tp_prev_x1) ? roll_x1 : s_tp_prev_x1;
        roll_y1 = (roll_y1 > s_tp_prev_y1) ? roll_y1 : s_tp_prev_y1;
    }
    tof_tp_fill_bg_rect(roll_x0, roll_y0, roll_x1, roll_y1, render_live);

    tof_draw_filled_ellipse(front_cx + (depth / 4),
                            cy + outer_ry + 10,
                            outer_rx + (depth / 2),
                            tof_clamp_i32(outer_ry / 4, 6, 28),
                            pack_rgb565(8u, 8u, 10u));
    tof_draw_filled_ellipse(front_cx + (depth / 4),
                            cy + outer_ry + 8,
                            outer_rx + (depth / 3),
                            tof_clamp_i32(outer_ry / 6, 4, 18),
                            pack_rgb565(14u, 14u, 18u));

    const uint16_t back_base = tof_tp_paper_color(98, render_live);
    tof_draw_filled_ellipse(back_cx, cy, outer_rx, outer_ry, back_base);
    tof_draw_ellipse_ring(back_cx, cy, outer_rx, outer_ry, 2, tof_tp_paper_color(72, render_live), back_base);
    tof_draw_filled_ellipse(back_cx, cy, hole_rx, hole_ry, pack_rgb565(18u, 16u, 14u));

    for (int32_t y = -outer_ry; y <= outer_ry; y += 3)
    {
        const int32_t py = cy + y;
        int32_t py1 = py + 2;
        if (py1 > TOF_TP_Y1)
        {
            py1 = TOF_TP_Y1;
        }
        if (py < TOF_TP_Y0 || py1 < TOF_TP_Y0 || py > TOF_TP_Y1)
        {
            continue;
        }

        const int32_t y_abs = (y < 0) ? -y : y;
        const int32_t xw = tof_ellipse_half_width(outer_rx, outer_ry, y_abs);
        int32_t x0 = back_cx - xw;
        int32_t x1 = front_cx + xw;
        x0 = tof_clamp_i32(x0, TOF_TP_X0, TOF_TP_X1);
        x1 = tof_clamp_i32(x1, TOF_TP_X0, TOF_TP_X1);
        if (x1 < x0)
        {
            continue;
        }

        const int32_t span = x1 - x0 + 1;
        if (span <= 0)
        {
            continue;
        }

        const int32_t seam_1 = x0 + ((span * 22) / 100);
        const int32_t seam_2 = x0 + ((span * 62) / 100);
        const int32_t seam_3 = x0 + ((span * 82) / 100);
        const int32_t tone_mid = 210 - ((y_abs * 56) / outer_ry);

        const int32_t a0 = x0;
        const int32_t a1 = tof_clamp_i32(seam_1, x0, x1);
        const int32_t b0 = tof_clamp_i32(a1 + 1, x0, x1);
        const int32_t b1 = tof_clamp_i32(seam_2, x0, x1);
        const int32_t c0 = tof_clamp_i32(b1 + 1, x0, x1);
        const int32_t c1 = tof_clamp_i32(seam_3, x0, x1);
        const int32_t d0 = tof_clamp_i32(c1 + 1, x0, x1);
        const int32_t d1 = x1;

        if (a1 >= a0)
        {
            display_hal_fill_rect(a0, py, a1, py1, tof_tp_paper_color(tone_mid - 48, render_live));
        }
        if (b1 >= b0)
        {
            display_hal_fill_rect(b0, py, b1, py1, tof_tp_paper_color(tone_mid - 12, render_live));
        }
        if (c1 >= c0)
        {
            display_hal_fill_rect(c0, py, c1, py1, tof_tp_paper_color(tone_mid + 20, render_live));
        }
        if (d1 >= d0)
        {
            display_hal_fill_rect(d0, py, d1, py1, tof_tp_paper_color(tone_mid - 20, render_live));
        }

        if (((py - (cy - outer_ry)) % 8) == 0)
        {
            const int32_t lx0 = tof_clamp_i32(x0 + 4, x0, x1);
            const int32_t lx1 = tof_clamp_i32(x1 - 4, x0, x1);
            if (lx1 >= lx0)
            {
                display_hal_fill_rect(lx0, py, lx1, py1, tof_tp_paper_color(tone_mid - 20, render_live));
            }
        }
    }

    const uint16_t front_base = tof_tp_paper_color(232, render_live);
    tof_draw_filled_ellipse(front_cx, cy, outer_rx, outer_ry, front_base);
    tof_draw_ellipse_ring(front_cx, cy, outer_rx, outer_ry, 2, tof_tp_paper_color(164, render_live), front_base);

    const int32_t paper_span = tof_clamp_i32(outer_ry - core_ry, 1, 512);
    for (int32_t ry_layer = outer_ry - 3; ry_layer > (core_ry + 2); ry_layer -= 6)
    {
        const int32_t rx_layer = (outer_rx * ry_layer) / outer_ry;
        const int32_t depth_px = outer_ry - ry_layer;
        const int32_t tone = 230 - ((depth_px * 92) / paper_span);
        tof_draw_ellipse_ring(front_cx, cy, rx_layer, ry_layer, 1, tof_tp_paper_color(tone, render_live), front_base);
    }

    const uint16_t core_base = tof_tp_core_color(184, render_live);
    tof_draw_filled_ellipse(front_cx, cy, core_rx, core_ry, core_base);
    tof_draw_ellipse_ring(front_cx, cy, core_rx, core_ry, 2, tof_tp_core_color(118, render_live), core_base);

    const int32_t core_span = tof_clamp_i32(core_ry - hole_ry, 1, 512);
    for (int32_t ry_layer = core_ry - 2; ry_layer > (hole_ry + 1); ry_layer -= 5)
    {
        const int32_t rx_layer = (core_rx * ry_layer) / core_ry;
        const int32_t depth_px = core_ry - ry_layer;
        const int32_t tone = 176 - ((depth_px * 86) / core_span);
        tof_draw_ellipse_ring(front_cx, cy, rx_layer, ry_layer, 1, tof_tp_core_color(tone, render_live), core_base);
    }

    tof_draw_filled_ellipse(front_cx, cy, hole_rx, hole_ry, pack_rgb565(24u, 22u, 20u));
    tof_draw_filled_ellipse(front_cx + 1,
                            cy - 1,
                            tof_clamp_i32((hole_rx * 68) / 100, 4, hole_rx),
                            tof_clamp_i32((hole_ry * 68) / 100, 4, hole_ry),
                            pack_rgb565(11u, 11u, 13u));

    tof_draw_fullness_bar(fullness_q10, s_tp_mm_q8, render_live);

    s_tp_prev_x0 = (int16_t)tof_clamp_i32(back_cx - outer_rx - 2, TOF_TP_X0, TOF_TP_X1);
    s_tp_prev_y0 = (int16_t)tof_clamp_i32(cy - outer_ry - 2, TOF_TP_Y0, TOF_TP_Y1);
    s_tp_prev_x1 = (int16_t)tof_clamp_i32(front_cx + outer_rx + (depth / 2) + 2, TOF_TP_X0, TOF_TP_X1);
    s_tp_prev_y1 = (int16_t)tof_clamp_i32(cy + outer_ry + 14, TOF_TP_Y0, TOF_TP_Y1);
    s_tp_prev_rect_valid = true;

    s_tp_last_tick = tick;
    s_tp_last_outer_ry = (uint16_t)outer_ry;
    s_tp_last_fullness_q10 = (uint16_t)fullness_q10;
    s_tp_last_live = render_live;
    s_tp_force_redraw = false;
}

static void tof_update_debug_panel(const uint16_t mm[64],
                                   bool live_data,
                                   bool got_live,
                                   bool got_complete,
                                   uint32_t stale_frames,
                                   uint32_t zero_live_frames,
                                   uint32_t tick)
{
    if (!s_dbg_force_redraw)
    {
        if ((uint32_t)(tick - s_dbg_last_tick) < TOF_DEBUG_UPDATE_TICKS)
        {
            return;
        }
    }
    s_dbg_last_tick = tick;

    uint32_t valid = 0u;
    uint16_t min_mm = 0u;
    uint16_t max_mm = 0u;
    uint16_t avg_mm = 0u;
    uint16_t actual_mm = 0u;
    tof_calc_frame_stats(mm, &valid, &min_mm, &max_mm, &avg_mm);
    actual_mm = tof_calc_actual_distance_mm(mm);

    char line[TOF_DBG_COLS + 1u];
    snprintf(line, sizeof(line), "LIVE:%u GL:%u GC:%u",
             live_data ? 1u : 0u,
             got_live ? 1u : 0u,
             got_complete ? 1u : 0u);
    tof_dbg_draw_line(0u, line, s_ui_dbg_fg);

    snprintf(line, sizeof(line), "RNG:%u-%u",
             (unsigned)s_range_near_mm,
             (unsigned)s_range_far_mm);
    tof_dbg_draw_line(1u, line, s_ui_dbg_fg);

    snprintf(line, sizeof(line), "AVG:%u ACT:%u",
             (unsigned)avg_mm,
             (unsigned)actual_mm);
    tof_dbg_draw_line(2u, line, s_ui_dbg_fg);

    snprintf(line, sizeof(line), "V:%lu M:%u/%u",
             (unsigned long)valid,
             (unsigned)min_mm,
             (unsigned)max_mm);
    tof_dbg_draw_line(3u, line, s_ui_dbg_dim);

    snprintf(line, sizeof(line), "ST:%lu Z:%lu",
             (unsigned long)stale_frames,
             (unsigned long)zero_live_frames);
    tof_dbg_draw_line(4u, line, s_ui_dbg_dim);

    snprintf(line, sizeof(line), "REF:%ums RAW:%u",
             (unsigned)(TOF_RESPONSE_TARGET_US / 1000u),
             (unsigned)TOF_DEBUG_RAW_DRAW);
    tof_dbg_draw_line(5u, line, s_ui_dbg_dim);

    s_dbg_force_redraw = false;
}

static void tof_build_layout(void)
{
    const int32_t gap_px = TOF_CELL_GAP;
    const int32_t q1_w = TOF_Q_W;
    const int32_t q1_top_h = (TOF_Q1_TOP_Y1 - TOF_Q1_TOP_Y0) + 1;
    const int32_t cell_px_w = (q1_w - ((TOF_GRID_W - 1) * gap_px)) / TOF_GRID_W;
    const int32_t cell_px_h = (q1_top_h - ((TOF_GRID_H - 1) * gap_px)) / TOF_GRID_H;
    int32_t cell_px = (cell_px_w < cell_px_h) ? cell_px_w : cell_px_h;
    if (cell_px < 3)
    {
        cell_px = 3;
    }
    const int32_t grid_w_px = (TOF_GRID_W * cell_px) + ((TOF_GRID_W - 1) * gap_px);
    const int32_t grid_h_px = (TOF_GRID_H * cell_px) + ((TOF_GRID_H - 1) * gap_px);
    int32_t x0 = TOF_Q1_X0 + ((q1_w - grid_w_px) / 2);
    int32_t y0 = TOF_Q1_TOP_Y0 + ((q1_top_h - grid_h_px) / 2);

    x0 = tof_clamp_i32(x0, TOF_Q1_X0, TOF_Q1_X1 - grid_w_px + 1);
    y0 = tof_clamp_i32(y0, TOF_Q1_TOP_Y0, TOF_Q1_TOP_Y1 - grid_h_px + 1);

    s_dbg_x0 = (int16_t)x0;
    s_dbg_y0 = TOF_Q1_BOT_Y0;
    s_dbg_x1 = (int16_t)(x0 + grid_w_px - 1);
    s_dbg_y1 = TOF_Q1_BOT_Y1;

    for (int32_t gy = 0; gy < TOF_GRID_H; gy++)
    {
        for (int32_t gx = 0; gx < TOF_GRID_W; gx++)
        {
            const int32_t idx = gy * TOF_GRID_W + gx;
            const int32_t cell_x = x0 + gx * (cell_px + gap_px);
            const int32_t cell_y = y0 + gy * (cell_px + gap_px);

            s_cells[idx].x0 = (int16_t)cell_x;
            s_cells[idx].y0 = (int16_t)cell_y;
            s_cells[idx].x1 = (int16_t)(cell_x + cell_px - 1);
            s_cells[idx].y1 = (int16_t)(cell_y + cell_px - 1);
            s_cells[idx].ix0 = (int16_t)(cell_x + TOF_CELL_INSET);
            s_cells[idx].iy0 = (int16_t)(cell_y + TOF_CELL_INSET);
            s_cells[idx].ix1 = (int16_t)(cell_x + cell_px - TOF_CELL_INSET - 1);
            s_cells[idx].iy1 = (int16_t)(cell_y + cell_px - TOF_CELL_INSET - 1);
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
    s_ui_hot = pack_rgb565(255u, 255u, 255u);
    s_ui_invalid = pack_rgb565(14u, 14u, 18u);
    s_ui_below_range = pack_rgb565(0u, 28u, 120u);
    s_ui_above_range = pack_rgb565(56u, 20u, 0u);
    s_ui_dbg_bg = pack_rgb565(6u, 8u, 10u);
    s_ui_dbg_fg = pack_rgb565(210u, 220u, 230u);
    s_ui_dbg_dim = pack_rgb565(120u, 132u, 146u);

    tof_build_layout();
    display_hal_fill(s_ui_bg);
    display_hal_fill_rect(TOF_Q_W, 0, TOF_Q_W, TOF_LCD_H - 1, pack_rgb565(20u, 24u, 28u));
    display_hal_fill_rect(s_dbg_x0, TOF_Q1_BOT_Y0, s_dbg_x1, TOF_Q1_BOT_Y0, pack_rgb565(28u, 32u, 36u));
    display_hal_fill_rect(s_dbg_x0, s_dbg_y0, s_dbg_x1, s_dbg_y1, s_ui_dbg_bg);
    display_hal_fill_rect(s_dbg_x0, s_dbg_y0, s_dbg_x1, s_dbg_y0, pack_rgb565(32u, 38u, 44u));

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
    memset(s_dbg_prev, 0, sizeof(s_dbg_prev));
    s_hot_idx = -1;
    s_range_near_mm = TOF_LOCKED_NEAR_MM;
    s_range_far_mm = TOF_LOCKED_FAR_MM;
    s_dbg_last_tick = 0u;
    s_dbg_force_redraw = true;
    s_tp_last_tick = 0u;
    s_tp_last_outer_ry = 0u;
    s_tp_last_fullness_q10 = 0u;
    s_tp_mm_q8 = 0u;
    s_tp_last_live = false;
    s_tp_prev_rect_valid = false;
    s_tp_prev_x0 = 0;
    s_tp_prev_y0 = 0;
    s_tp_prev_x1 = 0;
    s_tp_prev_y1 = 0;
    s_ai_log_last_tick = 0u;
    s_tp_force_redraw = true;
}

#if !TOF_DEBUG_RAW_DRAW
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
#endif

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

static void TOF_UNUSED tof_trace_quadrant_means(const uint16_t mm[64], uint32_t cycle_idx)
{
    uint32_t q_sum[4] = {0u, 0u, 0u, 0u};
    uint32_t q_cnt[4] = {0u, 0u, 0u, 0u};

    for (uint32_t y = 0u; y < 8u; y++)
    {
        for (uint32_t x = 0u; x < 8u; x++)
        {
            const uint32_t idx = (y * 8u) + x;
            const uint16_t v = mm[idx];
            if (!tof_mm_valid(v))
            {
                continue;
            }

            const uint32_t q = ((y >= 4u) ? 2u : 0u) | ((x >= 4u) ? 1u : 0u);
            q_sum[q] += v;
            q_cnt[q]++;
        }
    }

    const uint32_t q00 = (q_cnt[0] > 0u) ? (q_sum[0] / q_cnt[0]) : 0u;
    const uint32_t q01 = (q_cnt[1] > 0u) ? (q_sum[1] / q_cnt[1]) : 0u;
    const uint32_t q10 = (q_cnt[2] > 0u) ? (q_sum[2] / q_cnt[2]) : 0u;
    const uint32_t q11 = (q_cnt[3] > 0u) ? (q_sum[3] / q_cnt[3]) : 0u;
    const uint32_t valid = q_cnt[0] + q_cnt[1] + q_cnt[2] + q_cnt[3];

    PRINTF("TOF SYN: cycle=%u map=%u q00=%u q01=%u q10=%u q11=%u valid=%u\r\n",
           (unsigned)cycle_idx,
           (unsigned)TOF_SYNTH_MAP_MODE,
           (unsigned)q00,
           (unsigned)q01,
           (unsigned)q10,
           (unsigned)q11,
           (unsigned)valid);
}

static void TOF_UNUSED tof_fill_synth_fixed(uint16_t out_mm[64], uint32_t tick)
{
    const uint32_t drift = (tick / 6u) & 0x3Fu;
    for (uint32_t y = 0u; y < 8u; y++)
    {
        for (uint32_t x = 0u; x < 8u; x++)
        {
            const uint32_t idx = (y * 8u) + x;
            uint32_t mm = 350u + (x * 140u) + (y * 90u);
            mm += ((x + y + drift) & 0x7u) * 25u;
            out_mm[idx] = (uint16_t)mm;
        }
    }
}

static uint32_t TOF_UNUSED tof_synth_zone_index_8x8(uint32_t capture, uint32_t zone16)
{
    uint32_t x = 0u;
    uint32_t y = 0u;

#if (TOF_SYNTH_MAP_MODE == 1u)
    x = (zone16 & 0x3u) + ((capture & 0x1u) << 2u);
    y = ((zone16 >> 2u) & 0x3u) + (((capture >> 1u) & 0x1u) << 2u);
#elif (TOF_SYNTH_MAP_MODE == 2u)
    x = zone16 & 0x7u;
    y = ((zone16 >> 3u) & 0x1u) + (capture << 1u);
#elif (TOF_SYNTH_MAP_MODE == 3u)
    x = ((zone16 >> 3u) & 0x1u) + (capture << 1u);
    y = zone16 & 0x7u;
#else
    const uint32_t phase_x = capture & 0x1u;
    const uint32_t phase_y = (capture >> 1u) & 0x1u;
    const uint32_t local_x = zone16 & 0x3u;
    const uint32_t local_y = (zone16 >> 2u) & 0x3u;
    x = (local_x << 1u) | phase_x;
    y = (local_y << 1u) | phase_y;
#endif

    return (y * 8u) + x;
}

static bool TOF_UNUSED tof_fill_synth_subcap(uint16_t out_mm[64], uint32_t tick)
{
    const uint32_t capture = s_synth_subcap_capture;
    const uint16_t wave = (uint16_t)(((tick / 4u) & 0xFu) * 8u);

    for (uint32_t zone16 = 0u; zone16 < 16u; zone16++)
    {
        const uint32_t local_x = zone16 & 0x3u;
        const uint32_t local_y = (zone16 >> 2u) & 0x3u;
        const uint32_t dst = tof_synth_zone_index_8x8(capture, zone16);
        if (dst >= 64u)
        {
            continue;
        }

        const uint16_t base = (uint16_t)(700u + (capture * 600u));
        const uint16_t cell = (uint16_t)(base + (local_x * 90u) + (local_y * 120u) + wave);
        s_synth_subcap_frame[dst] = cell;
    }

    memcpy(out_mm, s_synth_subcap_frame, sizeof(s_synth_subcap_frame));
    s_synth_subcap_capture = (uint8_t)((s_synth_subcap_capture + 1u) & 0x3u);
    return (s_synth_subcap_capture == 0u);
}

#if !TOF_DEBUG_RAW_DRAW
static void tof_decay_frame(uint16_t mm[64])
{
    for (uint32_t i = 0; i < 64u; i++)
    {
        mm[i] = (uint16_t)(((uint32_t)mm[i] * 7u) / 8u);
    }
}
#endif

static void tof_update_range(const uint16_t mm[64], bool live_data)
{
#if !TOF_USE_DYNAMIC_RANGE
    (void)mm;
    (void)live_data;
    s_range_near_mm = TOF_LOCKED_NEAR_MM;
    s_range_far_mm = TOF_LOCKED_FAR_MM;
    return;
#else
    if (!live_data)
    {
        s_range_near_mm = TOF_LOCKED_NEAR_MM;
        s_range_far_mm = TOF_LOCKED_FAR_MM;
        return;
    }

    uint16_t min_mm = 0xFFFFu;
    uint16_t max_mm = 0u;
    uint32_t valid_count = 0u;

    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t v = mm[i];
        if (!tof_mm_valid(v))
        {
            continue;
        }
        valid_count++;
        if (v < min_mm)
        {
            min_mm = v;
        }
        if (v > max_mm)
        {
            max_mm = v;
        }
    }

    if (valid_count < 4u || min_mm >= max_mm)
    {
        s_range_near_mm = TOF_LOCKED_NEAR_MM;
        s_range_far_mm = TOF_LOCKED_FAR_MM;
        return;
    }

    uint16_t pad = (uint16_t)(((uint32_t)(max_mm - min_mm) / 8u) + 20u);
    uint16_t near_mm = (min_mm > pad) ? (uint16_t)(min_mm - pad) : 0u;
    uint16_t far_mm = (uint16_t)(max_mm + pad);
    if (far_mm <= near_mm)
    {
        far_mm = (uint16_t)(near_mm + 1u);
    }

    s_range_near_mm = near_mm;
    s_range_far_mm = far_mm;
#endif
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

#if !TOF_DEBUG_RAW_DRAW
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
#endif

#if TOF_DEBUG_RAW_DRAW
static void tof_draw_heatmap_raw(const uint16_t mm[64], bool live_data)
{
    uint16_t draw_mm[64];
    tof_fill_display_holes(mm, draw_mm);
    tof_update_range(draw_mm, live_data);

    for (uint32_t idx = 0; idx < 64u; idx++)
    {
        const tof_cell_rect_t *c = &s_cells[idx];
        const uint16_t color = tof_color_from_mm(draw_mm[idx], s_range_near_mm, s_range_far_mm);

        if (!s_cell_drawn[idx] || s_last_cell_color[idx] != color)
        {
            display_hal_fill_rect(c->ix0, c->iy0, c->ix1, c->iy1, color);
            s_last_cell_color[idx] = color;
            s_cell_drawn[idx] = true;
        }
    }

    tof_update_hotspot(draw_mm, live_data);
}
#endif

int main(void)
{
    BOARD_InitHardware();

    if (!display_hal_init())
    {
        for (;;) {}
    }

    bool tof_ok = true;
#if (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_LIVE)
    tof_ok = tmf8828_quick_init();
    PRINTF("TOF demo: TMF8828 %s\r\n", tof_ok ? "ready" : "fallback mode");
#elif (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_SYNTH_FIXED)
    PRINTF("TOF demo: synthetic fixed-frame mode enabled\r\n");
#elif (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_SYNTH_SUBCAP)
    PRINTF("TOF demo: synthetic 4-subcapture mode enabled (map=%u)\r\n", (unsigned)TOF_SYNTH_MAP_MODE);
#else
    PRINTF("TOF demo: invalid TOF_DEBUG_INPUT_MODE=%u\r\n", (unsigned)TOF_DEBUG_INPUT_MODE);
    tof_ok = false;
#endif
#if TOF_DEBUG_RAW_DRAW
    PRINTF("TOF demo: raw draw mode enabled (no smoothing/persistence/fallback)\r\n");
#endif

    tof_ui_init();
    memset(s_synth_subcap_frame, 0, sizeof(s_synth_subcap_frame));
    s_synth_subcap_capture = 0u;

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
    uint32_t TOF_UNUSED synth_complete_count = 0u;
    uint32_t last_draw_tick = 0u;
    bool have_drawn_frame = false;

    for (;;)
    {
        bool got_live = false;
        bool got_complete = false;
        uint16_t complete_frame_mm[64];
        bool have_complete_frame = false;
        if (tof_ok)
        {
#if (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_LIVE)
            for (uint32_t burst = 0u; burst < TOF_READ_BURST_MAX; burst++)
            {
                bool packet_complete = false;
                if (!tmf8828_quick_read_8x8(frame_mm, &packet_complete))
                {
                    break;
                }
                got_live = true;
                if (packet_complete)
                {
                    memcpy(complete_frame_mm, frame_mm, sizeof(complete_frame_mm));
                    have_complete_frame = true;
                }
            }
            got_complete = have_complete_frame;
            if (have_complete_frame)
            {
                memcpy(frame_mm, complete_frame_mm, sizeof(complete_frame_mm));
            }
#elif (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_SYNTH_FIXED)
            tof_fill_synth_fixed(frame_mm, tick);
            got_live = true;
            got_complete = true;
#elif (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_SYNTH_SUBCAP)
            got_complete = tof_fill_synth_subcap(frame_mm, tick);
            got_live = true;
#else
            got_live = false;
            got_complete = false;
#endif
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

#if (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_SYNTH_SUBCAP)
            if (got_complete)
            {
                synth_complete_count++;
                if ((synth_complete_count % TOF_SYNTH_TRACE_EVERY_COMPLETE) == 0u)
                {
                    tof_trace_quadrant_means(frame_mm, synth_complete_count);
                }
            }
#endif

#if (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_LIVE)
            if (TOF_ENABLE_AUTO_RECOVERY)
            {
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
#endif
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
#if (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_LIVE)
                if (TOF_ENABLE_AUTO_RECOVERY)
                {
                    restart_attempted = true;
                    if (!tmf8828_quick_restart_measurement())
                    {
                        PRINTF("TOF demo: stream restart failed\r\n");
                    }
                }
#endif
            }
        }

        bool draw_now = false;

        if (!tof_ok)
        {
#if TOF_DEBUG_RAW_DRAW
            memset(frame_mm, 0, sizeof(frame_mm));
            draw_now = true;
#else
            tof_make_fallback_frame(frame_mm, tick);
            draw_now = true;
#endif
        }
        else if (!have_live)
        {
#if TOF_DEBUG_RAW_DRAW
            /* Keep last frame visible during transient live gaps to avoid
             * full-screen blank/wipe flashes.
             */
            draw_now = true;
#else
            tof_decay_frame(frame_mm);
            draw_now = true;
#endif
        }
        else if (
#if (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_LIVE)
            (TOF_DRAW_ON_COMPLETE_ONLY ? got_complete : (got_live || got_complete))
#else
            (got_live || got_complete)
#endif
        )
        {
            draw_now = true;
        }
        else if (got_live)
        {
            const uint32_t draw_gap =
                have_drawn_frame ? (uint32_t)(tick - last_draw_tick) : TOF_MAX_DRAW_GAP_TICKS;
            if (draw_gap >= TOF_MAX_DRAW_GAP_TICKS)
            {
                draw_now = true;
            }
        }

        if (tof_ok && stale_frames >= TOF_REINIT_LIMIT_FRAMES)
        {
#if (TOF_DEBUG_INPUT_MODE == TOF_INPUT_MODE_LIVE)
            if (TOF_ENABLE_AUTO_RECOVERY)
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
#endif
        }

        if (draw_now)
        {
#if TOF_DEBUG_RAW_DRAW
            const bool raw_live = (tof_ok && got_live);
            tof_draw_heatmap_raw(frame_mm, raw_live);
#else
            tof_draw_heatmap_incremental(frame_mm, have_live);
#endif
            last_draw_tick = tick;
            have_drawn_frame = true;
        }

        tof_update_spool_model(frame_mm, (tof_ok && have_live), tick);

        tof_update_debug_panel(frame_mm,
                               (tof_ok && have_live),
                               got_live,
                               got_complete,
                               stale_frames,
                               zero_live_frames,
                               tick);

        tick++;
        SDK_DelayAtLeastUs(TOF_FRAME_US, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }
}
