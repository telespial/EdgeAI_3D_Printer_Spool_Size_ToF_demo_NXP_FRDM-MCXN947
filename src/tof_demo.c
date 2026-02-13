#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "app.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "fsl_lpi2c.h"
#include "fsl_port.h"
#include "fsl_gt911.h"

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
#define TOF_LOCKED_FAR_MM  120u
#define TOF_HEATMAP_DARK_MM 100u
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
#define TOF_DEBUG_UPDATE_US 200000u
#define TOF_DEBUG_UPDATE_TICKS_RAW ((TOF_DEBUG_UPDATE_US + TOF_FRAME_US - 1u) / TOF_FRAME_US)
#define TOF_DEBUG_UPDATE_TICKS ((TOF_DEBUG_UPDATE_TICKS_RAW > 0u) ? TOF_DEBUG_UPDATE_TICKS_RAW : 1u)
#define TOF_MAX_DRAW_GAP_TICKS_RAW ((TOF_RESPONSE_TARGET_US + TOF_FRAME_US - 1u) / TOF_FRAME_US)
#define TOF_MAX_DRAW_GAP_TICKS ((TOF_MAX_DRAW_GAP_TICKS_RAW > 0u) ? TOF_MAX_DRAW_GAP_TICKS_RAW : 1u)
#define TOF_TP_UPDATE_US 20000u
#define TOF_TP_UPDATE_TICKS_RAW ((TOF_TP_UPDATE_US + TOF_FRAME_US - 1u) / TOF_FRAME_US)
#define TOF_TP_UPDATE_TICKS ((TOF_TP_UPDATE_TICKS_RAW > 0u) ? TOF_TP_UPDATE_TICKS_RAW : 1u)

#define TOF_DBG_SCALE 2
#define TOF_DBG_CHAR_W 3
#define TOF_DBG_CHAR_H 5
#define TOF_DBG_CHAR_ADV ((TOF_DBG_CHAR_W * TOF_DBG_SCALE) + 2)
#define TOF_DBG_LINE_H ((TOF_DBG_CHAR_H * TOF_DBG_SCALE) + 2)
#define TOF_DBG_LINES 6u
#define TOF_DBG_COLS 19u

#define TOF_TP_MM_FULL_NEAR 35u
#define TOF_TP_MM_EMPTY_FAR 60u
#define TOF_TP_MM_CLIP_MIN TOF_TP_MM_FULL_NEAR
#define TOF_TP_MM_CLIP_MAX 120u
#define TOF_TP_MM_SHIFT (0)
#define TOF_TP_MM_GAIN_Q8 256u /* 1.00x gain to preserve full->empty spread */
#define TOF_AI_PILL_H 20
#define TOF_AI_PILL_MARGIN_X 4
#define TOF_AI_PILL_MARGIN_BOTTOM 4
#define TOF_ALERT_PILL_H 20
#define TOF_ALERT_PILL_GAP_Y 6
#define TOF_ALERT_POPUP_US 3000000u
#define TOF_ALERT_POPUP_TICKS_RAW ((TOF_ALERT_POPUP_US + TOF_FRAME_US - 1u) / TOF_FRAME_US)
#define TOF_ALERT_POPUP_TICKS ((TOF_ALERT_POPUP_TICKS_RAW > 0u) ? TOF_ALERT_POPUP_TICKS_RAW : 1u)
#define TOF_ALERT_POPUP_X_MARGIN 18
#define TOF_ALERT_POPUP_W_PCT 80
#define TOF_ALERT_POPUP_H 81
#define TOF_ALERT_POPUP_GAP_Y 6
#define TOF_TP_BAR_MARGIN_X 12
#define TOF_TP_BAR_H 18
#define TOF_TP_STATUS_H 16
#define TOF_TP_STATUS_GAP_Y 6
#define TOF_ROLL_MEDIUM_TRIGGER_MM TOF_TP_MM_FULL_NEAR
#define TOF_ROLL_LOW_TRIGGER_MM 50u
#define TOF_ROLL_EMPTY_TRIGGER_MM 60u
#define TOF_ROLL_MEDIUM_MIN_Q10 358u  /* 35% */
#define TOF_ROLL_FULL_MIN_Q10 768u    /* 75% */
#define TOF_ROLL_SEGMENT_COUNT 8u
#define TOF_ROLL_SEG_MED_MIN 3u
#define TOF_ROLL_SEG_FULL_MIN 6u
#define TOF_ROLL_SEG_LOW_TO_MED_ENTER 3u
#define TOF_ROLL_SEG_MED_TO_LOW_EXIT 2u
#define TOF_ROLL_SEG_MED_TO_FULL_ENTER 7u
#define TOF_ROLL_SEG_FULL_TO_MED_EXIT 5u
#define TOF_ROLL_LEVEL_CONSENSUS_FRAMES 2u
#define TOF_AI_MODEL_MM_BIAS 0u
#define TOF_ROLL_FULL_CAPTURE_MM (TOF_TP_MM_FULL_NEAR + 8u)
#define TOF_ROLL_FULL_REARM_STREAK 2u
#define TOF_TP_BAR_MM_EMPTY TOF_ROLL_EMPTY_TRIGGER_MM
#define TOF_TP_ROLL_REDRAW_MM_DELTA 2u
#define TOF_ROLL_FULL_SPARSE_VALID_MAX 36u
#define TOF_ROLL_FULL_SPARSE_AVG_MAX 90u
#define TOF_TP_CLOSEST_OUTLIER_VALID_MAX 40u
#define TOF_TP_CLOSEST_OUTLIER_AVG_MIN 90u
#define TOF_ROLL_EMPTY_SPARSE_VALID_MAX 10u
#define TOF_ROLL_EMPTY_FORCE_MM (TOF_ROLL_EMPTY_TRIGGER_MM + 8u)
#define TOF_ROLL_EMPTY_SPARSE_AVG_MIN 70u
#define TOF_ROLL_EMPTY_ENTER_MM 62u
#define TOF_ROLL_EMPTY_EXIT_MM 58u
#define TOF_TP_CURVE_ROWS_PICK 4u
#define TOF_TP_CURVE_EDGE_GUARD_COLS 1u
#define TOF_TP_CURVE_MIN_ROWS 4u

#define TOF_EST_ENABLE 1u
#define TOF_EST_VALID_MIN 10u
#define TOF_EST_SPREAD_GOOD_MM 120u
#define TOF_EST_SPREAD_BAD_MM 700u
#define TOF_EST_FAST_DELTA_MM 8u
#define TOF_EST_CONF_TRAIN_MIN_Q10 700u
#define TOF_EST_NEAR_MIN_MM 40u
#define TOF_EST_NEAR_MAX_MM 70u
#define TOF_EST_FAR_MIN_MM 90u
#define TOF_EST_FAR_MAX_MM 120u
#define TOF_EST_MIN_GAP_MM 25u
#define TOF_AI_FUSE_CONF_MIN_Q10 384u
#define TOF_AI_FUSE_MM_WEIGHT_MAX_Q10 512u
#define TOF_AI_FUSE_FULLNESS_WEIGHT_MAX_Q10 384u
#define TOF_AI_GRID_ENABLE 1u
#define TOF_AI_GRID_NEIGHBOR_MIN 2u
#define TOF_AI_GRID_OUTLIER_MM_MIN 16u
#define TOF_AI_GRID_OUTLIER_MM_MAX 80u
#define TOF_AI_GRID_FAST_DELTA_MM 10u
#define TOF_AI_GRID_HOLD_FRAMES 24u
#define TOF_CORNER_REPAIR_DELTA_MM 72u
#define TOF_TOUCH_I2C LPI2C2
#define TOF_TOUCH_I2C_SUBADDR_SIZE 2u
#define TOF_TOUCH_POLL_US 20000u
#define TOF_TOUCH_POLL_TICKS_RAW ((TOF_TOUCH_POLL_US + TOF_FRAME_US - 1u) / TOF_FRAME_US)
#define TOF_TOUCH_POLL_TICKS ((TOF_TOUCH_POLL_TICKS_RAW > 0u) ? TOF_TOUCH_POLL_TICKS_RAW : 1u)
#define TOF_TOUCH_POINTS 5u
#define TOF_TOUCH_INT_PORT PORT4
#define TOF_TOUCH_INT_PIN 6u

#ifndef TOF_AI_DATA_LOG_ENABLE
#define TOF_AI_DATA_LOG_ENABLE 1u
#endif
#ifndef TOF_AI_DATA_LOG_FULL_FRAME
#define TOF_AI_DATA_LOG_FULL_FRAME 1u
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

typedef enum
{
    kTofRollAlertFull = 0,
    kTofRollAlertMedium = 1,
    kTofRollAlertLow = 2,
    kTofRollAlertEmpty = 3,
} tof_roll_alert_level_t;

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
static uint16_t s_ui_pick;
static uint16_t s_ui_invalid;
static uint16_t s_ui_below_range;
static uint16_t s_ui_above_range;
static int32_t s_hot_idx = -1;
static int16_t s_curve_pick_idx[TOF_GRID_H];
static int16_t s_curve_prev_pick_idx[TOF_GRID_H];
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
static int16_t s_ai_pill_x0 = 0;
static int16_t s_ai_pill_y0 = 0;
static int16_t s_ai_pill_x1 = 0;
static int16_t s_ai_pill_y1 = 0;
static bool s_ai_pill_prev_valid = false;
static bool s_ai_pill_prev_on = false;
static int16_t s_alert_pill_x0 = 0;
static int16_t s_alert_pill_y0 = 0;
static int16_t s_alert_pill_x1 = 0;
static int16_t s_alert_pill_y1 = 0;
static bool s_alert_pill_prev_valid = false;
static bool s_alert_pill_prev_on = true;
static bool s_tp_force_redraw = true;
static uint32_t s_tp_last_tick = 0u;
static uint16_t s_tp_last_outer_ry = 0u;
static uint16_t s_tp_last_outer_rx = 0u;
static uint16_t s_tp_last_fullness_q10 = 0u;
static uint32_t s_tp_last_roll_mm_q8 = 0u;
static uint32_t s_tp_mm_q8 = 0u;
static uint16_t s_tp_live_actual_mm = 0u;
static uint16_t s_tp_live_closest_mm = 0u;
static bool s_tp_last_live = false;
static bool s_tp_prev_rect_valid = false;
static int16_t s_tp_prev_x0 = 0;
static int16_t s_tp_prev_y0 = 0;
static int16_t s_tp_prev_x1 = 0;
static int16_t s_tp_prev_y1 = 0;
static uint32_t s_ai_log_last_tick = 0u;
static gt911_handle_t s_touch_handle;
static bool s_touch_ready = false;
static bool s_touch_was_down = false;
static uint32_t s_touch_last_poll_tick = 0u;
static bool s_ai_runtime_on = (TOF_AI_DATA_LOG_ENABLE != 0u);
static uint16_t s_est_near_mm = TOF_TP_MM_FULL_NEAR;
static uint16_t s_est_far_mm = TOF_TP_MM_EMPTY_FAR;
static uint32_t s_est_mm_q8 = 0u;
static uint16_t s_est_conf_q10 = 0u;
static uint16_t s_est_fullness_q10 = 640u;
static uint32_t s_est_valid_count = 0u;
static uint16_t s_est_spread_mm = 0u;
static uint16_t s_roll_fullness_q10 = 640u;
static uint16_t s_roll_model_mm = 0u;
static bool s_alert_runtime_on = false;
static tof_roll_alert_level_t s_roll_alert_prev_level = kTofRollAlertFull;
static bool s_roll_alert_prev_valid = false;
static tof_roll_alert_level_t s_roll_status_prev_level = kTofRollAlertFull;
static bool s_roll_status_prev_valid = false;
static tof_roll_alert_level_t s_roll_level_stable = kTofRollAlertFull;
static bool s_roll_level_stable_valid = false;
static tof_roll_alert_level_t s_roll_level_candidate = kTofRollAlertFull;
static uint8_t s_roll_level_candidate_count = 0u;
static bool s_roll_status_prev_live = false;
static bool s_alert_popup_active = false;
static bool s_alert_popup_prev_drawn = false;
static bool s_alert_popup_rearm_on_full = true;
static bool s_alert_popup_hold_empty = false;
static bool s_alert_low_popup_shown = false;
static uint8_t s_roll_full_rearm_streak = 0u;
static tof_roll_alert_level_t s_alert_popup_level = kTofRollAlertLow;
static tof_roll_alert_level_t s_alert_popup_prev_level = kTofRollAlertLow;
static uint32_t s_alert_popup_until_tick = 0u;
static bool s_alert_popup_last_live = false;
static int16_t s_alert_popup_prev_x0 = 0;
static int16_t s_alert_popup_prev_y0 = 0;
static int16_t s_alert_popup_prev_x1 = 0;
static int16_t s_alert_popup_prev_y1 = 0;
static uint16_t s_ai_grid_mm[64];
static uint8_t s_ai_grid_hold_age[64];
static uint16_t s_ai_grid_noise_mm = 0u;

static uint16_t s_range_near_mm = TOF_LOCKED_NEAR_MM;
static uint16_t s_range_far_mm = TOF_LOCKED_FAR_MM;
static uint16_t s_synth_subcap_frame[64];
static uint8_t s_synth_subcap_capture = 0u;

static void tof_ai_grid_reset(void);
static void tof_tp_bar_rect(int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1);
static void tof_tp_status_rect(int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1);
static uint16_t tof_tp_bg_color(uint32_t t, bool live_data);
static void tof_tp_fill_bg_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, bool live_data);
static void tof_draw_roll_status_banner(tof_roll_alert_level_t level, bool live_data);
static void tof_draw_brand_mark(void);
static void tof_ai_denoise_heatmap_frame(const uint16_t in_mm[64], uint16_t out_mm[64], bool live_data);
static uint16_t tof_ai_grid_median_u16(uint16_t *values, uint32_t count);
static void tof_tiny_draw_char_scaled_clipped(int32_t x,
                                              int32_t y,
                                              char ch,
                                              uint16_t color,
                                              uint32_t scale,
                                              int32_t clip_x0,
                                              int32_t clip_y0,
                                              int32_t clip_x1,
                                              int32_t clip_y1);

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

    uint16_t color_far_mm = far_mm;
    if (color_far_mm > TOF_HEATMAP_DARK_MM)
    {
        color_far_mm = TOF_HEATMAP_DARK_MM;
    }

    if (color_far_mm <= near_mm)
    {
        color_far_mm = (uint16_t)(near_mm + 1u);
    }

    if (mm < near_mm)
    {
        return s_ui_below_range;
    }

    if (mm > color_far_mm)
    {
        return s_ui_above_range;
    }

    uint32_t t = 0u;
    if (mm <= near_mm)
    {
        t = 0u;
    }
    else if (mm >= color_far_mm)
    {
        t = 255u;
    }
    else
    {
        t = (uint32_t)(((uint32_t)(mm - near_mm) * 255u) / (color_far_mm - near_mm));
    }

    uint32_t r = 0u;
    uint32_t g = 0u;
    uint32_t b = 0u;
    if (t < 128u)
    {
        const uint32_t u = t * 2u;
        r = (u * 220u) / 255u;
        g = 220u - ((u * 40u) / 255u);
        b = 0u;
    }
    else
    {
        const uint32_t u = (t - 128u) * 2u;
        r = 220u - ((u * 164u) / 255u);
        g = 180u - ((u * 172u) / 255u);
        b = (u * 8u) / 255u;
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

static void tof_repair_corner_one(uint16_t mm[64], uint32_t corner_idx, uint32_t n0, uint32_t n1, uint32_t n2)
{
    uint32_t sum = 0u;
    uint32_t count = 0u;
    const uint16_t v0 = mm[n0];
    const uint16_t v1 = mm[n1];
    const uint16_t v2 = mm[n2];

    if (tof_mm_valid(v0))
    {
        sum += v0;
        count++;
    }
    if (tof_mm_valid(v1))
    {
        sum += v1;
        count++;
    }
    if (tof_mm_valid(v2))
    {
        sum += v2;
        count++;
    }

    if (count < 2u)
    {
        return;
    }

    const uint16_t neighbor_mm = (uint16_t)((sum + (count / 2u)) / count);
    const uint16_t corner_mm = mm[corner_idx];
    const uint16_t delta_mm = (corner_mm > neighbor_mm) ? (uint16_t)(corner_mm - neighbor_mm) : (uint16_t)(neighbor_mm - corner_mm);
    if (!tof_mm_valid(corner_mm) || delta_mm > TOF_CORNER_REPAIR_DELTA_MM)
    {
        mm[corner_idx] = neighbor_mm;
    }
}

static void tof_repair_corner_blindspots(uint16_t mm[64])
{
    const uint32_t top_left = 0u;
    const uint32_t top_right = (TOF_GRID_W - 1u);
    const uint32_t bot_left = ((TOF_GRID_H - 1u) * TOF_GRID_W);
    const uint32_t bot_right = (TOF_GRID_H * TOF_GRID_W) - 1u;

    tof_repair_corner_one(mm, top_left, 1u, TOF_GRID_W, TOF_GRID_W + 1u);
    tof_repair_corner_one(mm, top_right, TOF_GRID_W - 2u, (2u * TOF_GRID_W) - 2u, (2u * TOF_GRID_W) - 1u);
    tof_repair_corner_one(mm, bot_left, (TOF_GRID_H - 2u) * TOF_GRID_W, ((TOF_GRID_H - 2u) * TOF_GRID_W) + 1u, ((TOF_GRID_H - 1u) * TOF_GRID_W) + 1u);
    tof_repair_corner_one(mm, bot_right, (TOF_GRID_H * TOF_GRID_W) - 2u, ((TOF_GRID_H - 1u) * TOF_GRID_W) - 2u, ((TOF_GRID_H - 1u) * TOF_GRID_W) - 1u);
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
        case '(': rows[0] = 0x1; rows[1] = 0x2; rows[2] = 0x2; rows[3] = 0x2; rows[4] = 0x1; break;
        case ')': rows[0] = 0x4; rows[1] = 0x2; rows[2] = 0x2; rows[3] = 0x2; rows[4] = 0x4; break;
        case '=': rows[0] = 0x0; rows[1] = 0x7; rows[2] = 0x0; rows[3] = 0x7; rows[4] = 0x0; break;
        case ' ': rows[0] = 0x0; rows[1] = 0x0; rows[2] = 0x0; rows[3] = 0x0; rows[4] = 0x0; break;
        default:  rows[0] = 0x7; rows[1] = 0x1; rows[2] = 0x2; rows[3] = 0x0; rows[4] = 0x2; break;
    }
}

static void tof_brand_get_glyph_5x7(char ch, uint8_t rows[7])
{
    if (ch >= 'a' && ch <= 'z')
    {
        ch = (char)(ch - ('a' - 'A'));
    }

    switch (ch)
    {
        case 'A': rows[0] = 0x0E; rows[1] = 0x11; rows[2] = 0x11; rows[3] = 0x1F; rows[4] = 0x11; rows[5] = 0x11; rows[6] = 0x11; break;
        case 'B': rows[0] = 0x1E; rows[1] = 0x11; rows[2] = 0x11; rows[3] = 0x1E; rows[4] = 0x11; rows[5] = 0x11; rows[6] = 0x1E; break;
        case 'C': rows[0] = 0x0E; rows[1] = 0x11; rows[2] = 0x10; rows[3] = 0x10; rows[4] = 0x10; rows[5] = 0x11; rows[6] = 0x0E; break;
        case 'D': rows[0] = 0x1E; rows[1] = 0x11; rows[2] = 0x11; rows[3] = 0x11; rows[4] = 0x11; rows[5] = 0x11; rows[6] = 0x1E; break;
        case 'E': rows[0] = 0x1F; rows[1] = 0x10; rows[2] = 0x10; rows[3] = 0x1E; rows[4] = 0x10; rows[5] = 0x10; rows[6] = 0x1F; break;
        case 'H': rows[0] = 0x11; rows[1] = 0x11; rows[2] = 0x11; rows[3] = 0x1F; rows[4] = 0x11; rows[5] = 0x11; rows[6] = 0x11; break;
        case 'I': rows[0] = 0x1F; rows[1] = 0x04; rows[2] = 0x04; rows[3] = 0x04; rows[4] = 0x04; rows[5] = 0x04; rows[6] = 0x1F; break;
        case 'K': rows[0] = 0x11; rows[1] = 0x12; rows[2] = 0x14; rows[3] = 0x18; rows[4] = 0x14; rows[5] = 0x12; rows[6] = 0x11; break;
        case 'N': rows[0] = 0x11; rows[1] = 0x19; rows[2] = 0x15; rows[3] = 0x13; rows[4] = 0x11; rows[5] = 0x11; rows[6] = 0x11; break;
        case 'O': rows[0] = 0x0E; rows[1] = 0x11; rows[2] = 0x11; rows[3] = 0x11; rows[4] = 0x11; rows[5] = 0x11; rows[6] = 0x0E; break;
        case 'R': rows[0] = 0x1E; rows[1] = 0x11; rows[2] = 0x11; rows[3] = 0x1E; rows[4] = 0x14; rows[5] = 0x12; rows[6] = 0x11; break;
        case '(': rows[0] = 0x02; rows[1] = 0x04; rows[2] = 0x08; rows[3] = 0x08; rows[4] = 0x08; rows[5] = 0x04; rows[6] = 0x02; break;
        case ')': rows[0] = 0x08; rows[1] = 0x04; rows[2] = 0x02; rows[3] = 0x02; rows[4] = 0x02; rows[5] = 0x04; rows[6] = 0x08; break;
        case ' ': rows[0] = 0x00; rows[1] = 0x00; rows[2] = 0x00; rows[3] = 0x00; rows[4] = 0x00; rows[5] = 0x00; rows[6] = 0x00; break;
        default:  rows[0] = 0x1F; rows[1] = 0x01; rows[2] = 0x02; rows[3] = 0x04; rows[4] = 0x04; rows[5] = 0x00; rows[6] = 0x04; break;
    }
}

static void tof_brand_draw_char5x7_scaled_clipped(int32_t x,
                                                   int32_t y,
                                                   char ch,
                                                   uint16_t color,
                                                   uint32_t scale,
                                                   int32_t clip_x0,
                                                   int32_t clip_y0,
                                                   int32_t clip_x1,
                                                   int32_t clip_y1)
{
    if (scale == 0u)
    {
        return;
    }

    uint8_t rows[7];
    tof_brand_get_glyph_5x7(ch, rows);
    for (uint32_t ry = 0u; ry < 7u; ry++)
    {
        for (uint32_t rx = 0u; rx < 5u; rx++)
        {
            if (((rows[ry] >> (4u - rx)) & 0x1u) == 0u)
            {
                continue;
            }

            const int32_t px0 = x + (int32_t)(rx * scale);
            const int32_t py0 = y + (int32_t)(ry * scale);
            const int32_t px1 = px0 + (int32_t)scale - 1;
            const int32_t py1 = py0 + (int32_t)scale - 1;
            if (px1 < clip_x0 || px0 > clip_x1 || py1 < clip_y0 || py0 > clip_y1)
            {
                continue;
            }
            display_hal_fill_rect(px0, py0, px1, py1, color);
        }
    }
}

static void tof_tiny_draw_char_clipped(int32_t x,
                                       int32_t y,
                                       char ch,
                                       uint16_t color,
                                       int32_t clip_x0,
                                       int32_t clip_y0,
                                       int32_t clip_x1,
                                       int32_t clip_y1)
{
    tof_tiny_draw_char_scaled_clipped(x, y, ch, color, TOF_DBG_SCALE, clip_x0, clip_y0, clip_x1, clip_y1);
}

static void tof_tiny_draw_char_scaled_clipped(int32_t x,
                                              int32_t y,
                                              char ch,
                                              uint16_t color,
                                              uint32_t scale,
                                              int32_t clip_x0,
                                              int32_t clip_y0,
                                              int32_t clip_x1,
                                              int32_t clip_y1)
{
    if (scale == 0u)
    {
        return;
    }

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

            const int32_t px0 = x + (int32_t)(rx * scale);
            const int32_t py0 = y + (int32_t)(ry * scale);
            const int32_t px1 = px0 + (int32_t)scale - 1;
            const int32_t py1 = py0 + (int32_t)scale - 1;
            if (px1 < clip_x0 || px0 > clip_x1 || py1 < clip_y0 || py0 > clip_y1)
            {
                continue;
            }
            display_hal_fill_rect(px0, py0, px1, py1, color);
        }
    }
}

static void tof_dbg_draw_char(int32_t x, int32_t y, char ch, uint16_t color)
{
    tof_tiny_draw_char_clipped(x, y, ch, color, s_dbg_x0, s_dbg_y0, s_dbg_x1, s_dbg_y1);
}

static void tof_draw_brand_mark(void)
{
    static bool s_brand_drawn = false;
    if (!s_tp_force_redraw && s_brand_drawn)
    {
        return;
    }

    const uint16_t fg = pack_rgb565(255u, 255u, 255u);
    const char *name = "(C)RICHARD HABERKERN";
    const size_t n = strlen(name);
    const int32_t scale = 1;
    const int32_t brand_char_w = 5;
    const int32_t brand_char_h = 7;
    const int32_t char_adv = (brand_char_w * scale) + 1;
    const int32_t text_w = (n > 0u) ? ((int32_t)(n * char_adv) - 1) : 0;
    const int32_t text_h = brand_char_h * scale;
    const int32_t margin = 4;
    const int32_t mark_w = text_w;
    const int32_t mark_h = text_h;

    int32_t x1 = TOF_TP_X1 - margin;
    int32_t y0 = TOF_TP_Y0 + margin;
    int32_t x0 = x1 - mark_w + 1;
    int32_t y1 = y0 + mark_h - 1;
    x0 = tof_clamp_i32(x0, TOF_TP_X0, TOF_TP_X1);
    y0 = tof_clamp_i32(y0, TOF_TP_Y0, TOF_TP_Y1);
    x1 = tof_clamp_i32(x1, TOF_TP_X0, TOF_TP_X1);
    y1 = tof_clamp_i32(y1, TOF_TP_Y0, TOF_TP_Y1);

    int32_t tx = x0;
    const int32_t ty = y0;
    for (size_t i = 0u; i < n; i++)
    {
        tof_brand_draw_char5x7_scaled_clipped(tx, ty, name[i], fg, (uint32_t)scale, x0, y0, x1, y1);
        tx += char_adv;
    }

    s_brand_drawn = true;
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

static uint8_t tof_roll_segments_from_fullness_q10(uint32_t fullness_q10)
{
    if (fullness_q10 > 1024u)
    {
        fullness_q10 = 1024u;
    }

    uint32_t seg = (fullness_q10 + 64u) / 128u;
    if (seg > TOF_ROLL_SEGMENT_COUNT)
    {
        seg = TOF_ROLL_SEGMENT_COUNT;
    }
    return (uint8_t)seg;
}

static tof_roll_alert_level_t tof_roll_alert_level_from_mm_segments(uint16_t mm,
                                                                     uint8_t segments,
                                                                     tof_roll_alert_level_t prev,
                                                                     bool prev_valid)
{
    if (segments > TOF_ROLL_SEGMENT_COUNT)
    {
        segments = TOF_ROLL_SEGMENT_COUNT;
    }

    if (mm == 0u)
    {
        return kTofRollAlertFull;
    }

    if (segments == 0u)
    {
        return kTofRollAlertEmpty;
    }

    if (prev_valid && prev == kTofRollAlertEmpty)
    {
        if ((mm > TOF_ROLL_EMPTY_EXIT_MM) || (segments == 0u))
        {
            return kTofRollAlertEmpty;
        }
    }
    else
    {
        if ((mm >= TOF_ROLL_EMPTY_ENTER_MM) && (segments <= 1u))
        {
            return kTofRollAlertEmpty;
        }
    }

    if (!prev_valid)
    {
        if (segments >= TOF_ROLL_SEG_FULL_MIN)
        {
            return kTofRollAlertFull;
        }
        if (segments >= TOF_ROLL_SEG_MED_MIN)
        {
            return kTofRollAlertMedium;
        }
        return kTofRollAlertLow;
    }

    switch (prev)
    {
        case kTofRollAlertFull:
            if (segments >= TOF_ROLL_SEG_FULL_TO_MED_EXIT)
            {
                return kTofRollAlertFull;
            }
            if (segments >= TOF_ROLL_SEG_MED_MIN)
            {
                return kTofRollAlertMedium;
            }
            return kTofRollAlertLow;
        case kTofRollAlertMedium:
            if (segments >= TOF_ROLL_SEG_MED_TO_FULL_ENTER)
            {
                return kTofRollAlertFull;
            }
            if (segments > TOF_ROLL_SEG_MED_TO_LOW_EXIT)
            {
                return kTofRollAlertMedium;
            }
            return kTofRollAlertLow;
        case kTofRollAlertLow:
            if (segments >= TOF_ROLL_SEG_MED_TO_FULL_ENTER)
            {
                return kTofRollAlertFull;
            }
            if (segments >= TOF_ROLL_SEG_LOW_TO_MED_ENTER)
            {
                return kTofRollAlertMedium;
            }
            return kTofRollAlertLow;
        case kTofRollAlertEmpty:
        default:
            if (segments >= TOF_ROLL_SEG_FULL_MIN)
            {
                return kTofRollAlertFull;
            }
            if (segments >= TOF_ROLL_SEG_MED_MIN)
            {
                return kTofRollAlertMedium;
            }
            if (segments >= 1u)
            {
                return kTofRollAlertLow;
            }
            return kTofRollAlertEmpty;
    }
}

static void tof_roll_status_style(tof_roll_alert_level_t level,
                                  const char **label,
                                  uint16_t *bg,
                                  uint16_t *border,
                                  uint16_t *fg)
{
    const char *text = "ROLL FULL";
    uint16_t c_bg = pack_rgb565(22u, 82u, 28u);
    uint16_t c_border = pack_rgb565(118u, 210u, 126u);
    uint16_t c_fg = pack_rgb565(236u, 242u, 240u);

    switch (level)
    {
        case kTofRollAlertMedium:
            text = "ROLL MEDIUM";
            c_bg = pack_rgb565(98u, 82u, 18u);
            c_border = pack_rgb565(232u, 202u, 104u);
            c_fg = pack_rgb565(248u, 236u, 206u);
            break;
        case kTofRollAlertLow:
            text = "ROLL LOW";
            c_bg = pack_rgb565(128u, 106u, 20u);
            c_border = pack_rgb565(246u, 224u, 98u);
            c_fg = pack_rgb565(254u, 246u, 214u);
            break;
        case kTofRollAlertEmpty:
            text = "ROLL EMPTY";
            c_bg = pack_rgb565(86u, 8u, 8u);
            c_border = pack_rgb565(246u, 104u, 104u);
            c_fg = pack_rgb565(254u, 228u, 228u);
            break;
        case kTofRollAlertFull:
        default:
            break;
    }

    if (label != NULL)
    {
        *label = text;
    }
    if (bg != NULL)
    {
        *bg = c_bg;
    }
    if (border != NULL)
    {
        *border = c_border;
    }
    if (fg != NULL)
    {
        *fg = c_fg;
    }
}

static const char *tof_roll_popup_message(tof_roll_alert_level_t level)
{
    if (level == kTofRollAlertLow)
    {
        return "WARNING ROLL LOW";
    }
    if (level == kTofRollAlertEmpty)
    {
        return "WARNING ROLL EMPTY";
    }
    return "ROLL STATUS";
}

static void tof_draw_ai_pill(bool ai_on)
{
    if (!s_dbg_force_redraw && s_ai_pill_prev_valid && (s_ai_pill_prev_on == ai_on))
    {
        return;
    }

    const uint16_t bg = ai_on ? pack_rgb565(22u, 86u, 46u) : pack_rgb565(92u, 22u, 24u);
    const uint16_t border = ai_on ? pack_rgb565(120u, 210u, 140u) : pack_rgb565(220u, 120u, 120u);
    const uint16_t fg = pack_rgb565(238u, 242u, 246u);

    display_hal_fill_rect(s_ai_pill_x0, s_ai_pill_y0, s_ai_pill_x1, s_ai_pill_y1, bg);
    display_hal_fill_rect(s_ai_pill_x0, s_ai_pill_y0, s_ai_pill_x1, s_ai_pill_y0, border);
    display_hal_fill_rect(s_ai_pill_x0, s_ai_pill_y1, s_ai_pill_x1, s_ai_pill_y1, border);
    display_hal_fill_rect(s_ai_pill_x0, s_ai_pill_y0, s_ai_pill_x0, s_ai_pill_y1, border);
    display_hal_fill_rect(s_ai_pill_x1, s_ai_pill_y0, s_ai_pill_x1, s_ai_pill_y1, border);

    const char *label = ai_on ? "AI ON" : "AI OFF";
    const size_t n = strlen(label);
    const int32_t text_w = (n > 0u) ? ((int32_t)(n * TOF_DBG_CHAR_ADV) - 2) : 0;
    const int32_t text_h = TOF_DBG_CHAR_H * TOF_DBG_SCALE;
    int32_t x = s_ai_pill_x0 + ((s_ai_pill_x1 - s_ai_pill_x0 + 1 - text_w) / 2);
    int32_t y = s_ai_pill_y0 + ((s_ai_pill_y1 - s_ai_pill_y0 + 1 - text_h) / 2);
    if (x < (s_ai_pill_x0 + 2))
    {
        x = s_ai_pill_x0 + 2;
    }
    if (y < (s_ai_pill_y0 + 1))
    {
        y = s_ai_pill_y0 + 1;
    }

    for (size_t i = 0u; i < n; i++)
    {
        tof_dbg_draw_char(x, y, label[i], fg);
        x += TOF_DBG_CHAR_ADV;
    }

    s_ai_pill_prev_valid = true;
    s_ai_pill_prev_on = ai_on;
}

static void tof_draw_alert_pill(bool alert_on)
{
    if (!s_dbg_force_redraw && s_alert_pill_prev_valid && (s_alert_pill_prev_on == alert_on))
    {
        return;
    }

    const uint16_t bg = alert_on ? pack_rgb565(78u, 62u, 18u) : pack_rgb565(48u, 26u, 18u);
    const uint16_t border = alert_on ? pack_rgb565(232u, 198u, 98u) : pack_rgb565(202u, 128u, 104u);
    const uint16_t fg = pack_rgb565(246u, 238u, 218u);
    const char *label = alert_on ? "ALERT ON" : "ALERT OFF";

    display_hal_fill_rect(s_alert_pill_x0, s_alert_pill_y0, s_alert_pill_x1, s_alert_pill_y1, bg);
    display_hal_fill_rect(s_alert_pill_x0, s_alert_pill_y0, s_alert_pill_x1, s_alert_pill_y0, border);
    display_hal_fill_rect(s_alert_pill_x0, s_alert_pill_y1, s_alert_pill_x1, s_alert_pill_y1, border);
    display_hal_fill_rect(s_alert_pill_x0, s_alert_pill_y0, s_alert_pill_x0, s_alert_pill_y1, border);
    display_hal_fill_rect(s_alert_pill_x1, s_alert_pill_y0, s_alert_pill_x1, s_alert_pill_y1, border);

    const size_t n = strlen(label);
    const int32_t text_w = (n > 0u) ? ((int32_t)(n * TOF_DBG_CHAR_ADV) - 2) : 0;
    const int32_t text_h = TOF_DBG_CHAR_H * TOF_DBG_SCALE;
    int32_t x = s_alert_pill_x0 + ((s_alert_pill_x1 - s_alert_pill_x0 + 1 - text_w) / 2);
    int32_t y = s_alert_pill_y0 + ((s_alert_pill_y1 - s_alert_pill_y0 + 1 - text_h) / 2);
    if (x < (s_alert_pill_x0 + 2))
    {
        x = s_alert_pill_x0 + 2;
    }
    if (y < (s_alert_pill_y0 + 1))
    {
        y = s_alert_pill_y0 + 1;
    }

    for (size_t i = 0u; i < n; i++)
    {
        tof_tiny_draw_char_clipped(x, y, label[i], fg, s_alert_pill_x0, s_alert_pill_y0, s_alert_pill_x1, s_alert_pill_y1);
        x += TOF_DBG_CHAR_ADV;
    }

    s_alert_pill_prev_valid = true;
    s_alert_pill_prev_on = alert_on;
}

static void tof_alert_popup_rect(int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1)
{
    int32_t popup_w = (TOF_LCD_W * TOF_ALERT_POPUP_W_PCT) / 100;
    popup_w = tof_clamp_i32(popup_w, 160, TOF_LCD_W - (2 * TOF_ALERT_POPUP_X_MARGIN));
    int32_t lx0 = (TOF_LCD_W - popup_w) / 2;
    int32_t lx1 = lx0 + popup_w - 1;
    int32_t ly0 = (TOF_LCD_H - TOF_ALERT_POPUP_H) / 2;
    int32_t ly1 = ly0 + TOF_ALERT_POPUP_H - 1;

    lx0 = tof_clamp_i32(lx0, 0, TOF_LCD_W - 1);
    lx1 = tof_clamp_i32(lx1, 0, TOF_LCD_W - 1);
    ly0 = tof_clamp_i32(ly0, 0, TOF_LCD_H - 1);
    ly1 = tof_clamp_i32(ly1, 0, TOF_LCD_H - 1);
    if (ly1 <= ly0)
    {
        ly0 = 2;
        ly1 = ly0 + TOF_ALERT_POPUP_H - 1;
        if (ly1 >= TOF_LCD_H)
        {
            ly1 = TOF_LCD_H - 1;
            ly0 = ly1 - TOF_ALERT_POPUP_H + 1;
            ly0 = tof_clamp_i32(ly0, 0, TOF_LCD_H - 1);
        }
    }

    if (x0 != NULL)
    {
        *x0 = lx0;
    }
    if (y0 != NULL)
    {
        *y0 = ly0;
    }
    if (x1 != NULL)
    {
        *x1 = lx1;
    }
    if (y1 != NULL)
    {
        *y1 = ly1;
    }
}

static void tof_scrub_left_panel(bool force_debug_redraw)
{
    display_hal_fill_rect(TOF_Q1_X0, TOF_Q1_TOP_Y0, TOF_Q1_X1, TOF_Q1_TOP_Y1, s_ui_bg);
    display_hal_fill_rect(TOF_Q1_X0, TOF_Q1_BOT_Y0, TOF_Q1_X1, TOF_Q1_BOT_Y1, s_ui_bg);
    display_hal_fill_rect(s_dbg_x0, TOF_Q1_BOT_Y0, s_dbg_x1, TOF_Q1_BOT_Y0, pack_rgb565(28u, 32u, 36u));
    display_hal_fill_rect(s_dbg_x0, s_dbg_y0, s_dbg_x1, s_dbg_y1, s_ui_dbg_bg);
    display_hal_fill_rect(s_dbg_x0, s_dbg_y0, s_dbg_x1, s_dbg_y0, pack_rgb565(32u, 38u, 44u));

    for (uint32_t i = 0u; i < 64u; i++)
    {
        const tof_cell_rect_t *c = &s_cells[i];
        display_hal_fill_rect(c->x0, c->y0, c->x1, c->y1, s_ui_border);
        display_hal_fill_rect(c->ix0, c->iy0, c->ix1, c->iy1, s_ui_bg);
    }

    memset(s_cell_drawn, 0, sizeof(s_cell_drawn));
    memset(s_last_cell_color, 0, sizeof(s_last_cell_color));
    s_hot_idx = -1;
    for (uint32_t y = 0u; y < TOF_GRID_H; y++)
    {
        s_curve_pick_idx[y] = -1;
        s_curve_prev_pick_idx[y] = -1;
    }

    if (force_debug_redraw)
    {
        memset(s_dbg_prev, 0, sizeof(s_dbg_prev));
        s_dbg_force_redraw = true;
        s_ai_pill_prev_valid = false;
        s_alert_pill_prev_valid = false;
    }
}

static void tof_invalidate_after_popup_overlay(void)
{
    tof_scrub_left_panel(true);
    display_hal_fill_rect(TOF_TP_X0, TOF_TP_Y0, TOF_TP_X1, TOF_TP_Y1, tof_tp_bg_color(255u, true));
    display_hal_fill_rect(TOF_Q_W, 0, TOF_Q_W, TOF_LCD_H - 1, pack_rgb565(20u, 24u, 28u));
    s_roll_status_prev_valid = false;
    s_tp_force_redraw = true;
    s_tp_prev_rect_valid = false;
}

static void tof_clear_roll_status_popup(bool live_data)
{
    (void)live_data;

    if (!s_alert_popup_prev_drawn)
    {
        return;
    }

    tof_invalidate_after_popup_overlay();
    s_alert_popup_prev_drawn = false;
}

static void tof_draw_roll_status_popup(bool live_data, tof_roll_alert_level_t level)
{
    int32_t x0 = 0;
    int32_t y0 = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    tof_alert_popup_rect(&x0, &y0, &x1, &y1);
    if (x1 < x0 || y1 < y0)
    {
        return;
    }

    if (s_alert_popup_prev_drawn &&
        (s_alert_popup_prev_x0 != x0 || s_alert_popup_prev_y0 != y0 ||
         s_alert_popup_prev_x1 != x1 || s_alert_popup_prev_y1 != y1))
    {
        tof_clear_roll_status_popup(live_data);
    }

    const char *subtitle = (level == kTofRollAlertEmpty) ? "ROLL EMPTY" : "ROLL LOW";
    const char *fallback = tof_roll_popup_message(level);
    const bool two_line = (level == kTofRollAlertLow) || (level == kTofRollAlertEmpty);
    uint16_t bg = 0u;
    uint16_t border = 0u;
    uint16_t fg = 0u;
    tof_roll_status_style(level, NULL, &bg, &border, &fg);

    uint16_t frame_color = border;
    uint16_t panel_bg = bg;
    uint16_t title_fg = pack_rgb565(255u, 248u, 224u);
    uint16_t sub_fg = fg;
    const uint16_t shadow_fg = pack_rgb565(10u, 10u, 12u);

    if (level == kTofRollAlertEmpty)
    {
        frame_color = pack_rgb565(248u, 140u, 140u);
        panel_bg = pack_rgb565(72u, 14u, 16u);
        title_fg = pack_rgb565(255u, 234u, 224u);
        sub_fg = pack_rgb565(255u, 214u, 206u);
    }
    else if (level == kTofRollAlertLow)
    {
        frame_color = pack_rgb565(246u, 214u, 96u);
        panel_bg = pack_rgb565(92u, 76u, 20u);
        title_fg = pack_rgb565(255u, 248u, 218u);
        sub_fg = pack_rgb565(252u, 240u, 190u);
    }

    display_hal_fill_rect(x0, y0, x1, y1, frame_color);
    if ((x1 - x0) >= 2 && (y1 - y0) >= 2)
    {
        display_hal_fill_rect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, panel_bg);
    }

    const int32_t ix0 = x0 + 1;
    const int32_t iy0 = y0 + 1;
    const int32_t ix1 = x1 - 1;

    if (two_line)
    {
        const char *hint = (level == kTofRollAlertEmpty) ? "Replace Spool To Reset" : NULL;
        const size_t hint_n = (hint != NULL) ? strlen(hint) : 0u;
        const int32_t avail_w = (ix1 - ix0 + 1) - 4;
        uint32_t title_scale = 4u;
        uint32_t sub_scale = 3u;
        uint32_t hint_scale = 2u;
        int32_t title_adv = (TOF_DBG_CHAR_W * (int32_t)title_scale) + 2;
        int32_t sub_adv = (TOF_DBG_CHAR_W * (int32_t)sub_scale) + 2;
        int32_t hint_adv = (TOF_DBG_CHAR_W * (int32_t)hint_scale) + 2;
        const size_t title_n = strlen("WARNING");
        const size_t sub_n = strlen(subtitle);
        int32_t title_w = (title_n > 0u) ? ((int32_t)(title_n * (size_t)title_adv) - 2) : 0;
        int32_t sub_w = (sub_n > 0u) ? ((int32_t)(sub_n * (size_t)sub_adv) - 2) : 0;
        int32_t hint_w = (hint_n > 0u) ? ((int32_t)(hint_n * (size_t)hint_adv) - 2) : 0;
        if (title_w > avail_w)
        {
            title_scale = 2u;
            title_adv = (TOF_DBG_CHAR_W * (int32_t)title_scale) + 2;
            title_w = (title_n > 0u) ? ((int32_t)(title_n * (size_t)title_adv) - 2) : 0;
        }
        if (sub_w > avail_w)
        {
            sub_scale = 1u;
            sub_adv = (TOF_DBG_CHAR_W * (int32_t)sub_scale) + 2;
            sub_w = (sub_n > 0u) ? ((int32_t)(sub_n * (size_t)sub_adv) - 2) : 0;
        }
        if (hint_w > avail_w)
        {
            hint_scale = 1u;
            hint_adv = (TOF_DBG_CHAR_W * (int32_t)hint_scale) + 2;
            hint_w = (hint_n > 0u) ? ((int32_t)(hint_n * (size_t)hint_adv) - 2) : 0;
            if (hint_w > avail_w)
            {
                hint_w = avail_w;
            }
        }

        const int32_t title_h = TOF_DBG_CHAR_H * (int32_t)title_scale;
        const int32_t sub_h = TOF_DBG_CHAR_H * (int32_t)sub_scale;
        const int32_t hint_h = (hint_n > 0u) ? (TOF_DBG_CHAR_H * (int32_t)hint_scale) : 0;
        const int32_t gap = 5;
        const int32_t hint_gap = (hint_n > 0u) ? 3 : 0;
        const int32_t total_h = title_h + gap + sub_h + hint_gap + hint_h;
        int32_t ty0 = y0 + ((y1 - y0 + 1 - total_h) / 2);
        if (ty0 < (iy0 + 1))
        {
            ty0 = iy0 + 1;
        }
        int32_t tx0 = x0 + ((x1 - x0 + 1 - title_w) / 2);
        if (tx0 < (ix0 + 1))
        {
            tx0 = ix0 + 1;
        }
        for (size_t i = 0u; i < title_n; i++)
        {
            tof_tiny_draw_char_scaled_clipped(tx0 + 1,
                                              ty0 + 1,
                                              "WARNING"[i],
                                              shadow_fg,
                                              title_scale,
                                              x0,
                                              y0,
                                              x1,
                                              y1);
            tof_tiny_draw_char_scaled_clipped(tx0,
                                              ty0,
                                              "WARNING"[i],
                                              title_fg,
                                              title_scale,
                                              x0,
                                              y0,
                                              x1,
                                              y1);
            tx0 += title_adv;
        }

        int32_t sy = ty0 + title_h + gap;
        if (sy < (iy0 + 1))
        {
            sy = iy0 + 1;
        }
        int32_t sx = x0 + ((x1 - x0 + 1 - sub_w) / 2);
        if (sx < (ix0 + 1))
        {
            sx = ix0 + 1;
        }
        for (size_t i = 0u; i < sub_n; i++)
        {
            tof_tiny_draw_char_scaled_clipped(sx + 1, sy + 1, subtitle[i], shadow_fg, sub_scale, x0, y0, x1, y1);
            tof_tiny_draw_char_scaled_clipped(sx, sy, subtitle[i], sub_fg, sub_scale, x0, y0, x1, y1);
            sx += sub_adv;
        }

        if (hint_n > 0u)
        {
            int32_t hy = sy + sub_h + hint_gap;
            if (hy < (iy0 + 1))
            {
                hy = iy0 + 1;
            }
            int32_t hx = x0 + ((x1 - x0 + 1 - hint_w) / 2);
            if (hx < (ix0 + 1))
            {
                hx = ix0 + 1;
            }
            const uint16_t hint_fg = pack_rgb565(255u, 206u, 206u);
            int32_t max_w = ix1 - hx + 1;
            int32_t draw_chars = max_w / hint_adv;
            if (draw_chars < 0)
            {
                draw_chars = 0;
            }
            if ((size_t)draw_chars > hint_n)
            {
                draw_chars = (int32_t)hint_n;
            }
            for (int32_t i = 0; i < draw_chars; i++)
            {
                tof_tiny_draw_char_scaled_clipped(hx + 1,
                                                  hy + 1,
                                                  hint[(size_t)i],
                                                  shadow_fg,
                                                  hint_scale,
                                                  x0,
                                                  y0,
                                                  x1,
                                                  y1);
                tof_tiny_draw_char_scaled_clipped(hx,
                                                  hy,
                                                  hint[(size_t)i],
                                                  hint_fg,
                                                  hint_scale,
                                                  x0,
                                                  y0,
                                                  x1,
                                                  y1);
                hx += hint_adv;
            }
        }
    }
    else
    {
        const size_t n = strlen(fallback);
        const int32_t text_w = (n > 0u) ? ((int32_t)(n * TOF_DBG_CHAR_ADV) - 2) : 0;
        const int32_t text_h = TOF_DBG_CHAR_H * TOF_DBG_SCALE;
        int32_t tx = x0 + ((x1 - x0 + 1 - text_w) / 2);
        int32_t ty = y0 + ((y1 - y0 + 1 - text_h) / 2);
        if (tx < (ix0 + 1))
        {
            tx = ix0 + 1;
        }
        if (ty < (iy0 + 1))
        {
            ty = iy0 + 1;
        }

        for (size_t i = 0u; i < n; i++)
        {
            tof_tiny_draw_char_clipped(tx + 1, ty + 1, fallback[i], shadow_fg, x0, y0, x1, y1);
            tof_tiny_draw_char_clipped(tx, ty, fallback[i], sub_fg, x0, y0, x1, y1);
            tx += TOF_DBG_CHAR_ADV;
        }
    }

    s_alert_popup_prev_drawn = true;
    s_alert_popup_prev_level = level;
    s_alert_popup_last_live = live_data;
    s_alert_popup_prev_x0 = (int16_t)x0;
    s_alert_popup_prev_y0 = (int16_t)y0;
    s_alert_popup_prev_x1 = (int16_t)x1;
    s_alert_popup_prev_y1 = (int16_t)y1;
}

static void tof_update_roll_alert_ui(uint32_t fullness_q10, bool live_data, uint32_t tick)
{
    if (fullness_q10 > 1024u)
    {
        fullness_q10 = 1024u;
    }

    uint16_t level_mm = s_roll_model_mm;
    if (level_mm == 0u)
    {
        level_mm = s_tp_live_actual_mm;
    }
    const uint8_t segments = tof_roll_segments_from_fullness_q10(fullness_q10);
    tof_roll_alert_level_t level = tof_roll_alert_level_from_mm_segments(level_mm,
                                                                          segments,
                                                                          s_roll_level_stable,
                                                                          s_roll_level_stable_valid);

    if (!s_roll_level_stable_valid)
    {
        s_roll_level_stable = level;
        s_roll_level_stable_valid = true;
        s_roll_level_candidate = level;
        s_roll_level_candidate_count = TOF_ROLL_LEVEL_CONSENSUS_FRAMES;
    }
    else if (level != s_roll_level_stable)
    {
        if (level == s_roll_level_candidate)
        {
            if (s_roll_level_candidate_count < 255u)
            {
                s_roll_level_candidate_count++;
            }
        }
        else
        {
            s_roll_level_candidate = level;
            s_roll_level_candidate_count = 1u;
        }

        if (s_roll_level_candidate_count >= TOF_ROLL_LEVEL_CONSENSUS_FRAMES)
        {
            s_roll_level_stable = level;
            s_roll_level_candidate_count = 0u;
        }
        else
        {
            level = s_roll_level_stable;
        }
    }
    else
    {
        s_roll_level_candidate = level;
        s_roll_level_candidate_count = 0u;
    }

    const bool warning_level = (level == kTofRollAlertLow) || (level == kTofRollAlertEmpty);
    const bool hard_empty_popup = (level == kTofRollAlertEmpty);
    tof_draw_alert_pill(s_alert_runtime_on);
    tof_draw_roll_status_banner(level, live_data);

    const bool level_changed = (!s_roll_alert_prev_valid || (s_roll_alert_prev_level != level));
    if (level_changed)
    {
        s_roll_alert_prev_level = level;
        s_roll_alert_prev_valid = true;
    }

    const bool live_full_sample = (((s_tp_live_actual_mm > 0u) &&
                                    (s_tp_live_actual_mm <= TOF_ROLL_FULL_CAPTURE_MM)) ||
                                   ((s_tp_live_closest_mm > 0u) &&
                                    (s_tp_live_closest_mm <= TOF_ROLL_FULL_CAPTURE_MM)) ||
                                   ((s_roll_model_mm > 0u) &&
                                    (s_roll_model_mm <= TOF_ROLL_FULL_CAPTURE_MM)));
    if (live_full_sample)
    {
        if (s_roll_full_rearm_streak < 255u)
        {
            s_roll_full_rearm_streak++;
        }
    }
    else
    {
        s_roll_full_rearm_streak = 0u;
    }

    const bool live_full_reset = (s_roll_full_rearm_streak >= TOF_ROLL_FULL_REARM_STREAK);
    const bool level_full_valid = (level_mm > 0u) &&
                                  (level_mm <= TOF_ROLL_MEDIUM_TRIGGER_MM);
    const bool forced_full_capture = s_alert_popup_hold_empty &&
                                     (s_tp_live_closest_mm > 0u) &&
                                     (s_tp_live_closest_mm <= TOF_ROLL_FULL_CAPTURE_MM);
    const bool full_reset = live_full_reset || level_full_valid || forced_full_capture;
    if (full_reset)
    {
        s_roll_full_rearm_streak = 0u;
        s_alert_low_popup_shown = false;
        s_alert_popup_hold_empty = false;
        s_alert_popup_rearm_on_full = true;
        s_alert_popup_active = false;
    }

    if (!s_alert_runtime_on)
    {
        s_alert_popup_active = false;
        s_alert_popup_hold_empty = false;
        if (s_alert_popup_prev_drawn || s_dbg_force_redraw)
        {
            tof_clear_roll_status_popup(live_data);
        }
        return;
    }

    if (s_alert_popup_hold_empty)
    {
        s_alert_popup_active = true;
        s_alert_popup_level = kTofRollAlertEmpty;
    }
    else if (hard_empty_popup)
    {
        s_alert_popup_active = true;
        s_alert_popup_level = kTofRollAlertEmpty;
        s_alert_popup_until_tick = 0u;
        s_alert_popup_prev_drawn = false;
        s_alert_popup_hold_empty = true;
        s_alert_popup_rearm_on_full = false;
    }
    else if (level_changed &&
             warning_level &&
             s_alert_popup_rearm_on_full &&
             ((level != kTofRollAlertLow) || !s_alert_low_popup_shown))
    {
        s_alert_popup_active = true;
        s_alert_popup_level = (level == kTofRollAlertEmpty) ? kTofRollAlertLow : level;
        s_alert_popup_until_tick = tick + TOF_ALERT_POPUP_TICKS;
        s_alert_popup_prev_drawn = false;
        s_alert_popup_rearm_on_full = false;
        if (level == kTofRollAlertLow)
        {
            s_alert_low_popup_shown = true;
        }
    }

    if (s_alert_popup_active &&
        !s_alert_popup_hold_empty &&
        ((int32_t)(tick - s_alert_popup_until_tick) >= 0))
    {
        s_alert_popup_active = false;
    }

    if (s_alert_popup_active)
    {
        /* Keep popup topmost even while spool redraws underneath. */
        tof_draw_roll_status_popup(live_data, s_alert_popup_level);
    }
    else if (s_alert_popup_prev_drawn)
    {
        tof_clear_roll_status_popup(live_data);
    }
}

static void tof_touch_delay_ms(uint32_t delay_ms)
{
    SDK_DelayAtLeastUs(delay_ms * 1000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
}

static status_t tof_touch_i2c_send(uint8_t deviceAddress,
                                   uint32_t subAddress,
                                   uint8_t subaddressSize,
                                   const uint8_t *txBuff,
                                   uint8_t txBuffSize)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.flags = kLPI2C_TransferDefaultFlag;
    xfer.slaveAddress = deviceAddress;
    xfer.direction = kLPI2C_Write;
    xfer.subaddress = subAddress;
    xfer.subaddressSize = subaddressSize;
    xfer.data = (uint8_t *)(uintptr_t)txBuff;
    xfer.dataSize = txBuffSize;
    return LPI2C_MasterTransferBlocking(TOF_TOUCH_I2C, &xfer);
}

static status_t tof_touch_i2c_receive(uint8_t deviceAddress,
                                      uint32_t subAddress,
                                      uint8_t subaddressSize,
                                      uint8_t *rxBuff,
                                      uint8_t rxBuffSize)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.flags = kLPI2C_TransferDefaultFlag;
    xfer.slaveAddress = deviceAddress;
    xfer.direction = kLPI2C_Read;
    xfer.subaddress = subAddress;
    xfer.subaddressSize = subaddressSize;
    xfer.data = rxBuff;
    xfer.dataSize = rxBuffSize;
    return LPI2C_MasterTransferBlocking(TOF_TOUCH_I2C, &xfer);
}

static void tof_touch_config_int_pin(gt911_int_pin_mode_t mode)
{
    CLOCK_EnableClock(kCLOCK_Port4);

    port_pin_config_t cfg = {
        .pullSelect = kPORT_PullDown,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainDisable,
        .driveStrength = kPORT_LowDriveStrength,
#if defined(FSL_FEATURE_PORT_HAS_DRIVE_STRENGTH1) && FSL_FEATURE_PORT_HAS_DRIVE_STRENGTH1
        .driveStrength1 = kPORT_NormalDriveStrength,
#endif
        .mux = kPORT_MuxAlt0,
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };

    switch (mode)
    {
        case kGT911_IntPinPullUp:
            cfg.pullSelect = kPORT_PullUp;
            break;
        case kGT911_IntPinPullDown:
            cfg.pullSelect = kPORT_PullDown;
            break;
        case kGT911_IntPinInput:
            cfg.pullSelect = kPORT_PullDisable;
            break;
        default:
            break;
    }

    PORT_SetPinConfig(TOF_TOUCH_INT_PORT, TOF_TOUCH_INT_PIN, &cfg);
}

static void tof_touch_config_reset_pin(bool pullUp)
{
    (void)pullUp;
}

static void tof_touch_init(void)
{
    s_touch_ready = false;
    s_touch_was_down = false;
    s_touch_last_poll_tick = 0u;

    gt911_config_t cfg = {
        .I2C_SendFunc = tof_touch_i2c_send,
        .I2C_ReceiveFunc = tof_touch_i2c_receive,
        .timeDelayMsFunc = tof_touch_delay_ms,
        .intPinFunc = tof_touch_config_int_pin,
        .pullResetPinFunc = tof_touch_config_reset_pin,
        .touchPointNum = TOF_TOUCH_POINTS,
        .i2cAddrMode = kGT911_I2cAddrAny,
        .intTrigMode = kGT911_IntFallingEdge,
    };

    const status_t st = GT911_Init(&s_touch_handle, &cfg);
    if (st == kStatus_Success)
    {
        s_touch_ready = true;
        PRINTF("TOF touch: GT911 ready (%u x %u)\r\n",
               (unsigned)s_touch_handle.resolutionX,
               (unsigned)s_touch_handle.resolutionY);
    }
    else
    {
        PRINTF("TOF touch: GT911 init failed: %d\r\n", (int)st);
    }
}

static bool tof_touch_get_point(int32_t *x_out, int32_t *y_out)
{
    if (!s_touch_ready || x_out == NULL || y_out == NULL)
    {
        return false;
    }

    touch_point_t points[TOF_TOUCH_POINTS];
    uint8_t point_count = TOF_TOUCH_POINTS;
    const status_t st = GT911_GetMultiTouch(&s_touch_handle, &point_count, points);
    if (st != kStatus_Success)
    {
        return false;
    }

    const touch_point_t *selected = NULL;
    for (uint8_t i = 0u; i < point_count; i++)
    {
        if (points[i].valid && points[i].touchID == 0u)
        {
            selected = &points[i];
            break;
        }
    }
    if (selected == NULL)
    {
        for (uint8_t i = 0u; i < point_count; i++)
        {
            if (points[i].valid)
            {
                selected = &points[i];
                break;
            }
        }
    }
    if (selected == NULL)
    {
        return false;
    }

    const int32_t res_x = (s_touch_handle.resolutionX > 0u) ? (int32_t)s_touch_handle.resolutionX : TOF_LCD_W;
    int32_t x = (int32_t)selected->y;
    int32_t y = res_x - (int32_t)selected->x;
    x = tof_clamp_i32(x, 0, TOF_LCD_W - 1);
    y = tof_clamp_i32(y, 0, TOF_LCD_H - 1);

    *x_out = x;
    *y_out = y;
    return true;
}

static void tof_touch_poll_ai_toggle(uint32_t tick)
{
    if (!s_touch_ready)
    {
        return;
    }

    if ((uint32_t)(tick - s_touch_last_poll_tick) < TOF_TOUCH_POLL_TICKS)
    {
        return;
    }
    s_touch_last_poll_tick = tick;

    int32_t tx = 0;
    int32_t ty = 0;
    const bool pressed = tof_touch_get_point(&tx, &ty);
    const bool in_ai_pill = pressed &&
                            tx >= s_ai_pill_x0 && tx <= s_ai_pill_x1 &&
                            ty >= s_ai_pill_y0 && ty <= s_ai_pill_y1;
    const bool in_alert_pill = pressed &&
                               tx >= s_alert_pill_x0 && tx <= s_alert_pill_x1 &&
                               ty >= s_alert_pill_y0 && ty <= s_alert_pill_y1;

    if (in_ai_pill && !s_touch_was_down)
    {
        s_ai_runtime_on = !s_ai_runtime_on;
        tof_ai_grid_reset();
        s_dbg_force_redraw = true;
        s_tp_force_redraw = true;
        s_ai_pill_prev_valid = false;
        s_alert_pill_prev_valid = false;
        PRINTF("TOF AI: %s\r\n", s_ai_runtime_on ? "ON" : "OFF");
    }
    else if (in_alert_pill && !s_touch_was_down)
    {
        s_alert_runtime_on = !s_alert_runtime_on;
        s_dbg_force_redraw = true;
        s_alert_pill_prev_valid = false;
        s_roll_alert_prev_valid = false;
        if (!s_alert_runtime_on)
        {
            s_alert_popup_active = false;
            s_alert_popup_hold_empty = false;
            s_roll_full_rearm_streak = 0u;
        }
        PRINTF("TOF ALERT: %s\r\n", s_alert_runtime_on ? "ON" : "OFF");
    }

    s_touch_was_down = pressed;
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

static const uint16_t *tof_calc_metric_frame(const uint16_t mm[64], bool live_data)
{
    if (!live_data)
    {
        return mm;
    }

    static uint16_t metric_mm[64];
    uint16_t repaired_mm[64];
    tof_ai_denoise_heatmap_frame(mm, metric_mm, live_data);
    tof_fill_display_holes(metric_mm, repaired_mm);
    memcpy(metric_mm, repaired_mm, sizeof(repaired_mm));
    tof_repair_corner_blindspots(metric_mm);

    uint32_t filtered_valid = 0u;
    tof_calc_frame_stats(metric_mm, &filtered_valid, NULL, NULL, NULL);
    if (filtered_valid >= TOF_EST_VALID_MIN)
    {
        return metric_mm;
    }

    return mm;
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

static uint16_t tof_calc_closest_valid_mm(const uint16_t mm[64])
{
    uint16_t closest = 0xFFFFu;
    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t v = mm[i];
        if (!tof_mm_valid(v))
        {
            continue;
        }
        if (v < closest)
        {
            closest = v;
        }
    }

    return (closest == 0xFFFFu) ? 0u : closest;
}

static uint16_t tof_apply_tp_mm_shift(uint16_t mm)
{
    if (mm == 0u)
    {
        return 0u;
    }

    int32_t shifted = (int32_t)mm + TOF_TP_MM_SHIFT;
    if (shifted < (int32_t)TOF_TP_MM_CLIP_MIN)
    {
        shifted = (int32_t)TOF_TP_MM_CLIP_MIN;
    }
    if (shifted > (int32_t)TOF_TP_MM_CLIP_MAX)
    {
        shifted = (int32_t)TOF_TP_MM_CLIP_MAX;
    }
    return (uint16_t)shifted;
}

static uint16_t tof_apply_tp_mm_gain(uint16_t mm)
{
    if (mm == 0u)
    {
        return 0u;
    }

    if (mm <= TOF_TP_MM_FULL_NEAR)
    {
        return mm;
    }

    const uint32_t delta = (uint32_t)(mm - TOF_TP_MM_FULL_NEAR);
    uint32_t scaled = ((delta * TOF_TP_MM_GAIN_Q8) + 128u) >> 8;
    uint32_t gained = (uint32_t)TOF_TP_MM_FULL_NEAR + scaled;
    if (gained > TOF_TP_MM_CLIP_MAX)
    {
        gained = TOF_TP_MM_CLIP_MAX;
    }
    return (uint16_t)gained;
}

static uint16_t tof_apply_tp_mm_calibration(uint16_t mm)
{
    return tof_apply_tp_mm_gain(tof_apply_tp_mm_shift(mm));
}

static uint16_t tof_calc_roll_curve_distance_mm(const uint16_t mm[64], int16_t row_pick_idx_out[TOF_GRID_H])
{
    uint16_t row_near_mm[TOF_GRID_H];
    uint32_t row_count = 0u;

    if (row_pick_idx_out != NULL)
    {
        for (uint32_t y = 0u; y < TOF_GRID_H; y++)
        {
            row_pick_idx_out[y] = -1;
        }
    }

    for (uint32_t y = 0u; y < TOF_GRID_H; y++)
    {
        uint16_t low0 = 0xFFFFu;
        uint16_t low1 = 0xFFFFu;
        int32_t low0_idx = -1;
        uint32_t valid = 0u;
        const uint32_t row_base = y * TOF_GRID_W;
        uint32_t x_start = TOF_TP_CURVE_EDGE_GUARD_COLS;
        uint32_t x_end = TOF_GRID_W - TOF_TP_CURVE_EDGE_GUARD_COLS;
        if (x_start >= x_end)
        {
            x_start = 0u;
            x_end = TOF_GRID_W;
        }

        for (uint32_t x = x_start; x < x_end; x++)
        {
            const uint16_t v = mm[row_base + x];
            if (!tof_mm_valid(v))
            {
                continue;
            }

            valid++;
            if (v < low0)
            {
                low1 = low0;
                low0 = v;
                low0_idx = (int32_t)(row_base + x);
            }
            else if (v < low1)
            {
                low1 = v;
            }
        }

        if (valid == 0u || low0 == 0xFFFFu)
        {
            continue;
        }
        if (row_pick_idx_out != NULL)
        {
            row_pick_idx_out[y] = (int16_t)low0_idx;
        }

        uint16_t row_mm = low0;
        if (valid >= 3u && low1 != 0xFFFFu)
        {
            /* Use second-nearest sample per row to reduce single-pixel near outliers. */
            row_mm = low1;
        }
        else if (valid >= 2u && low1 != 0xFFFFu)
        {
            row_mm = (uint16_t)(((uint32_t)low0 + (uint32_t)low1 + 1u) / 2u);
        }
        row_near_mm[row_count++] = row_mm;
    }

    if (row_count < TOF_TP_CURVE_MIN_ROWS)
    {
        return 0u;
    }

    for (uint32_t i = 1u; i < row_count; i++)
    {
        const uint16_t key = row_near_mm[i];
        uint32_t j = i;
        while (j > 0u && row_near_mm[j - 1u] > key)
        {
            row_near_mm[j] = row_near_mm[j - 1u];
            j--;
        }
        row_near_mm[j] = key;
    }

    if ((row_count & 1u) == 0u)
    {
        const uint16_t a = row_near_mm[(row_count / 2u) - 1u];
        const uint16_t b = row_near_mm[row_count / 2u];
        return (uint16_t)(((uint32_t)a + (uint32_t)b + 1u) / 2u);
    }
    return row_near_mm[row_count / 2u];
}

static uint16_t tof_abs_diff_u16(uint16_t a, uint16_t b)
{
    return (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static uint32_t tof_abs_diff_u32(uint32_t a, uint32_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

static uint32_t tof_tp_fullness_q10_from_mm_q8_bounds(uint32_t mm_q8, uint16_t near_mm, uint16_t far_mm)
{
    if (mm_q8 == 0u)
    {
        return 640u;
    }

    if (far_mm <= near_mm)
    {
        far_mm = (uint16_t)(near_mm + 1u);
    }

    const uint32_t near_q8 = ((uint32_t)near_mm << 8);
    const uint32_t far_q8 = ((uint32_t)far_mm << 8);

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

static bool tof_estimator_measure_mm_q8(const uint16_t mm[64],
                                        uint32_t *mm_q8_out,
                                        uint32_t *valid_out,
                                        uint16_t *spread_out)
{
    uint16_t values[64];
    uint32_t count = 0u;
    uint32_t center_sum = 0u;
    uint32_t center_count = 0u;
    uint16_t min_mm = 0xFFFFu;
    uint16_t max_mm = 0u;

    for (uint32_t y = 0u; y < TOF_GRID_H; y++)
    {
        for (uint32_t x = 0u; x < TOF_GRID_W; x++)
        {
            const uint16_t v = mm[(y * TOF_GRID_W) + x];
            if (!tof_mm_valid(v))
            {
                continue;
            }

            values[count++] = v;
            if (v < min_mm)
            {
                min_mm = v;
            }
            if (v > max_mm)
            {
                max_mm = v;
            }
            if ((x >= 2u && x <= 5u) && (y >= 2u && y <= 5u))
            {
                center_sum += v;
                center_count++;
            }
        }
    }

    if (valid_out)
    {
        *valid_out = count;
    }
    if (spread_out)
    {
        *spread_out = (count > 0u) ? (uint16_t)(max_mm - min_mm) : 0u;
    }

    if (count < TOF_EST_VALID_MIN)
    {
        return false;
    }

    for (uint32_t i = 1u; i < count; i++)
    {
        uint16_t key = values[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && values[(uint32_t)j] > key)
        {
            values[(uint32_t)j + 1u] = values[(uint32_t)j];
            j--;
        }
        values[(uint32_t)j + 1u] = key;
    }

    uint32_t trim = count / 8u;
    if ((trim * 2u) >= count)
    {
        trim = 0u;
    }

    uint32_t sum = 0u;
    uint32_t keep = 0u;
    for (uint32_t i = trim; i < (count - trim); i++)
    {
        sum += values[i];
        keep++;
    }
    if (keep == 0u)
    {
        return false;
    }

    uint32_t mean_mm = (sum + (keep / 2u)) / keep;
    if (center_count >= 4u)
    {
        const uint32_t center_mm = (center_sum + (center_count / 2u)) / center_count;
        mean_mm = (((mean_mm * 3u) + (center_mm * 2u)) + 2u) / 5u;
    }

    if (mm_q8_out)
    {
        *mm_q8_out = (mean_mm << 8);
    }
    return true;
}

static uint16_t tof_estimator_confidence_q10(uint32_t valid_count, uint16_t spread_mm, bool live_data)
{
    uint32_t valid_q10 = 0u;
    uint32_t spread_q10 = 0u;

    if (valid_count > TOF_EST_VALID_MIN)
    {
        valid_q10 = ((valid_count - TOF_EST_VALID_MIN) * 1024u) / (64u - TOF_EST_VALID_MIN);
        if (valid_q10 > 1024u)
        {
            valid_q10 = 1024u;
        }
    }

    if (spread_mm <= TOF_EST_SPREAD_GOOD_MM)
    {
        spread_q10 = 1024u;
    }
    else if (spread_mm >= TOF_EST_SPREAD_BAD_MM)
    {
        spread_q10 = 0u;
    }
    else
    {
        spread_q10 =
            (((uint32_t)(TOF_EST_SPREAD_BAD_MM - spread_mm)) * 1024u) /
            ((uint32_t)(TOF_EST_SPREAD_BAD_MM - TOF_EST_SPREAD_GOOD_MM));
    }

    uint32_t conf = (valid_q10 + spread_q10) / 2u;
    if (!live_data)
    {
        conf = (conf * 3u) / 4u;
    }
    if (conf > 1024u)
    {
        conf = 1024u;
    }
    return (uint16_t)conf;
}

static TOF_UNUSED void tof_estimator_update(const uint16_t mm[64], bool live_data)
{
#if TOF_EST_ENABLE
    uint32_t measured_mm_q8 = 0u;
    uint32_t valid_count = 0u;
    uint16_t spread_mm = 0u;
    const bool have_meas = tof_estimator_measure_mm_q8(mm, &measured_mm_q8, &valid_count, &spread_mm);

    if (have_meas)
    {
        const uint16_t conf_q10 = tof_estimator_confidence_q10(valid_count, spread_mm, live_data);
        s_est_valid_count = valid_count;
        s_est_spread_mm = spread_mm;
        s_est_conf_q10 = conf_q10;

        if (s_est_mm_q8 == 0u)
        {
            s_est_mm_q8 = measured_mm_q8;
        }
        else
        {
            const uint32_t delta_q8 = tof_abs_diff_u32(s_est_mm_q8, measured_mm_q8);
            uint32_t den = 8u;
            if ((delta_q8 >= ((uint32_t)TOF_EST_FAST_DELTA_MM << 8)) && (conf_q10 >= 512u))
            {
                den = 2u;
            }
            else if (conf_q10 >= 512u)
            {
                den = 4u;
            }
            else if (conf_q10 >= 256u)
            {
                den = 6u;
            }

            s_est_mm_q8 = ((s_est_mm_q8 * (den - 1u)) + measured_mm_q8 + (den / 2u)) / den;
        }

        uint16_t est_mm = (uint16_t)(s_est_mm_q8 >> 8);
        if (conf_q10 >= TOF_EST_CONF_TRAIN_MIN_Q10)
        {
            if (est_mm <= (uint16_t)(s_est_near_mm + 20u))
            {
                s_est_near_mm = (uint16_t)((((uint32_t)s_est_near_mm * 31u) + est_mm + 16u) / 32u);
            }
            if (est_mm >= (uint16_t)(s_est_far_mm - 20u))
            {
                s_est_far_mm = (uint16_t)((((uint32_t)s_est_far_mm * 31u) + est_mm + 16u) / 32u);
            }
        }

        if (s_est_near_mm < TOF_EST_NEAR_MIN_MM)
        {
            s_est_near_mm = TOF_EST_NEAR_MIN_MM;
        }
        if (s_est_near_mm > TOF_EST_NEAR_MAX_MM)
        {
            s_est_near_mm = TOF_EST_NEAR_MAX_MM;
        }
        if (s_est_far_mm < TOF_EST_FAR_MIN_MM)
        {
            s_est_far_mm = TOF_EST_FAR_MIN_MM;
        }
        if (s_est_far_mm > TOF_EST_FAR_MAX_MM)
        {
            s_est_far_mm = TOF_EST_FAR_MAX_MM;
        }
        if (s_est_far_mm < (uint16_t)(s_est_near_mm + TOF_EST_MIN_GAP_MM))
        {
            s_est_far_mm = (uint16_t)(s_est_near_mm + TOF_EST_MIN_GAP_MM);
            if (s_est_far_mm > TOF_EST_FAR_MAX_MM)
            {
                s_est_far_mm = TOF_EST_FAR_MAX_MM;
                if (s_est_far_mm > TOF_EST_MIN_GAP_MM)
                {
                    s_est_near_mm = (uint16_t)(s_est_far_mm - TOF_EST_MIN_GAP_MM);
                }
            }
        }
        const uint32_t target_fullness_q10 =
            tof_tp_fullness_q10_from_mm_q8_bounds(s_est_mm_q8, s_est_near_mm, s_est_far_mm);
        const uint32_t fullness_delta = tof_abs_diff_u32(s_est_fullness_q10, target_fullness_q10);
        uint32_t den = 8u;
        if ((conf_q10 >= 700u) && (fullness_delta >= 96u))
        {
            den = 2u;
        }
        else if (conf_q10 >= 450u)
        {
            den = 4u;
        }
        s_est_fullness_q10 =
            (uint16_t)(((uint32_t)s_est_fullness_q10 * (den - 1u) + target_fullness_q10 + (den / 2u)) / den);
    }
    else
    {
        s_est_valid_count = 0u;
        s_est_spread_mm = 0u;
        s_est_conf_q10 = (uint16_t)(((uint32_t)s_est_conf_q10 * 15u) / 16u);
    }
#else
    (void)mm;
    (void)live_data;
#endif
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

    PRINTF("AI_CSV,t=%u,ai=%u,live=%u,valid=%u,min=%u,max=%u,avg=%u,act=%u,center=%u,edge=%u,full_q10=%u\r\n",
           (unsigned)tick,
           (unsigned)(s_ai_runtime_on ? 1u : 0u),
           1u,
           (unsigned)valid,
           (unsigned)min_mm,
           (unsigned)max_mm,
           (unsigned)avg_mm,
           (unsigned)actual_mm,
           (unsigned)center_avg,
           (unsigned)edge_avg,
           (unsigned)fullness_q10);

#if TOF_AI_DATA_LOG_FULL_FRAME
    PRINTF("AI_F64,t=%u", (unsigned)tick);
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

static uint32_t TOF_UNUSED tof_tp_fullness_q10_from_mm_q8(uint32_t mm_q8)
{
    return tof_tp_fullness_q10_from_mm_q8_bounds(mm_q8, TOF_TP_MM_FULL_NEAR, TOF_TP_MM_EMPTY_FAR);
}

static uint16_t tof_tp_bg_color(uint32_t t, bool live_data)
{
    (void)t;
    (void)live_data;
    uint32_t r = 8u;
    uint32_t g = 28u;
    uint32_t b = 64u;
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

static void tof_tp_bar_rect(int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1)
{
    int32_t bx0 = TOF_TP_X0 + TOF_TP_BAR_MARGIN_X;
    int32_t bx1 = TOF_TP_X1 - TOF_TP_BAR_MARGIN_X;
    int32_t by1 = TOF_TP_Y1 - 8;
    int32_t by0 = by1 - TOF_TP_BAR_H;
    bx0 = tof_clamp_i32(bx0, TOF_TP_X0, TOF_TP_X1);
    bx1 = tof_clamp_i32(bx1, TOF_TP_X0, TOF_TP_X1);
    by0 = tof_clamp_i32(by0, TOF_TP_Y0, TOF_TP_Y1);
    by1 = tof_clamp_i32(by1, TOF_TP_Y0, TOF_TP_Y1);
    if (x0 != NULL)
    {
        *x0 = bx0;
    }
    if (y0 != NULL)
    {
        *y0 = by0;
    }
    if (x1 != NULL)
    {
        *x1 = bx1;
    }
    if (y1 != NULL)
    {
        *y1 = by1;
    }
}

static void tof_tp_status_rect(int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1)
{
    int32_t bar_x0 = 0;
    int32_t bar_y0 = 0;
    int32_t bar_x1 = 0;
    int32_t bar_y1 = 0;
    tof_tp_bar_rect(&bar_x0, &bar_y0, &bar_x1, &bar_y1);

    int32_t sx0 = bar_x0;
    int32_t sx1 = bar_x1;
    int32_t sy1 = bar_y0 - TOF_TP_STATUS_GAP_Y;
    int32_t sy0 = sy1 - TOF_TP_STATUS_H + 1;

    sx0 = tof_clamp_i32(sx0, TOF_TP_X0, TOF_TP_X1);
    sx1 = tof_clamp_i32(sx1, TOF_TP_X0, TOF_TP_X1);
    sy0 = tof_clamp_i32(sy0, TOF_TP_Y0, TOF_TP_Y1);
    sy1 = tof_clamp_i32(sy1, TOF_TP_Y0, TOF_TP_Y1);
    if (sy1 <= sy0)
    {
        sy0 = TOF_TP_Y0 + 2;
        sy1 = sy0 + TOF_TP_STATUS_H - 1;
        if (sy1 > TOF_TP_Y1)
        {
            sy1 = TOF_TP_Y1;
            sy0 = tof_clamp_i32(sy1 - TOF_TP_STATUS_H + 1, TOF_TP_Y0, TOF_TP_Y1);
        }
    }

    if (x0 != NULL)
    {
        *x0 = sx0;
    }
    if (y0 != NULL)
    {
        *y0 = sy0;
    }
    if (x1 != NULL)
    {
        *x1 = sx1;
    }
    if (y1 != NULL)
    {
        *y1 = sy1;
    }
}

static uint32_t tof_roll_level_ref_fullness_q10(tof_roll_alert_level_t level)
{
    switch (level)
    {
        case kTofRollAlertFull:
            return 1024u;
        case kTofRollAlertMedium:
            return 512u;
        case kTofRollAlertLow:
            return 256u;
        case kTofRollAlertEmpty:
        default:
            return 0u;
    }
}

static void tof_draw_roll_status_banner(tof_roll_alert_level_t level, bool live_data)
{
    if (!s_dbg_force_redraw &&
        s_roll_status_prev_valid &&
        s_roll_status_prev_level == level &&
        s_roll_status_prev_live == live_data)
    {
        return;
    }

    int32_t x0 = 0;
    int32_t y0 = 0;
    int32_t x1 = 0;
    int32_t y1 = 0;
    tof_tp_status_rect(&x0, &y0, &x1, &y1);
    if (x1 < x0 || y1 < y0)
    {
        return;
    }

    const uint16_t frame = pack_rgb565(40u, 46u, 52u);
    const uint16_t tray = pack_rgb565(10u, 12u, 14u);
    display_hal_fill_rect(x0, y0, x1, y1, tray);
    display_hal_fill_rect(x0, y0, x1, y0, frame);
    display_hal_fill_rect(x0, y1, x1, y1, frame);
    display_hal_fill_rect(x0, y0, x0, y1, frame);
    display_hal_fill_rect(x1, y0, x1, y1, frame);

    const int32_t ix0 = x0 + 2;
    const int32_t ix1 = x1 - 2;
    const int32_t iy0 = y0 + 2;
    const int32_t iy1 = y1 - 2;
    if (ix1 >= ix0 && iy1 >= iy0)
    {
        const uint16_t fill = tof_tp_bar_color(tof_roll_level_ref_fullness_q10(level), live_data);
        display_hal_fill_rect(ix0, iy0, ix1, iy1, fill);
    }

    const char *msg = NULL;
    uint16_t fg = pack_rgb565(246u, 238u, 226u);
    tof_roll_status_style(level, &msg, NULL, NULL, &fg);
    const size_t n = strlen(msg);
    const int32_t text_w = (n > 0u) ? ((int32_t)(n * TOF_DBG_CHAR_ADV) - 2) : 0;
    const int32_t text_h = TOF_DBG_CHAR_H * TOF_DBG_SCALE;
    int32_t tx = x0 + ((x1 - x0 + 1 - text_w) / 2);
    int32_t ty = y0 + ((y1 - y0 + 1 - text_h) / 2);
    if (tx < (x0 + 2))
    {
        tx = x0 + 2;
    }
    if (ty < (y0 + 1))
    {
        ty = y0 + 1;
    }
    for (size_t i = 0u; i < n; i++)
    {
        tof_tiny_draw_char_clipped(tx, ty, msg[i], fg, x0, y0, x1, y1);
        tx += TOF_DBG_CHAR_ADV;
    }

    s_roll_status_prev_valid = true;
    s_roll_status_prev_level = level;
    s_roll_status_prev_live = live_data;
}

static void tof_draw_fullness_bar(uint32_t fullness_q10, uint32_t mm_q8, bool live_data)
{
    int32_t bar_x0 = 0;
    int32_t bar_y0 = 0;
    int32_t bar_x1 = 0;
    int32_t bar_y1 = 0;
    tof_tp_bar_rect(&bar_x0, &bar_y0, &bar_x1, &bar_y1);
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

    const uint8_t segments_on = tof_roll_segments_from_fullness_q10(fullness_q10);
    const int32_t seg_count = (int32_t)TOF_ROLL_SEGMENT_COUNT;
    const int32_t seg_gap = 2;
    const int32_t total_gap = (seg_count - 1) * seg_gap;
    int32_t seg_area = iw - total_gap;
    if (seg_area < seg_count)
    {
        seg_area = iw;
    }
    int32_t seg_w = seg_area / seg_count;
    if (seg_w < 1)
    {
        seg_w = 1;
    }
    int32_t seg_extra = seg_area - (seg_w * seg_count);
    if (seg_extra < 0)
    {
        seg_extra = 0;
    }

    int32_t sx = ix0;
    for (int32_t i = 0; i < seg_count; i++)
    {
        int32_t cur_w = seg_w;
        if (seg_extra > 0)
        {
            cur_w++;
            seg_extra--;
        }
        int32_t ex = sx + cur_w - 1;
        if (ex > ix1)
        {
            ex = ix1;
        }

        const bool on = ((uint8_t)i < segments_on);
        const uint16_t seg_color = on ? tof_tp_bar_color(fullness_q10, live_data) : pack_rgb565(42u, 32u, 28u);
        if (ex >= sx)
        {
            display_hal_fill_rect(sx, iy0, ex, iy1, seg_color);
        }
        sx = ex + 1 + seg_gap;
        if (sx > ix1)
        {
            break;
        }
    }
    (void)mm_q8;
}

static void tof_update_spool_model(const uint16_t mm[64], bool live_data, uint32_t tick, bool draw_enable)
{
    const bool force_now = s_tp_force_redraw && draw_enable;
    if (!force_now && (uint32_t)(tick - s_tp_last_tick) < TOF_TP_UPDATE_TICKS)
    {
        return;
    }

    /* Rewritten TP detection pipeline:
     * 1) use one AI-independent input frame for state decisions;
     * 2) detect sparse-full and hard-empty explicitly;
     * 3) smooth only after state decision to keep UI reactive.
     */
    const uint16_t *calc_mm = mm;
    uint32_t valid_count = 0u;
    uint16_t avg_mm_raw = 0u;
    tof_calc_frame_stats(calc_mm, &valid_count, NULL, NULL, &avg_mm_raw);
    const uint16_t closest_mm_raw = tof_calc_closest_valid_mm(calc_mm);
    const uint16_t curve_mm_raw = tof_calc_roll_curve_distance_mm(calc_mm, NULL);

    const uint16_t closest_mm = tof_apply_tp_mm_calibration(closest_mm_raw);
    const uint16_t curve_mm = tof_apply_tp_mm_calibration(curve_mm_raw);
    const uint16_t avg_mm = tof_apply_tp_mm_calibration(avg_mm_raw);
    s_tp_live_closest_mm = closest_mm;

    if (s_ai_runtime_on)
    {
        /* Keep estimator hot only when AI mode is enabled so AI ON can improve stability. */
        tof_estimator_update(calc_mm, live_data);
    }

    const bool no_surface_signal = (closest_mm == 0u) && (curve_mm == 0u) && (avg_mm == 0u);
    const bool sparse_or_missing = (valid_count <= TOF_ROLL_EMPTY_SPARSE_VALID_MAX);
    const bool hard_empty_candidate =
        no_surface_signal ||
        (sparse_or_missing && (avg_mm >= TOF_ROLL_EMPTY_SPARSE_AVG_MIN)) ||
        ((closest_mm > TOF_ROLL_EMPTY_TRIGGER_MM) &&
         ((curve_mm == 0u) || (curve_mm > TOF_ROLL_EMPTY_TRIGGER_MM)));

    const bool full_capture_candidate =
        (closest_mm > 0u) && (closest_mm <= TOF_ROLL_FULL_CAPTURE_MM);
    const bool full_sparse_candidate =
        (closest_mm > 0u) &&
        (closest_mm <= TOF_ROLL_LOW_TRIGGER_MM) &&
        (valid_count <= TOF_ROLL_FULL_SPARSE_VALID_MAX) &&
        (avg_mm > 0u) &&
        (avg_mm <= TOF_ROLL_FULL_SPARSE_AVG_MAX);

    uint16_t actual_mm = 0u;
    if (full_sparse_candidate)
    {
        actual_mm = TOF_TP_MM_FULL_NEAR;
    }
    else if (hard_empty_candidate)
    {
        actual_mm = TOF_ROLL_EMPTY_FORCE_MM;
    }
    else if ((curve_mm > 0u) && (avg_mm > 0u))
    {
        const uint32_t blended = ((uint32_t)curve_mm * 3u) + ((uint32_t)avg_mm * 2u);
        actual_mm = (uint16_t)((blended + 2u) / 5u);
    }
    else if (curve_mm > 0u)
    {
        actual_mm = curve_mm;
    }
    else if (avg_mm > 0u)
    {
        actual_mm = avg_mm;
    }
    else
    {
        actual_mm = closest_mm;
    }

    if (actual_mm == 0u)
    {
        actual_mm = TOF_ROLL_EMPTY_FORCE_MM;
    }
    if (s_alert_popup_hold_empty &&
        full_capture_candidate &&
        (closest_mm <= TOF_ROLL_FULL_CAPTURE_MM))
    {
        actual_mm = TOF_TP_MM_FULL_NEAR;
    }

    if (s_ai_runtime_on &&
        live_data &&
        !hard_empty_candidate &&
        !full_sparse_candidate &&
        (s_est_mm_q8 > 0u) &&
        (s_est_conf_q10 >= TOF_AI_FUSE_CONF_MIN_Q10))
    {
        uint32_t w_mm = ((uint32_t)(s_est_conf_q10 - TOF_AI_FUSE_CONF_MIN_Q10) *
                         TOF_AI_FUSE_MM_WEIGHT_MAX_Q10) /
                        (1024u - TOF_AI_FUSE_CONF_MIN_Q10);
        if (w_mm > TOF_AI_FUSE_MM_WEIGHT_MAX_Q10)
        {
            w_mm = TOF_AI_FUSE_MM_WEIGHT_MAX_Q10;
        }

        const uint16_t est_mm = tof_apply_tp_mm_calibration((uint16_t)(s_est_mm_q8 >> 8));
        if (est_mm > 0u)
        {
            const uint32_t fused_mm =
                (((uint32_t)actual_mm * (1024u - w_mm)) + ((uint32_t)est_mm * w_mm) + 512u) / 1024u;
            actual_mm = (uint16_t)fused_mm;
        }
    }
    s_tp_live_actual_mm = actual_mm;

    bool snap_extreme = false;
    const uint32_t raw_mm_q8 = (actual_mm > 0u) ? ((uint32_t)actual_mm << 8) : 0u;
    if (raw_mm_q8 > 0u)
    {
        uint16_t snap_mm = actual_mm;
        snap_extreme = full_sparse_candidate ||
                       hard_empty_candidate ||
                       (snap_mm <= TOF_ROLL_FULL_CAPTURE_MM) ||
                       (snap_mm > TOF_ROLL_EMPTY_TRIGGER_MM);
        if (s_tp_mm_q8 == 0u || snap_extreme)
        {
            s_tp_mm_q8 = ((uint32_t)snap_mm << 8);
        }
        else
        {
            uint32_t delta_q8 = (s_tp_mm_q8 > raw_mm_q8) ? (s_tp_mm_q8 - raw_mm_q8) : (raw_mm_q8 - s_tp_mm_q8);
            if (delta_q8 >= (6u << 8))
            {
                s_tp_mm_q8 = (s_tp_mm_q8 + raw_mm_q8 + 1u) / 2u;
            }
            else
            {
                s_tp_mm_q8 = ((s_tp_mm_q8 * 3u) + raw_mm_q8 + 2u) / 4u;
            }
        }
    }
    s_tp_live_actual_mm = actual_mm;

    uint32_t model_mm_q8 = s_tp_mm_q8;

#if TOF_EST_ENABLE
    (void)live_data;
#endif
    uint32_t fullness_q10 = tof_tp_fullness_q10_from_mm_q8_bounds(model_mm_q8,
                                                                   TOF_TP_MM_FULL_NEAR,
                                                                   TOF_TP_BAR_MM_EMPTY);
    if (full_sparse_candidate)
    {
        fullness_q10 = 1024u;
    }
    else if (hard_empty_candidate)
    {
        fullness_q10 = 0u;
    }
    if (fullness_q10 > 1024u)
    {
        fullness_q10 = 1024u;
    }
    if (s_ai_runtime_on &&
        live_data &&
        !hard_empty_candidate &&
        !full_sparse_candidate &&
        (s_est_conf_q10 >= TOF_AI_FUSE_CONF_MIN_Q10))
    {
        uint32_t w_full = ((uint32_t)(s_est_conf_q10 - TOF_AI_FUSE_CONF_MIN_Q10) *
                           TOF_AI_FUSE_FULLNESS_WEIGHT_MAX_Q10) /
                          (1024u - TOF_AI_FUSE_CONF_MIN_Q10);
        if (w_full > TOF_AI_FUSE_FULLNESS_WEIGHT_MAX_Q10)
        {
            w_full = TOF_AI_FUSE_FULLNESS_WEIGHT_MAX_Q10;
        }
        const uint32_t est_full = (s_est_fullness_q10 > 1024u) ? 1024u : s_est_fullness_q10;
        fullness_q10 =
            ((fullness_q10 * (1024u - w_full)) + (est_full * w_full) + 512u) / 1024u;
    }

    uint32_t bar_fullness_q10 = fullness_q10;
    if (bar_fullness_q10 > 1024u)
    {
        bar_fullness_q10 = 1024u;
    }
    const uint8_t bar_segments = tof_roll_segments_from_fullness_q10(bar_fullness_q10);
    uint16_t bar_fullness_draw_q10 = (uint16_t)((uint32_t)bar_segments * 128u);
    if (bar_fullness_draw_q10 > 1024u)
    {
        bar_fullness_draw_q10 = 1024u;
    }
    s_roll_fullness_q10 = (uint16_t)fullness_q10;
    s_roll_model_mm = (model_mm_q8 > 0u) ? (uint16_t)((model_mm_q8 + 128u) >> 8) : 0u;
    if (s_roll_model_mm > TOF_TP_MM_CLIP_MAX)
    {
        s_roll_model_mm = TOF_TP_MM_CLIP_MAX;
    }

    const int32_t area_w = (TOF_TP_X1 - TOF_TP_X0) + 1;
    int32_t status_y0 = 0;
    tof_tp_status_rect(NULL, &status_y0, NULL, NULL);
    const int32_t roll_top = TOF_TP_Y0 + 6;
    int32_t roll_bottom = status_y0 - 6;
    if (roll_bottom < (roll_top + 24))
    {
        roll_bottom = roll_top + 24;
    }
    const int32_t roll_h = (roll_bottom - roll_top) + 1;
    const int32_t outer_ry_min = 34;
    int32_t outer_ry_max = (roll_h / 2) - 2;
    outer_ry_max = tof_clamp_i32(outer_ry_max, outer_ry_min, 92);
    if (outer_ry_max < outer_ry_min)
    {
        outer_ry_max = outer_ry_min;
    }

    const uint8_t roll_segments = bar_segments;
    const int32_t depth = tof_clamp_i32(14 + (outer_ry_max / 3), 14, 28);
    int32_t outer_rx_fit = ((area_w - depth) / 2) - 3;
    outer_rx_fit = tof_clamp_i32(outer_rx_fit, 32, 120);
    const int32_t core_ry_fixed = tof_clamp_i32((outer_ry_max * 56) / 100, 18, outer_ry_max - 6);
    const int32_t core_rx_fixed = tof_clamp_i32((outer_rx_fit * 56) / 100, 18, outer_rx_fit - 6);
    const int32_t white_max_ry = tof_clamp_i32(outer_ry_max - core_ry_fixed, 0, outer_ry_max);
    const int32_t white_max_rx = tof_clamp_i32(outer_rx_fit - core_rx_fixed, 0, outer_rx_fit);
    const int32_t white_add_ry =
        (int32_t)(((uint32_t)roll_segments * (uint32_t)white_max_ry + 4u) / TOF_ROLL_SEGMENT_COUNT);
    const int32_t white_add_rx =
        (int32_t)(((uint32_t)roll_segments * (uint32_t)white_max_rx + 4u) / TOF_ROLL_SEGMENT_COUNT);
    const int32_t outer_ry = core_ry_fixed + white_add_ry;
    const int32_t outer_rx = core_rx_fixed + white_add_rx;

    const bool render_live = live_data || (model_mm_q8 > 0u);
    tof_ai_log_frame(mm, render_live, tick, fullness_q10);
    const bool roll_geom_changed = ((uint16_t)outer_ry != s_tp_last_outer_ry) ||
                                   ((uint16_t)outer_rx != s_tp_last_outer_rx);
    const uint32_t roll_delta_q8 = tof_abs_diff_u32(model_mm_q8, s_tp_last_roll_mm_q8);
    const bool roll_changed = s_tp_force_redraw ||
                              (render_live != s_tp_last_live) ||
                              (roll_geom_changed &&
                               (roll_delta_q8 >= ((uint32_t)TOF_TP_ROLL_REDRAW_MM_DELTA << 8)));
    const bool bar_changed = s_tp_force_redraw ||
                             (render_live != s_tp_last_live) ||
                             (bar_fullness_draw_q10 != s_tp_last_fullness_q10);

    if (!draw_enable)
    {
        s_tp_last_tick = tick;
        /* Redraw immediately once the popup releases, but keep model cadence stable meanwhile. */
        s_tp_force_redraw = true;
        return;
    }

    if (!roll_changed && !bar_changed)
    {
        s_tp_last_tick = tick;
        return;
    }

    if (roll_changed)
    {
        const bool has_paper = (roll_segments > 0u);
        const int32_t core_ry = core_ry_fixed;
        const int32_t core_rx = core_rx_fixed;
        const int32_t hole_ry = tof_clamp_i32((core_ry * 60) / 100, 7, core_ry - 3);
        const int32_t hole_rx = tof_clamp_i32((core_rx * 60) / 100, 7, core_rx - 3);
        const int32_t center_x = TOF_TP_X0 + (area_w / 2);
        int32_t cy = roll_top + (roll_h / 2) - 6;
        cy = tof_clamp_i32(cy, roll_top + outer_ry + 2, roll_bottom - outer_ry - 2);
        const int32_t back_cx = center_x - (depth / 2);
        const int32_t front_cx = center_x + (depth / 2);

        int32_t roll_x0 = back_cx - outer_rx - 3;
        int32_t roll_x1 = front_cx + outer_rx + 3;
        int32_t roll_y0 = cy - outer_ry - 3;
        int32_t roll_y1 = cy + outer_ry + 8;

        if (s_tp_prev_rect_valid)
        {
            roll_x0 = (roll_x0 < s_tp_prev_x0) ? roll_x0 : s_tp_prev_x0;
            roll_y0 = (roll_y0 < s_tp_prev_y0) ? roll_y0 : s_tp_prev_y0;
            roll_x1 = (roll_x1 > s_tp_prev_x1) ? roll_x1 : s_tp_prev_x1;
            roll_y1 = (roll_y1 > s_tp_prev_y1) ? roll_y1 : s_tp_prev_y1;
        }
        tof_tp_fill_bg_rect(roll_x0, roll_y0, roll_x1, roll_y1, render_live);

        const int32_t shadow_y = tof_clamp_i32(cy + outer_ry + 2, TOF_TP_Y0, roll_bottom + 2);
        tof_draw_filled_ellipse(center_x + (depth / 8),
                                shadow_y,
                                outer_rx + (depth / 5),
                                tof_clamp_i32(outer_ry / 16, 2, 6),
                                pack_rgb565(8u, 8u, 10u));
        tof_draw_filled_ellipse(center_x + (depth / 10),
                                shadow_y - 1,
                                outer_rx + (depth / 7),
                                tof_clamp_i32(outer_ry / 20, 2, 4),
                                pack_rgb565(14u, 14u, 18u));

        if (has_paper)
        {
            for (int32_t y = -outer_ry; y <= outer_ry; y += 2)
            {
                const int32_t y_abs = (y < 0) ? -y : y;
                const int32_t py = cy + y;
                int32_t py1 = py + 1;
                if (py1 > TOF_TP_Y1)
                {
                    py1 = TOF_TP_Y1;
                }
                if (py < TOF_TP_Y0 || py > TOF_TP_Y1)
                {
                    continue;
                }

                const int32_t xw = tof_ellipse_half_width(outer_rx, outer_ry, y_abs);
                int32_t x0 = back_cx - xw + 1;
                int32_t x1 = front_cx + xw - 1;
                x0 = tof_clamp_i32(x0, TOF_TP_X0, TOF_TP_X1);
                x1 = tof_clamp_i32(x1, TOF_TP_X0, TOF_TP_X1);
                if (x1 < x0)
                {
                    continue;
                }

                const int32_t tone = 168 + (((outer_ry - y_abs) * 44) / tof_clamp_i32(outer_ry, 1, 1024));
                display_hal_fill_rect(x0, py, x1, py1, tof_tp_paper_color(tone, render_live));
            }
        }

        const uint16_t back_base = has_paper ? tof_tp_paper_color(110, render_live) : tof_tp_core_color(154, render_live);
        tof_draw_filled_ellipse(back_cx, cy, outer_rx, outer_ry, back_base);
        tof_draw_ellipse_ring(back_cx,
                              cy,
                              outer_rx,
                              outer_ry,
                              2,
                              has_paper ? tof_tp_paper_color(84, render_live) : tof_tp_core_color(118, render_live),
                              back_base);
        tof_draw_filled_ellipse(back_cx, cy, hole_rx, hole_ry, pack_rgb565(18u, 16u, 14u));

        const uint16_t front_base = has_paper ? tof_tp_paper_color(232, render_live) : tof_tp_core_color(182, render_live);
        tof_draw_filled_ellipse(front_cx, cy, outer_rx, outer_ry, front_base);
        tof_draw_ellipse_ring(front_cx,
                              cy,
                              outer_rx,
                              outer_ry,
                              2,
                              has_paper ? tof_tp_paper_color(168, render_live) : tof_tp_core_color(118, render_live),
                              front_base);

        for (int32_t i = 0; has_paper && i < 4; i++)
        {
            const int32_t ry_layer = outer_ry - 3 - (i * 5);
            if (ry_layer <= (core_ry + 2))
            {
                break;
            }
            const int32_t rx_layer = (outer_rx * ry_layer) / outer_ry;
            const int32_t tone = 228 - (i * 16);
            tof_draw_ellipse_ring(front_cx,
                                  cy,
                                  rx_layer,
                                  ry_layer,
                                  1,
                                  tof_tp_paper_color(tone, render_live),
                                  front_base);
        }

        const uint16_t core_base = tof_tp_core_color(182, render_live);
        tof_draw_filled_ellipse(front_cx, cy, core_rx, core_ry, core_base);
        tof_draw_ellipse_ring(front_cx, cy, core_rx, core_ry, 2, tof_tp_core_color(118, render_live), core_base);
        tof_draw_filled_ellipse(front_cx, cy, hole_rx, hole_ry, pack_rgb565(24u, 22u, 20u));
        tof_draw_filled_ellipse(front_cx + 1,
                                cy - 1,
                                tof_clamp_i32((hole_rx * 68) / 100, 4, hole_rx),
                                tof_clamp_i32((hole_ry * 68) / 100, 4, hole_ry),
                                pack_rgb565(11u, 11u, 13u));

        s_tp_prev_x0 = (int16_t)tof_clamp_i32(back_cx - outer_rx - 3, TOF_TP_X0, TOF_TP_X1);
        s_tp_prev_y0 = (int16_t)tof_clamp_i32(cy - outer_ry - 3, TOF_TP_Y0, TOF_TP_Y1);
        s_tp_prev_x1 = (int16_t)tof_clamp_i32(front_cx + outer_rx + 3, TOF_TP_X0, TOF_TP_X1);
        s_tp_prev_y1 = (int16_t)tof_clamp_i32(cy + outer_ry + 8, TOF_TP_Y0, TOF_TP_Y1);
        s_tp_prev_rect_valid = true;
        s_tp_last_roll_mm_q8 = model_mm_q8;
    }

    if (bar_changed)
    {
        tof_draw_fullness_bar(bar_fullness_draw_q10, model_mm_q8, render_live);
    }
    tof_draw_brand_mark();

    s_tp_last_tick = tick;
    s_tp_last_outer_ry = (uint16_t)outer_ry;
    s_tp_last_outer_rx = (uint16_t)outer_rx;
    s_tp_last_fullness_q10 = bar_fullness_draw_q10;
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

    if (s_dbg_force_redraw)
    {
        display_hal_fill_rect(s_dbg_x0, s_dbg_y0, s_dbg_x1, s_dbg_y1, s_ui_dbg_bg);
        display_hal_fill_rect(s_dbg_x0, s_dbg_y0, s_dbg_x1, s_dbg_y0, pack_rgb565(32u, 38u, 44u));
        memset(s_dbg_prev, 0, sizeof(s_dbg_prev));
        s_ai_pill_prev_valid = false;
        s_alert_pill_prev_valid = false;
    }

    uint32_t valid = 0u;
    uint16_t min_mm = 0u;
    uint16_t max_mm = 0u;
    uint16_t avg_mm = 0u;
    uint16_t actual_mm = 0u;
    uint16_t est_mm = (uint16_t)(s_tp_mm_q8 >> 8);
    uint16_t conf_q10 = s_est_conf_q10;
    uint32_t full_q10 = s_roll_fullness_q10;
    uint16_t fullness_pct = (uint16_t)((full_q10 * 100u + 512u) / 1024u);
    uint16_t conf_pct = (uint16_t)((((uint32_t)conf_q10 * 100u) + 512u) / 1024u);
    const uint16_t *calc_mm = tof_calc_metric_frame(mm, live_data);
    tof_calc_frame_stats(calc_mm, &valid, &min_mm, &max_mm, &avg_mm);
    uint16_t curve_mm = 0u;
    if (s_ai_runtime_on)
    {
        curve_mm = tof_calc_roll_curve_distance_mm(calc_mm, NULL);
    }
    uint16_t closest_mm = tof_calc_closest_valid_mm(calc_mm);
    uint16_t actual_candidates[3];
    uint32_t actual_count = 0u;
    if (avg_mm > 0u)
    {
        actual_candidates[actual_count++] = avg_mm;
    }
    if (curve_mm > 0u)
    {
        actual_candidates[actual_count++] = curve_mm;
    }
    if (closest_mm > 0u)
    {
        actual_candidates[actual_count++] = closest_mm;
    }
    actual_mm = (actual_count > 0u) ? tof_ai_grid_median_u16(actual_candidates, actual_count) : 0u;
    actual_mm = tof_apply_tp_mm_calibration(actual_mm);

    if (!s_ai_runtime_on)
    {
        conf_q10 = 0u;
        conf_pct = 0u;
    }

    char line[TOF_DBG_COLS + 1u];
    snprintf(line, sizeof(line), "LIVE:%u GL:%u GC:%u",
             live_data ? 1u : 0u,
             got_live ? 1u : 0u,
             got_complete ? 1u : 0u);
    tof_dbg_draw_line(0u, line, s_ui_dbg_fg);

    snprintf(line, sizeof(line), "TP:%u-%u",
             (unsigned)TOF_TP_MM_FULL_NEAR,
             (unsigned)TOF_TP_BAR_MM_EMPTY);
    tof_dbg_draw_line(1u, line, s_ui_dbg_fg);

    snprintf(line, sizeof(line), "AVG:%u EST:%u",
             (unsigned)avg_mm,
             (unsigned)est_mm);
    tof_dbg_draw_line(2u, line, s_ui_dbg_fg);

    snprintf(line, sizeof(line), "CF:%u FQ:%u",
             (unsigned)conf_q10,
             (unsigned)fullness_pct);
    tof_dbg_draw_line(3u, line, s_ui_dbg_dim);

    snprintf(line, sizeof(line), "CONF:%u",
             (unsigned)conf_pct);
    tof_dbg_draw_line(5u, line, s_ui_dbg_dim);

    snprintf(line, sizeof(line), "AI:%u A:%u",
             (unsigned)(s_ai_runtime_on ? 1u : 0u),
             (unsigned)actual_mm);
    tof_dbg_draw_line(4u, line, s_ui_dbg_dim);

    (void)stale_frames;
    (void)zero_live_frames;
    (void)valid;
    (void)min_mm;
    (void)max_mm;
    tof_draw_ai_pill(s_ai_runtime_on);

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
    s_ai_pill_x0 = (int16_t)(s_dbg_x0 + TOF_AI_PILL_MARGIN_X);
    s_ai_pill_x1 = (int16_t)(s_dbg_x1 - TOF_AI_PILL_MARGIN_X);
    s_ai_pill_y1 = (int16_t)(s_dbg_y1 - TOF_AI_PILL_MARGIN_BOTTOM);
    s_ai_pill_y0 = (int16_t)(s_ai_pill_y1 - TOF_AI_PILL_H + 1);
    if (s_ai_pill_y0 < (s_dbg_y0 + 1))
    {
        s_ai_pill_y0 = (int16_t)(s_dbg_y0 + 1);
    }
    s_alert_pill_x0 = s_ai_pill_x0;
    s_alert_pill_x1 = s_ai_pill_x1;
    s_alert_pill_y1 = (int16_t)(s_ai_pill_y0 - TOF_ALERT_PILL_GAP_Y);
    s_alert_pill_y0 = (int16_t)(s_alert_pill_y1 - TOF_ALERT_PILL_H + 1);
    if (s_alert_pill_y0 < (s_dbg_y0 + 1))
    {
        s_alert_pill_y0 = (int16_t)(s_dbg_y0 + 1);
        s_alert_pill_y1 = (int16_t)(s_alert_pill_y0 + TOF_ALERT_PILL_H - 1);
    }

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

static void tof_draw_curve_pick_overlay(bool show_overlay)
{
    for (uint32_t y = 0u; y < TOF_GRID_H; y++)
    {
        const int32_t prev_idx = s_curve_prev_pick_idx[y];
        const int32_t next_idx = (show_overlay) ? s_curve_pick_idx[y] : -1;
        if (prev_idx == next_idx)
        {
            continue;
        }

        if (prev_idx >= 0 && prev_idx < 64)
        {
            const uint16_t restore = (prev_idx == s_hot_idx) ? s_ui_hot : s_ui_border;
            tof_draw_outline(&s_cells[prev_idx], restore);
        }

        if (next_idx >= 0 && next_idx < 64)
        {
            tof_draw_outline(&s_cells[next_idx], s_ui_pick);
        }

        s_curve_prev_pick_idx[y] = (int16_t)next_idx;
    }
}

static void tof_ui_init(void)
{
    s_ui_bg = pack_rgb565(3u, 4u, 6u);
    s_ui_border = pack_rgb565(16u, 18u, 22u);
    s_ui_hot = pack_rgb565(255u, 255u, 255u);
    s_ui_pick = pack_rgb565(56u, 242u, 255u);
    s_ui_invalid = pack_rgb565(14u, 14u, 18u);
    s_ui_below_range = pack_rgb565(0u, 220u, 0u);
    s_ui_above_range = pack_rgb565(56u, 8u, 8u);
    s_ui_dbg_bg = pack_rgb565(6u, 8u, 10u);
    s_ui_dbg_fg = pack_rgb565(210u, 220u, 230u);
    s_ui_dbg_dim = pack_rgb565(120u, 132u, 146u);

    tof_build_layout();
    display_hal_fill(s_ui_bg);
    display_hal_fill_rect(TOF_TP_X0, TOF_TP_Y0, TOF_TP_X1, TOF_TP_Y1, tof_tp_bg_color(255u, true));
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
    for (uint32_t y = 0u; y < TOF_GRID_H; y++)
    {
        s_curve_pick_idx[y] = -1;
        s_curve_prev_pick_idx[y] = -1;
    }
    s_range_near_mm = TOF_LOCKED_NEAR_MM;
    s_range_far_mm = TOF_LOCKED_FAR_MM;
    s_dbg_last_tick = 0u;
    s_dbg_force_redraw = true;
    s_ai_pill_prev_valid = false;
    s_ai_pill_prev_on = false;
    s_alert_pill_prev_valid = false;
    s_alert_pill_prev_on = true;
    s_ai_runtime_on = (TOF_AI_DATA_LOG_ENABLE != 0u);
    s_alert_runtime_on = false;
    s_tp_last_tick = 0u;
    s_tp_last_outer_ry = 0u;
    s_tp_last_outer_rx = 0u;
    s_tp_last_fullness_q10 = 0u;
    s_tp_last_roll_mm_q8 = 0u;
    s_tp_mm_q8 = 0u;
    s_tp_live_actual_mm = 0u;
    s_tp_live_closest_mm = 0u;
    s_tp_last_live = false;
    s_tp_prev_rect_valid = false;
    s_tp_prev_x0 = 0;
    s_tp_prev_y0 = 0;
    s_tp_prev_x1 = 0;
    s_tp_prev_y1 = 0;
    s_ai_log_last_tick = 0u;
    s_est_near_mm = TOF_TP_MM_FULL_NEAR;
    s_est_far_mm = TOF_TP_MM_EMPTY_FAR;
    s_est_mm_q8 = 0u;
    s_est_conf_q10 = 0u;
    s_est_fullness_q10 = 640u;
    s_est_valid_count = 0u;
    s_est_spread_mm = 0u;
    s_roll_fullness_q10 = 640u;
    s_roll_model_mm = 0u;
    s_roll_alert_prev_level = kTofRollAlertFull;
    s_roll_alert_prev_valid = false;
    s_roll_status_prev_level = kTofRollAlertFull;
    s_roll_status_prev_valid = false;
    s_roll_level_stable = kTofRollAlertFull;
    s_roll_level_stable_valid = false;
    s_roll_level_candidate = kTofRollAlertFull;
    s_roll_level_candidate_count = 0u;
    s_roll_status_prev_live = false;
    s_alert_popup_active = false;
    s_alert_popup_prev_drawn = false;
    s_alert_popup_rearm_on_full = true;
    s_alert_popup_hold_empty = false;
    s_alert_low_popup_shown = false;
    s_roll_full_rearm_streak = 0u;
    s_alert_popup_level = kTofRollAlertLow;
    s_alert_popup_prev_level = kTofRollAlertLow;
    s_alert_popup_until_tick = 0u;
    s_alert_popup_last_live = false;
    s_alert_popup_prev_x0 = 0;
    s_alert_popup_prev_y0 = 0;
    s_alert_popup_prev_x1 = 0;
    s_alert_popup_prev_y1 = 0;
    s_touch_was_down = false;
    s_touch_last_poll_tick = 0u;
    s_tp_force_redraw = true;
    tof_ai_grid_reset();
}

static void tof_ai_grid_reset(void)
{
    memset(s_ai_grid_mm, 0, sizeof(s_ai_grid_mm));
    memset(s_ai_grid_hold_age, 0, sizeof(s_ai_grid_hold_age));
    s_ai_grid_noise_mm = 0u;
}

static uint16_t tof_ai_grid_median_u16(uint16_t *values, uint32_t count)
{
    if (count == 0u)
    {
        return 0u;
    }

    for (uint32_t i = 1u; i < count; i++)
    {
        const uint16_t key = values[i];
        int32_t j = (int32_t)i - 1;
        while ((j >= 0) && (values[(uint32_t)j] > key))
        {
            values[(uint32_t)j + 1u] = values[(uint32_t)j];
            j--;
        }
        values[(uint32_t)j + 1u] = key;
    }

    return values[count / 2u];
}

static uint16_t tof_ai_grid_outlier_threshold_mm(uint16_t spread_mm)
{
    uint32_t threshold = TOF_AI_GRID_OUTLIER_MM_MIN + (spread_mm / 12u);
    if (threshold < TOF_AI_GRID_OUTLIER_MM_MIN)
    {
        threshold = TOF_AI_GRID_OUTLIER_MM_MIN;
    }
    if (threshold > TOF_AI_GRID_OUTLIER_MM_MAX)
    {
        threshold = TOF_AI_GRID_OUTLIER_MM_MAX;
    }
    return (uint16_t)threshold;
}

static void tof_ai_denoise_heatmap_frame(const uint16_t in_mm[64], uint16_t out_mm[64], bool live_data)
{
#if TOF_AI_GRID_ENABLE
    if (!live_data)
    {
        for (uint32_t i = 0u; i < 64u; i++)
        {
            const uint16_t raw = in_mm[i];
            uint16_t next = s_ai_grid_mm[i];

            if (tof_mm_valid(raw))
            {
                next = raw;
                s_ai_grid_hold_age[i] = 0u;
            }
            else if (tof_mm_valid(next))
            {
                next = (uint16_t)(((uint32_t)next * 31u) / 32u);
                if (next < 8u)
                {
                    next = 0u;
                }
            }

            s_ai_grid_mm[i] = next;
            out_mm[i] = next;
        }

        s_ai_grid_noise_mm = 0u;
        return;
    }

    uint32_t valid_count = 0u;
    uint16_t min_mm = 0xFFFFu;
    uint16_t max_mm = 0u;
    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t v = in_mm[i];
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

    const uint16_t spread_mm = (valid_count > 0u) ? (uint16_t)(max_mm - min_mm) : 0u;
    const uint16_t outlier_threshold_mm = tof_ai_grid_outlier_threshold_mm(spread_mm);
    const uint16_t conf_q10 = tof_estimator_confidence_q10(valid_count, spread_mm, live_data);

    uint16_t candidate[64];
    uint32_t noise_sum = 0u;
    uint32_t noise_count = 0u;

    for (uint32_t y = 0u; y < TOF_GRID_H; y++)
    {
        for (uint32_t x = 0u; x < TOF_GRID_W; x++)
        {
            const uint32_t idx = (y * TOF_GRID_W) + x;
            const uint16_t raw = in_mm[idx];

            uint16_t neighbors[9];
            uint32_t ncount = 0u;
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

                    const uint16_t nv = in_mm[(uint32_t)ny * TOF_GRID_W + (uint32_t)nx];
                    if (tof_mm_valid(nv))
                    {
                        neighbors[ncount++] = nv;
                    }
                }
            }

            uint16_t pred = 0u;
            bool have_pred = false;
            if (ncount >= TOF_AI_GRID_NEIGHBOR_MIN)
            {
                pred = tof_ai_grid_median_u16(neighbors, ncount);
                have_pred = true;
            }
            else if (tof_mm_valid(s_ai_grid_mm[idx]))
            {
                pred = s_ai_grid_mm[idx];
                have_pred = true;
            }

            if (tof_mm_valid(raw))
            {
                uint16_t fused = raw;
                if (have_pred)
                {
                    const uint16_t delta = tof_abs_diff_u16(raw, pred);
                    noise_sum += delta;
                    noise_count++;
                    if (delta > outlier_threshold_mm)
                    {
                        fused = (uint16_t)((((uint32_t)pred * 3u) + raw + 2u) / 4u);
                    }
                    else if (delta > (outlier_threshold_mm / 2u))
                    {
                        fused = (uint16_t)(((uint32_t)pred + raw + 1u) / 2u);
                    }
                }
                candidate[idx] = fused;
            }
            else if (have_pred)
            {
                candidate[idx] = pred;
            }
            else
            {
                candidate[idx] = 0u;
            }
        }
    }

    uint32_t slow_den = 6u;
    if (conf_q10 >= 768u)
    {
        slow_den = 3u;
    }
    else if (conf_q10 >= 512u)
    {
        slow_den = 4u;
    }
    else if (conf_q10 >= 256u)
    {
        slow_den = 5u;
    }

    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t prev = s_ai_grid_mm[i];
        const uint16_t cur = candidate[i];
        uint16_t next = 0u;

        if (tof_mm_valid(cur))
        {
            if (tof_mm_valid(prev))
            {
                const uint16_t delta = tof_abs_diff_u16(prev, cur);
                const uint32_t den = (delta >= TOF_AI_GRID_FAST_DELTA_MM) ? 2u : slow_den;
                next = (uint16_t)(((uint32_t)prev * (den - 1u) + cur + (den / 2u)) / den);
            }
            else
            {
                next = cur;
            }
            s_ai_grid_hold_age[i] = 0u;
        }
        else if (tof_mm_valid(prev) && (s_ai_grid_hold_age[i] < TOF_AI_GRID_HOLD_FRAMES))
        {
            s_ai_grid_hold_age[i]++;
            next = prev;
        }
        else if (tof_mm_valid(prev))
        {
            next = (uint16_t)(((uint32_t)prev * 31u) / 32u);
            if (next < 8u)
            {
                next = 0u;
            }
        }
        else
        {
            next = 0u;
        }

        s_ai_grid_mm[i] = next;
        out_mm[i] = next;
    }

    s_ai_grid_noise_mm = (noise_count > 0u) ? (uint16_t)(noise_sum / noise_count) : 0u;
#else
    memcpy(out_mm, in_mm, sizeof(uint16_t) * 64u);
    (void)live_data;
    s_ai_grid_noise_mm = 0u;
#endif
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
    if (s_ai_runtime_on)
    {
        uint16_t denoised[64];
        uint16_t repaired[64];
        tof_ai_denoise_heatmap_frame(s_display_mm, denoised, live_data);
        tof_fill_display_holes(denoised, repaired);
        memcpy(s_display_mm, repaired, sizeof(repaired));
    }
    else
    {
        s_ai_grid_noise_mm = 0u;
    }
    tof_repair_corner_blindspots(s_display_mm);
    tof_update_range(s_display_mm, live_data);
    if (s_ai_runtime_on && live_data)
    {
        (void)tof_calc_roll_curve_distance_mm(s_display_mm, s_curve_pick_idx);
    }
    else
    {
        for (uint32_t y = 0u; y < TOF_GRID_H; y++)
        {
            s_curve_pick_idx[y] = -1;
        }
    }

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

    const uint32_t corner_idx = (TOF_GRID_H - 1u) * TOF_GRID_W;
    const tof_cell_rect_t *corner = &s_cells[corner_idx];
    const uint16_t corner_color = tof_color_from_mm(s_display_mm[corner_idx], s_range_near_mm, s_range_far_mm);
    display_hal_fill_rect(corner->ix0, corner->iy1, corner->ix0, corner->iy1, corner_color);

    tof_update_hotspot(s_display_mm, live_data);
    tof_draw_curve_pick_overlay(s_ai_runtime_on && live_data);
}
#endif

#if TOF_DEBUG_RAW_DRAW
static void tof_draw_heatmap_raw(const uint16_t mm[64], bool live_data)
{
    uint16_t draw_mm[64];
    if (s_ai_runtime_on)
    {
        uint16_t repaired[64];
        tof_ai_denoise_heatmap_frame(mm, draw_mm, live_data);
        tof_fill_display_holes(draw_mm, repaired);
        memcpy(draw_mm, repaired, sizeof(repaired));
    }
    else
    {
        tof_fill_display_holes(mm, draw_mm);
        s_ai_grid_noise_mm = 0u;
    }
    tof_repair_corner_blindspots(draw_mm);
    tof_update_range(draw_mm, live_data);
    if (s_ai_runtime_on && live_data)
    {
        (void)tof_calc_roll_curve_distance_mm(draw_mm, s_curve_pick_idx);
    }
    else
    {
        for (uint32_t y = 0u; y < TOF_GRID_H; y++)
        {
            s_curve_pick_idx[y] = -1;
        }
    }

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

    const uint32_t corner_idx = (TOF_GRID_H - 1u) * TOF_GRID_W;
    const tof_cell_rect_t *corner = &s_cells[corner_idx];
    const uint16_t corner_color = tof_color_from_mm(draw_mm[corner_idx], s_range_near_mm, s_range_far_mm);
    display_hal_fill_rect(corner->ix0, corner->iy1, corner->ix0, corner->iy1, corner_color);

    tof_update_hotspot(draw_mm, live_data);
    tof_draw_curve_pick_overlay(s_ai_runtime_on && live_data);
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
    tof_touch_init();
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

    uint16_t boot_roll_mm[64];
    for (uint32_t i = 0u; i < 64u; i++)
    {
        boot_roll_mm[i] = TOF_TP_MM_FULL_NEAR;
    }
    s_tp_force_redraw = true;
    tof_update_spool_model(boot_roll_mm, false, tick, true);
    s_tp_force_redraw = true;

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

        const bool popup_visible = s_alert_runtime_on && s_alert_popup_active;

        if (draw_now && !popup_visible)
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

        tof_touch_poll_ai_toggle(tick);
        const bool model_live = (tof_ok && have_live);
        tof_update_spool_model(frame_mm, model_live, tick, true);
        if (!popup_visible)
        {
            tof_update_debug_panel(frame_mm,
                                   model_live,
                                   got_live,
                                   got_complete,
                                   stale_frames,
                                   zero_live_frames,
                                   tick);
        }
        tof_update_roll_alert_ui(s_roll_fullness_q10, model_live, tick);

        tick++;
        SDK_DelayAtLeastUs(TOF_FRAME_US, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }
}
