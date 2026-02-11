#include "tmf8828_quick.h"

#include <string.h>

#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_lpi2c.h"

#ifndef TOF_I2C
#define TOF_I2C LPI2C3
#endif

#define TMF8828_REG_APPID        0x00u
#define TMF8828_REG_CMD_STAT     0x08u
#define TMF8828_REG_CONFIG_RES   0x20u
#define TMF8828_REG_PAGED_BASE   0x24u
#define TMF8828_REG_ENABLE       0xE0u
#define TMF8828_REG_INT_STATUS   0xE1u
#define TMF8828_REG_INT_ENAB     0xE2u
#define TMF8828_REG_ID           0xE3u
#define TMF8828_REG_REVID        0xE4u

#define TMF8828_CMD_MEASURE           0x10u
#define TMF8828_CMD_WRITE_CONFIG_PAGE 0x15u
#define TMF8828_CMD_LOAD_COMMON       0x16u
#define TMF8828_CMD_STOP              0xFFu

#define TMF8828_INT_ENABLE_MASK 0x22u

static bool s_i2c_inited = false;
static bool s_sensor_ready = false;
static tmf8828_info_t s_info;

static uint32_t tmf_i2c_get_freq(void)
{
    return CLOCK_GetLPFlexCommClkFreq(3u);
}

static bool tmf_i2c_write(uint8_t reg, const uint8_t *data, uint32_t len)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress = TMF8828_I2C_ADDR;
    xfer.direction = kLPI2C_Write;
    xfer.subaddress = reg;
    xfer.subaddressSize = 1u;
    xfer.data = (uint8_t *)data;
    xfer.dataSize = len;
    xfer.flags = kLPI2C_TransferDefaultFlag;
    return (LPI2C_MasterTransferBlocking(TOF_I2C, &xfer) == kStatus_Success);
}

static bool tmf_i2c_read(uint8_t reg, uint8_t *data, uint32_t len)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress = TMF8828_I2C_ADDR;
    xfer.direction = kLPI2C_Read;
    xfer.subaddress = reg;
    xfer.subaddressSize = 1u;
    xfer.data = data;
    xfer.dataSize = len;
    xfer.flags = kLPI2C_TransferDefaultFlag;
    return (LPI2C_MasterTransferBlocking(TOF_I2C, &xfer) == kStatus_Success);
}

static bool tmf_wr8(uint8_t reg, uint8_t v)
{
    return tmf_i2c_write(reg, &v, 1u);
}

static bool tmf_rd8(uint8_t reg, uint8_t *v)
{
    return tmf_i2c_read(reg, v, 1u);
}

static bool tmf_wait_cpu_ready(void)
{
    for (uint32_t i = 0; i < 40u; i++)
    {
        uint8_t en = 0u;
        if (!tmf_rd8(TMF8828_REG_ENABLE, &en))
        {
            return false;
        }
        if ((en & (1u << 6)) != 0u)
        {
            return true;
        }
        SDK_DelayAtLeastUs(2000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }
    return false;
}

static bool tmf_load_common_config(void)
{
    if (!tmf_wr8(TMF8828_REG_CMD_STAT, TMF8828_CMD_LOAD_COMMON))
    {
        return false;
    }

    /* Use MRD recipe defaults: period=0 (host paced), kilo_iterations=125. */
    if (!tmf_wr8((uint8_t)(TMF8828_REG_PAGED_BASE + 0u), 0u)) return false;
    if (!tmf_wr8((uint8_t)(TMF8828_REG_PAGED_BASE + 1u), 0u)) return false;
    if (!tmf_wr8((uint8_t)(TMF8828_REG_PAGED_BASE + 2u), 125u)) return false;
    if (!tmf_wr8((uint8_t)(TMF8828_REG_PAGED_BASE + 3u), 0u)) return false;

    return tmf_wr8(TMF8828_REG_CMD_STAT, TMF8828_CMD_WRITE_CONFIG_PAGE);
}

bool tmf8828_quick_init(void)
{
    memset(&s_info, 0, sizeof(s_info));
    s_sensor_ready = false;

    if (!s_i2c_inited)
    {
        lpi2c_master_config_t cfg;
        LPI2C_MasterGetDefaultConfig(&cfg);
        cfg.baudRate_Hz = 400000u;
        LPI2C_MasterInit(TOF_I2C, &cfg, tmf_i2c_get_freq());
        s_i2c_inited = true;
    }

    uint8_t chip_id = 0u;
    uint8_t rev_id = 0u;
    if (!tmf_rd8(TMF8828_REG_ID, &chip_id) || !tmf_rd8(TMF8828_REG_REVID, &rev_id))
    {
        PRINTF("TOF: TMF8828 not responding at 0x%02x\r\n", TMF8828_I2C_ADDR);
        return false;
    }

    s_info.present = true;
    s_info.chip_id = (uint8_t)(chip_id & 0x3Fu);
    s_info.rev_id = (uint8_t)(rev_id & 0x07u);

    uint8_t en = 0u;
    if (!tmf_rd8(TMF8828_REG_ENABLE, &en)) return false;
    en |= 0x01u;
    if (!tmf_wr8(TMF8828_REG_ENABLE, en)) return false;

    if (!tmf_wait_cpu_ready())
    {
        PRINTF("TOF: CPU_READY timeout\r\n");
        return false;
    }

    if (!tmf_wr8(TMF8828_REG_APPID, 0x03u)) return false;
    if (!tmf_wr8(TMF8828_REG_INT_ENAB, TMF8828_INT_ENABLE_MASK)) return false;
    if (!tmf_load_common_config()) return false;
    if (!tmf_wr8(TMF8828_REG_CMD_STAT, TMF8828_CMD_MEASURE)) return false;

    s_sensor_ready = true;
    PRINTF("TOF: TMF8828 chip=0x%02x rev=%u ready\r\n", s_info.chip_id, s_info.rev_id);
    return true;
}

bool tmf8828_quick_get_info(tmf8828_info_t *out)
{
    if (!out) return false;
    *out = s_info;
    return s_info.present;
}

bool tmf8828_quick_read_8x8(uint16_t out_mm[64])
{
    if (!out_mm || !s_sensor_ready)
    {
        return false;
    }

    uint8_t int_status = 0u;
    if (!tmf_rd8(TMF8828_REG_INT_STATUS, &int_status))
    {
        return false;
    }

    (void)int_status;

    uint8_t cfg_res = 0u;
    (void)tmf_rd8(TMF8828_REG_CONFIG_RES, &cfg_res);

    uint8_t raw[128];
    if (!tmf_i2c_read(TMF8828_REG_PAGED_BASE, raw, sizeof(raw)))
    {
        return false;
    }

    uint32_t plausible = 0u;
    for (uint32_t i = 0; i < 64u; i++)
    {
        uint16_t v = (uint16_t)raw[2u * i] | ((uint16_t)raw[(2u * i) + 1u] << 8);
        out_mm[i] = v;
        if (v >= 50u && v <= 6000u)
        {
            plausible++;
        }
    }

    if (plausible >= 8u)
    {
        return true;
    }

    /* Retry once after re-issuing measure command to recover from page state drift. */
    (void)tmf_wr8(TMF8828_REG_CMD_STAT, TMF8828_CMD_MEASURE);
    SDK_DelayAtLeastUs(1000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

    if (!tmf_i2c_read(TMF8828_REG_PAGED_BASE, raw, sizeof(raw)))
    {
        return false;
    }

    plausible = 0u;
    for (uint32_t i = 0; i < 64u; i++)
    {
        uint16_t v = (uint16_t)raw[2u * i] | ((uint16_t)raw[(2u * i) + 1u] << 8);
        out_mm[i] = v;
        if (v >= 50u && v <= 6000u)
        {
            plausible++;
        }
    }

    return (plausible >= 8u);
}
