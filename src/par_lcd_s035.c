#include "par_lcd_s035.h"

#include <string.h>

#include "board.h"
#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_dbi_flexio_edma.h"
#include "fsl_flexio_mculcd.h"
#include "fsl_gpio.h"
#include "fsl_st7796s.h"
#include "pin_mux.h"

#define TOF_LCD_WIDTH  480u
#define TOF_LCD_HEIGHT 320u

#define TOF_LCD_RST_GPIO GPIO4
#define TOF_LCD_RST_PIN  7u
#define TOF_LCD_CS_GPIO  GPIO0
#define TOF_LCD_CS_PIN   12u
#define TOF_LCD_RS_GPIO  GPIO0
#define TOF_LCD_RS_PIN   7u

#define TOF_FLEXIO             FLEXIO0
#define TOF_FLEXIO_CLOCK_FREQ  CLOCK_GetFlexioClkFreq()
#define TOF_FLEXIO_BAUD_BPS    160000000u

#define TOF_FLEXIO_WR_PIN          1u
#define TOF_FLEXIO_RD_PIN          0u
#define TOF_FLEXIO_DATA_PIN_START  16u
#define TOF_FLEXIO_TX_START        0u
#define TOF_FLEXIO_TX_END          7u
#define TOF_FLEXIO_RX_START        0u
#define TOF_FLEXIO_RX_END          7u
#define TOF_FLEXIO_TIMER           0u

static volatile bool s_mem_write_done = false;
static st7796s_handle_t s_lcd;
static dbi_flexio_edma_xfer_handle_t s_dbi;

static void tof_set_cs(bool set)
{
    GPIO_PinWrite(TOF_LCD_CS_GPIO, TOF_LCD_CS_PIN, set ? 1u : 0u);
}

static void tof_set_rs(bool set)
{
    GPIO_PinWrite(TOF_LCD_RS_GPIO, TOF_LCD_RS_PIN, set ? 1u : 0u);
}

static void tof_dbi_done_cb(status_t status, void *userData)
{
    (void)status;
    (void)userData;
    s_mem_write_done = true;
}

static FLEXIO_MCULCD_Type s_flexio_lcd = {
    .flexioBase = TOF_FLEXIO,
    .busType = kFLEXIO_MCULCD_8080,
    .dataPinStartIndex = TOF_FLEXIO_DATA_PIN_START,
    .ENWRPinIndex = TOF_FLEXIO_WR_PIN,
    .RDPinIndex = TOF_FLEXIO_RD_PIN,
    .txShifterStartIndex = TOF_FLEXIO_TX_START,
    .txShifterEndIndex = TOF_FLEXIO_TX_END,
    .rxShifterStartIndex = TOF_FLEXIO_RX_START,
    .rxShifterEndIndex = TOF_FLEXIO_RX_END,
    .timerIndex = TOF_FLEXIO_TIMER,
    .setCSPin = tof_set_cs,
    .setRSPin = tof_set_rs,
    .setRDWRPin = NULL,
};

static void tof_lcd_wait_write_done(void)
{
    uint32_t spin = 0;
    while (!s_mem_write_done)
    {
        if (++spin > 60000000u)
        {
            s_mem_write_done = true;
            break;
        }
        __NOP();
    }
}

bool par_lcd_s035_init(void)
{
    BOARD_InitLcdPins();

    flexio_mculcd_config_t flexio_cfg;
    FLEXIO_MCULCD_GetDefaultConfig(&flexio_cfg);
    flexio_cfg.baudRate_Bps = TOF_FLEXIO_BAUD_BPS;

    status_t st = FLEXIO_MCULCD_Init(&s_flexio_lcd, &flexio_cfg, TOF_FLEXIO_CLOCK_FREQ);
    if (st != kStatus_Success)
    {
        PRINTF("TOF LCD: FLEXIO_MCULCD_Init failed: %d\r\n", (int)st);
        return false;
    }

    st = DBI_FLEXIO_EDMA_CreateXferHandle(&s_dbi, &s_flexio_lcd, NULL, NULL);
    if (st != kStatus_Success)
    {
        PRINTF("TOF LCD: DBI_FLEXIO_EDMA_CreateXferHandle failed: %d\r\n", (int)st);
        return false;
    }

    GPIO_PinWrite(TOF_LCD_RST_GPIO, TOF_LCD_RST_PIN, 0u);
    SDK_DelayAtLeastUs(2000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    GPIO_PinWrite(TOF_LCD_RST_GPIO, TOF_LCD_RST_PIN, 1u);
    SDK_DelayAtLeastUs(10000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

    const st7796s_config_t cfg = {
        .driverPreset = kST7796S_DriverPresetLCDPARS035,
        .pixelFormat = kST7796S_PixelFormatRGB565,
        .orientationMode = kST7796S_Orientation270,
        .teConfig = kST7796S_TEDisabled,
        .invertDisplay = true,
        .flipDisplay = true,
        .bgrFilter = true,
    };

    st = ST7796S_Init(&s_lcd, &cfg, &g_dbiFlexioEdmaXferOps, &s_dbi);
    if (st != kStatus_Success)
    {
        PRINTF("TOF LCD: ST7796S_Init failed: %d\r\n", (int)st);
        return false;
    }

    ST7796S_SetMemoryDoneCallback(&s_lcd, tof_dbi_done_cb, NULL);
    ST7796S_EnableDisplay(&s_lcd, true);
    return true;
}

void par_lcd_s035_fill(uint16_t rgb565)
{
    static uint16_t line[TOF_LCD_WIDTH];
    for (uint32_t i = 0; i < TOF_LCD_WIDTH; i++)
    {
        line[i] = rgb565;
    }

    for (uint32_t y = 0; y < TOF_LCD_HEIGHT; y++)
    {
        ST7796S_SelectArea(&s_lcd, 0u, (uint16_t)y, TOF_LCD_WIDTH - 1u, (uint16_t)y);
        s_mem_write_done = false;
        ST7796S_WritePixels(&s_lcd, line, TOF_LCD_WIDTH);
        tof_lcd_wait_write_done();
    }
}

void par_lcd_s035_blit_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t *rgb565)
{
    if (!rgb565) return;
    if (x1 < x0 || y1 < y0) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int32_t)TOF_LCD_WIDTH) x1 = (int32_t)TOF_LCD_WIDTH - 1;
    if (y1 >= (int32_t)TOF_LCD_HEIGHT) y1 = (int32_t)TOF_LCD_HEIGHT - 1;

    uint32_t w = (uint32_t)(x1 - x0 + 1);
    uint32_t h = (uint32_t)(y1 - y0 + 1);
    uint32_t n = w * h;

    ST7796S_SelectArea(&s_lcd, (uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1);
    s_mem_write_done = false;
    ST7796S_WritePixels(&s_lcd, rgb565, n);
    tof_lcd_wait_write_done();
}

void par_lcd_s035_fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t rgb565)
{
    if (x1 < x0 || y1 < y0) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int32_t)TOF_LCD_WIDTH) x1 = (int32_t)TOF_LCD_WIDTH - 1;
    if (y1 >= (int32_t)TOF_LCD_HEIGHT) y1 = (int32_t)TOF_LCD_HEIGHT - 1;

    uint32_t w = (uint32_t)(x1 - x0 + 1);
    static uint16_t row[TOF_LCD_WIDTH];
    if (w > TOF_LCD_WIDTH) w = TOF_LCD_WIDTH;

    for (uint32_t i = 0; i < w; i++)
    {
        row[i] = rgb565;
    }

    for (int32_t y = y0; y <= y1; y++)
    {
        ST7796S_SelectArea(&s_lcd, (uint16_t)x0, (uint16_t)y, (uint16_t)x1, (uint16_t)y);
        s_mem_write_done = false;
        ST7796S_WritePixels(&s_lcd, row, w);
        tof_lcd_wait_write_done();
    }
}
