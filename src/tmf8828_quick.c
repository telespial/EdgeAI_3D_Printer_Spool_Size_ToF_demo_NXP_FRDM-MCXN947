#include "tmf8828_quick.h"

#include <string.h>

#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "fsl_lpi2c.h"
#include "fsl_port.h"

#include "tmf8828_patch.h"

#define TMF8828_REG_APPID        0x00u
#define TMF8828_REG_CMD_STAT     0x08u
#define TMF8828_REG_MODE         0x10u
#define TMF8828_REG_ACTIVE_RANGE 0x19u
#define TMF8828_REG_CONFIG_RES   0x20u
#define TMF8828_REG_ENABLE       0xE0u
#define TMF8828_REG_INT_STATUS   0xE1u
#define TMF8828_REG_INT_ENAB     0xE2u
#define TMF8828_REG_ID           0xE3u
#define TMF8828_REG_REVID        0xE4u

#define TMF8828_CMD_CLEAR_STATUS       0x11u
#define TMF8828_CMD_MEASURE            0x10u
#define TMF8828_CMD_WRITE_CONFIG_PAGE  0x15u
#define TMF8828_CMD_LOAD_COMMON        0x16u
#define TMF8828_CMD_SWITCH_TO_8X8      0x6Cu
#define TMF8828_CMD_STOP               0xFFu

#define TMF8828_BL_CMD_RAMREMAP  0x11u
#define TMF8828_BL_CMD_WR_RAM    0x41u
#define TMF8828_BL_CMD_ADDR_RAM  0x43u

#define TMF8828_APP_ID_APP        0x03u
#define TMF8828_APP_ID_BOOTLOADER 0x80u
#define TMF8828_CHIP_ID           0x08u
#define TMF8828_MODE_8X8          0x08u
#define TMF8828_ACTIVE_RANGE_SHORT 0x6Eu
#define TMF8828_ACTIVE_RANGE_LONG  0x6Fu

#define TMF8828_INT_RESULT_READY  0x02u
#define TMF8828_BOOT_CHUNK_SIZE   0x80u

#define TMF8828_FRAME_SIZE        132u
#define TMF8828_OBJ1_OFFSET       24u
#define TMF8828_OBJ_ENTRY_SIZE    6u
#define TMF8828_OBJ_ENTRIES_RAW   18u
#define TMF8828_ZONE_COUNT_8X8    16u
#define TMF8828_CAPTURE_COUNT_8X8 4u
#define TMF8828_MIN_CONFIDENCE    0u
#define TMF8828_CLOSE_CAL_MAX_MM  120u
#define TMF8828_TOO_CLOSE_MM      50u
#define TMF8828_ZONE_HOLD_FRAMES  10u
#define TMF8828_DUAL_OBJ_SPLIT_MM 300u
#define TMF8828_EXPECTED_MIN_MM   50u
#define TMF8828_EXPECTED_MAX_MM   150u
/* Drop out-of-range updates when the previously published zone value was in-range.
 * This suppresses near/far range toggling artifacts under close-hand occlusion.
 */
#ifndef TMF8828_RANGE_GLITCH_REJECT
#define TMF8828_RANGE_GLITCH_REJECT 1u
#endif
/* Object selection policy when both object returns are valid:
 * 0: nearest valid return
 * 1: prefer obj0; use obj1 only when obj0 is invalid
 * 2: strict obj0 only (ignore obj1 even if obj0 invalid)
 */
#ifndef TMF8828_OBJECT_SELECT_POLICY
#define TMF8828_OBJECT_SELECT_POLICY 2u
#endif
#ifndef TMF8828_MEAS_PERIOD_MS
#define TMF8828_MEAS_PERIOD_MS    24u
#endif
#define TMF8828_KILO_ITERATIONS   64u
#define TMF8828_LOW_THRESHOLD_MM  0u
#define TMF8828_HIGH_THRESHOLD_MM 0xFFFFu
#define TMF8828_INTERRUPT_MASK    0x3FFFFu
#define TMF8828_PERSISTENCE       0u
#define TMF8828_ALG_SETTING0      0x84u

/* 8x8 capture-to-zone map modes:
 * 0: interleaved 4x4 phases (default)
 * 1: 4x4 quadrants
 * 2: 8x2 row bands per capture
 * 3: 2x8 column bands per capture
 */
#ifndef TMF8828_ZONE_MAP_MODE
#define TMF8828_ZONE_MAP_MODE 1u
#endif

#ifndef TMF8828_CAPTURE_REMAP_0
#define TMF8828_CAPTURE_REMAP_0 1u
#endif
#ifndef TMF8828_CAPTURE_REMAP_1
#define TMF8828_CAPTURE_REMAP_1 3u
#endif
#ifndef TMF8828_CAPTURE_REMAP_2
#define TMF8828_CAPTURE_REMAP_2 0u
#endif
#ifndef TMF8828_CAPTURE_REMAP_3
#define TMF8828_CAPTURE_REMAP_3 2u
#endif

#ifndef TMF8828_TRACE_GRIDS
#define TMF8828_TRACE_GRIDS 0
#endif

#ifndef TMF8828_PACKET_DIAG
#define TMF8828_PACKET_DIAG 0
#endif

/* TMF8828 shield EN is typically routed on Arduino D6 (P1_2 on FRDM-MCXN947). */
#define TMF8828_EN_PORT PORT1
#define TMF8828_EN_GPIO GPIO1
#define TMF8828_EN_PIN  2u

/* Tunable close-range linear calibration:
 * corrected_mm = raw_mm * SCALE_Q10 / 1024 + OFFSET_MM
 */
#ifndef TMF8828_CLOSE_CAL_SCALE_Q10
#define TMF8828_CLOSE_CAL_SCALE_Q10 1024
#endif

#ifndef TMF8828_CLOSE_CAL_OFFSET_MM
#define TMF8828_CLOSE_CAL_OFFSET_MM 0
#endif

typedef struct
{
    LPI2C_Type *base;
    uint32_t flexcomm_idx;
    const char *name;
} tmf8828_bus_cfg_t;

static const tmf8828_bus_cfg_t s_buses[] = {
    {LPI2C2, 2u, "FC2"},
    {LPI2C3, 3u, "FC3"},
};

#define TMF8828_BUS_COUNT (sizeof(s_buses) / sizeof(s_buses[0]))

static bool s_bus_inited[TMF8828_BUS_COUNT] = {false};
static int32_t s_active_bus = -1;
static uint8_t s_active_addr = TMF8828_I2C_ADDR;
static bool s_sensor_ready = false;
static tmf8828_info_t s_info;

static uint16_t s_capture_mm[TMF8828_CAPTURE_COUNT_8X8][TMF8828_ZONE_COUNT_8X8];
static uint8_t s_capture_mask = 0u;
static uint8_t s_capture_sequence = 0u;
static bool s_capture_sequence_valid = false;
static uint16_t s_last_frame_mm[64];
static uint8_t s_zone_invalid_streak[64];
static uint16_t s_sequence_updated_total = 0u;
static uint32_t s_grid_counter = 0u;
static const uint8_t s_probe_addrs[] = {TMF8828_I2C_ADDR, 0x42u, 0x43u};

static void tmf_force_i2c_clock(uint32_t flexcomm_idx)
{
    switch (flexcomm_idx)
    {
        case 2u:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom2Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM2);
            break;
        case 3u:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom3Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM3);
            break;
        default:
            break;
    }
}

static void tmf_host_force_enable(void)
{
    CLOCK_EnableClock(kCLOCK_Port1);
    CLOCK_EnableClock(kCLOCK_Gpio1);

    const port_pin_config_t en_pin_cfg = {
        .pullSelect = kPORT_PullDisable,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainDisable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = kPORT_MuxAlt0,
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(TMF8828_EN_PORT, TMF8828_EN_PIN, &en_pin_cfg);

    const gpio_pin_config_t en_gpio_cfg = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = 1u,
    };
    GPIO_PinInit(TMF8828_EN_GPIO, TMF8828_EN_PIN, &en_gpio_cfg);
    GPIO_PinWrite(TMF8828_EN_GPIO, TMF8828_EN_PIN, 1u);
}

static bool tmf_get_bus_pins(uint32_t bus_idx, PORT_Type **port, GPIO_Type **gpio, uint32_t *sda_pin, uint32_t *scl_pin)
{
    if (!port || !gpio || !sda_pin || !scl_pin)
    {
        return false;
    }

    switch (bus_idx)
    {
        case 0u: /* FC2: P4_0/P4_1 */
            *port = PORT4;
            *gpio = GPIO4;
            *sda_pin = 0u;
            *scl_pin = 1u;
            return true;
        case 1u: /* FC3: P1_0/P1_1 */
            *port = PORT1;
            *gpio = GPIO1;
            *sda_pin = 0u;
            *scl_pin = 1u;
            return true;
        default:
            return false;
    }
}

static void tmf_enable_bus_pin_clocks(PORT_Type *port, GPIO_Type *gpio)
{
    if (port == PORT1)
    {
        CLOCK_EnableClock(kCLOCK_Port1);
    }
    else if (port == PORT4)
    {
        CLOCK_EnableClock(kCLOCK_Port4);
    }

    if (gpio == GPIO1)
    {
        CLOCK_EnableClock(kCLOCK_Gpio1);
    }
    else if (gpio == GPIO4)
    {
        CLOCK_EnableClock(kCLOCK_Gpio4);
    }
}

static bool tmf_recover_bus(uint32_t bus_idx)
{
    PORT_Type *port = NULL;
    GPIO_Type *gpio = NULL;
    uint32_t sda_pin = 0u;
    uint32_t scl_pin = 0u;

    if (!tmf_get_bus_pins(bus_idx, &port, &gpio, &sda_pin, &scl_pin))
    {
        return false;
    }

    tmf_enable_bus_pin_clocks(port, gpio);

    const port_pin_config_t gpio_od_cfg = {
        .pullSelect = kPORT_PullUp,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_SlowSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainEnable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = kPORT_MuxAlt0,
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(port, sda_pin, &gpio_od_cfg);
    PORT_SetPinConfig(port, scl_pin, &gpio_od_cfg);

    const gpio_pin_config_t out_high_cfg = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = 1u,
    };
    GPIO_PinInit(gpio, sda_pin, &out_high_cfg);
    GPIO_PinInit(gpio, scl_pin, &out_high_cfg);

    GPIO_PinWrite(gpio, sda_pin, 1u);
    GPIO_PinWrite(gpio, scl_pin, 1u);
    SDK_DelayAtLeastUs(10u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

    /* Release a stuck target by pulsing SCL and issuing a STOP condition. */
    for (uint32_t i = 0u; i < 18u; i++)
    {
        GPIO_PinWrite(gpio, scl_pin, 0u);
        SDK_DelayAtLeastUs(5u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
        GPIO_PinWrite(gpio, scl_pin, 1u);
        SDK_DelayAtLeastUs(5u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }

    GPIO_PinWrite(gpio, sda_pin, 0u);
    SDK_DelayAtLeastUs(5u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    GPIO_PinWrite(gpio, scl_pin, 1u);
    SDK_DelayAtLeastUs(5u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    GPIO_PinWrite(gpio, sda_pin, 1u);
    SDK_DelayAtLeastUs(5u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

    const port_pin_config_t i2c_cfg = {
        .pullSelect = kPORT_PullUp,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_SlowSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = kPORT_OpenDrainEnable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = kPORT_MuxAlt2,
        .inputBuffer = kPORT_InputBufferEnable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(port, sda_pin, &i2c_cfg);
    PORT_SetPinConfig(port, scl_pin, &i2c_cfg);

    return true;
}

static uint32_t tmf_zone_index_8x8(uint32_t capture, uint32_t zone16)
{
    static const uint8_t cap_remap[TMF8828_CAPTURE_COUNT_8X8] = {
        (uint8_t)(TMF8828_CAPTURE_REMAP_0 & 0x3u),
        (uint8_t)(TMF8828_CAPTURE_REMAP_1 & 0x3u),
        (uint8_t)(TMF8828_CAPTURE_REMAP_2 & 0x3u),
        (uint8_t)(TMF8828_CAPTURE_REMAP_3 & 0x3u),
    };

    const uint32_t cap = cap_remap[capture & 0x3u];
    uint32_t x = 0u;
    uint32_t y = 0u;

#if (TMF8828_ZONE_MAP_MODE == 1u)
    x = (zone16 & 0x3u) + ((cap & 0x1u) << 2u);
    y = ((zone16 >> 2u) & 0x3u) + (((cap >> 1u) & 0x1u) << 2u);
#elif (TMF8828_ZONE_MAP_MODE == 2u)
    x = zone16 & 0x7u;
    y = ((zone16 >> 3u) & 0x1u) + (cap << 1u);
#elif (TMF8828_ZONE_MAP_MODE == 3u)
    x = ((zone16 >> 3u) & 0x1u) + (cap << 1u);
    y = zone16 & 0x7u;
#else
    const uint32_t phase_x = cap & 0x1u;
    const uint32_t phase_y = (cap >> 1u) & 0x1u;
    const uint32_t local_x = zone16 & 0x3u;      /* 4 columns per subcapture */
    const uint32_t local_y = (zone16 >> 2u) & 0x3u; /* 4 rows per subcapture */
    x = (local_x << 1u) | phase_x;
    y = (local_y << 1u) | phase_y;
#endif

    return (y * 8u) + x;
}

static uint32_t tmf_i2c_get_freq(uint32_t bus_idx)
{
    return CLOCK_GetLPFlexCommClkFreq(s_buses[bus_idx].flexcomm_idx);
}

static bool tmf_i2c_init_bus(uint32_t bus_idx)
{
    if (bus_idx >= TMF8828_BUS_COUNT)
    {
        return false;
    }

    if (s_bus_inited[bus_idx])
    {
        return true;
    }

    lpi2c_master_config_t cfg;
    LPI2C_MasterGetDefaultConfig(&cfg);
    cfg.baudRate_Hz = 400000u;

    tmf_force_i2c_clock(s_buses[bus_idx].flexcomm_idx);

    const uint32_t src_hz = tmf_i2c_get_freq(bus_idx);
    if (src_hz == 0u)
    {
        PRINTF("TOF: %s clock source is 0 Hz\r\n", s_buses[bus_idx].name);
        return false;
    }

    LPI2C_MasterInit(s_buses[bus_idx].base, &cfg, src_hz);
    s_bus_inited[bus_idx] = true;
    return true;
}

static bool tmf_i2c_write_on_bus_addr(uint32_t bus_idx, uint8_t addr, uint8_t reg, const uint8_t *data, uint32_t len)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress = addr;
    xfer.direction = kLPI2C_Write;
    xfer.subaddress = reg;
    xfer.subaddressSize = 1u;
    xfer.data = (uint8_t *)data;
    xfer.dataSize = len;
    xfer.flags = kLPI2C_TransferDefaultFlag;
    return (LPI2C_MasterTransferBlocking(s_buses[bus_idx].base, &xfer) == kStatus_Success);
}

static bool tmf_i2c_read_on_bus_addr(uint32_t bus_idx, uint8_t addr, uint8_t reg, uint8_t *data, uint32_t len)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress = addr;
    xfer.direction = kLPI2C_Read;
    xfer.subaddress = reg;
    xfer.subaddressSize = 1u;
    xfer.data = data;
    xfer.dataSize = len;
    xfer.flags = kLPI2C_TransferDefaultFlag;
    return (LPI2C_MasterTransferBlocking(s_buses[bus_idx].base, &xfer) == kStatus_Success);
}

static status_t tmf_i2c_read_on_bus_addr_st(uint32_t bus_idx, uint8_t addr, uint8_t reg, uint8_t *data, uint32_t len)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress = addr;
    xfer.direction = kLPI2C_Read;
    xfer.subaddress = reg;
    xfer.subaddressSize = 1u;
    xfer.data = data;
    xfer.dataSize = len;
    xfer.flags = kLPI2C_TransferDefaultFlag;
    return LPI2C_MasterTransferBlocking(s_buses[bus_idx].base, &xfer);
}

static bool tmf_i2c_write(uint8_t reg, const uint8_t *data, uint32_t len)
{
    if (s_active_bus < 0)
    {
        return false;
    }
    return tmf_i2c_write_on_bus_addr((uint32_t)s_active_bus, s_active_addr, reg, data, len);
}

static bool tmf_i2c_read(uint8_t reg, uint8_t *data, uint32_t len)
{
    if (s_active_bus < 0)
    {
        return false;
    }
    return tmf_i2c_read_on_bus_addr((uint32_t)s_active_bus, s_active_addr, reg, data, len);
}

static bool tmf_wr8(uint8_t reg, uint8_t v)
{
    return tmf_i2c_write(reg, &v, 1u);
}

static bool tmf_rd8(uint8_t reg, uint8_t *v)
{
    return tmf_i2c_read(reg, v, 1u);
}

static bool tmf_probe_bus(uint32_t bus_idx, uint8_t addr, uint8_t *chip_id, uint8_t *rev_id)
{
    uint8_t id = 0u;
    uint8_t rev = 0u;
    status_t st = kStatus_Fail;

    if (!tmf_i2c_init_bus(bus_idx))
    {
        return false;
    }

    st = tmf_i2c_read_on_bus_addr_st(bus_idx, addr, TMF8828_REG_ID, &id, 1u);
    if (st == kStatus_LPI2C_Busy)
    {
        PRINTF("TOF: %s bus busy, attempting recovery\r\n", s_buses[bus_idx].name);
        (void)tmf_recover_bus(bus_idx);
        s_bus_inited[bus_idx] = false;
        if (tmf_i2c_init_bus(bus_idx))
        {
            st = tmf_i2c_read_on_bus_addr_st(bus_idx, addr, TMF8828_REG_ID, &id, 1u);
        }
    }
    if (st != kStatus_Success)
    {
        PRINTF("TOF: probe %s @0x%02x ID read failed st=%d\r\n",
               s_buses[bus_idx].name,
               addr,
               (int)st);
        return false;
    }
    st = tmf_i2c_read_on_bus_addr_st(bus_idx, addr, TMF8828_REG_REVID, &rev, 1u);
    if (st == kStatus_LPI2C_Busy)
    {
        PRINTF("TOF: %s bus busy on REVID, attempting recovery\r\n", s_buses[bus_idx].name);
        (void)tmf_recover_bus(bus_idx);
        s_bus_inited[bus_idx] = false;
        if (tmf_i2c_init_bus(bus_idx))
        {
            st = tmf_i2c_read_on_bus_addr_st(bus_idx, addr, TMF8828_REG_REVID, &rev, 1u);
        }
    }
    if (st != kStatus_Success)
    {
        PRINTF("TOF: probe %s @0x%02x REVID read failed st=%d\r\n",
               s_buses[bus_idx].name,
               addr,
               (int)st);
        return false;
    }

    if (chip_id)
    {
        *chip_id = id;
    }
    if (rev_id)
    {
        *rev_id = rev;
    }
    return true;
}

static bool tmf_select_bus(uint8_t *chip_id, uint8_t *rev_id)
{
    for (uint32_t i = 0; i < TMF8828_BUS_COUNT; i++)
    {
        for (uint32_t a = 0u; a < (sizeof(s_probe_addrs) / sizeof(s_probe_addrs[0])); a++)
        {
            uint8_t id = 0u;
            uint8_t rev = 0u;
            const uint8_t addr = s_probe_addrs[a];

            if (!tmf_probe_bus(i, addr, &id, &rev))
            {
                continue;
            }

            if ((id & 0x3Fu) != TMF8828_CHIP_ID)
            {
                PRINTF("TOF: probe on %s @0x%02x got unexpected ID=0x%02x\r\n",
                       s_buses[i].name,
                       addr,
                       id);
                continue;
            }

            s_active_bus = (int32_t)i;
            s_active_addr = addr;
            if (chip_id)
            {
                *chip_id = id;
            }
            if (rev_id)
            {
                *rev_id = rev;
            }
            return true;
        }
    }

    return false;
}

static bool tmf_wait_cpu_ready(void)
{
    for (uint32_t i = 0; i < 80u; i++)
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

static uint8_t tmf_bl_checksum(const uint8_t *data, uint32_t len)
{
    uint32_t sum = 0u;
    for (uint32_t i = 0; i < len; i++)
    {
        sum += data[i];
    }
    return (uint8_t)(0xFFu ^ (sum & 0xFFu));
}

static bool tmf_bl_wait_response(uint8_t cmd, uint8_t *payload_out, uint8_t payload_len, uint32_t timeout_us)
{
    uint8_t rx[3u + TMF8828_BOOT_CHUNK_SIZE];
    const uint32_t rx_len = (uint32_t)payload_len + 3u;
    const uint32_t polls = (timeout_us / 1000u) + 1u;

    for (uint32_t i = 0; i < polls; i++)
    {
        if (!tmf_i2c_read(TMF8828_REG_CMD_STAT, rx, rx_len))
        {
            return false;
        }

        if (rx[0] == cmd)
        {
            SDK_DelayAtLeastUs(1000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
            continue;
        }

        const uint8_t status = rx[0];
        const uint8_t actual_payload_len = rx[1];
        const uint8_t checksum = rx[2u + payload_len];
        const uint8_t calc = tmf_bl_checksum(&rx[1], (uint32_t)payload_len + 1u);

        if (status != 0u || actual_payload_len != payload_len || checksum != calc)
        {
            PRINTF("TOF: BL response err st=0x%02x len=%u chk=0x%02x calc=0x%02x\r\n",
                   status,
                   actual_payload_len,
                   checksum,
                   calc);
            return false;
        }

        if (payload_len > 0u && payload_out != NULL)
        {
            memcpy(payload_out, &rx[2], payload_len);
        }
        return true;
    }

    PRINTF("TOF: BL command 0x%02x timeout\r\n", cmd);
    return false;
}

static bool tmf_bl_send_cmd(uint8_t cmd, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t tx[3u + TMF8828_BOOT_CHUNK_SIZE];
    tx[0] = cmd;
    tx[1] = payload_len;
    if (payload_len > 0u && payload != NULL)
    {
        memcpy(&tx[2], payload, payload_len);
    }
    tx[2u + payload_len] = tmf_bl_checksum(tx, (uint32_t)payload_len + 2u);

    if (!tmf_i2c_write(TMF8828_REG_CMD_STAT, tx, (uint32_t)payload_len + 3u))
    {
        return false;
    }

    return tmf_bl_wait_response(cmd, NULL, 0u, 30000u);
}

static bool tmf_bootloader_download_patch(void)
{
    const uint8_t addr_payload[2] = {
        (uint8_t)(TMF8828_PATCH_ADDR_LO16 & 0xFFu),
        (uint8_t)((TMF8828_PATCH_ADDR_LO16 >> 8) & 0xFFu),
    };

    if (!tmf_bl_send_cmd(TMF8828_BL_CMD_ADDR_RAM, addr_payload, (uint8_t)sizeof(addr_payload)))
    {
        PRINTF("TOF: BL set RAM addr failed\r\n");
        return false;
    }

    for (uint32_t off = 0u; off < TMF8828_PATCH_SIZE; off += TMF8828_BOOT_CHUNK_SIZE)
    {
        uint8_t chunk = (uint8_t)((TMF8828_PATCH_SIZE - off) > TMF8828_BOOT_CHUNK_SIZE ?
                                  TMF8828_BOOT_CHUNK_SIZE :
                                  (TMF8828_PATCH_SIZE - off));
        if (!tmf_bl_send_cmd(TMF8828_BL_CMD_WR_RAM, &g_tmf8828_patch_data[off], chunk))
        {
            PRINTF("TOF: BL patch chunk write failed at %lu\r\n", (unsigned long)off);
            return false;
        }
    }

    return true;
}

static bool tmf_bootloader_start_ram_app(void)
{
    uint8_t frame[3];
    frame[0] = TMF8828_BL_CMD_RAMREMAP;
    frame[1] = 0u;
    frame[2] = tmf_bl_checksum(frame, 2u);

    if (!tmf_i2c_write(TMF8828_REG_CMD_STAT, frame, sizeof(frame)))
    {
        return false;
    }

    for (uint32_t i = 0; i < 120u; i++)
    {
        uint8_t appid = 0u;
        if (tmf_rd8(TMF8828_REG_APPID, &appid) && appid == TMF8828_APP_ID_APP)
        {
            return true;
        }
        SDK_DelayAtLeastUs(2000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }

    return false;
}

static bool tmf_send_cmd_expect(uint8_t cmd, uint8_t expected, uint32_t timeout_us)
{
    if (!tmf_wr8(TMF8828_REG_CMD_STAT, cmd))
    {
        return false;
    }

    const uint32_t polls = (timeout_us / 1000u) + 1u;
    for (uint32_t i = 0; i < polls; i++)
    {
        uint8_t st = 0u;
        if (!tmf_rd8(TMF8828_REG_CMD_STAT, &st))
        {
            return false;
        }

        if (cmd == TMF8828_CMD_MEASURE)
        {
            if (st == 0u || st == 1u)
            {
                return true;
            }
        }
        else if (st == expected)
        {
            return true;
        }

        if (st != cmd && st > 1u)
        {
            PRINTF("TOF: command 0x%02x failed with status 0x%02x\r\n", cmd, st);
            return false;
        }

        SDK_DelayAtLeastUs(1000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }

    PRINTF("TOF: command 0x%02x timeout\r\n", cmd);
    return false;
}

static bool tmf_load_8x8_config(void)
{
    for (uint32_t attempt = 0u; attempt < 3u; attempt++)
    {
        if (!tmf_send_cmd_expect(TMF8828_CMD_LOAD_COMMON, 0u, 50000u))
        {
            SDK_DelayAtLeastUs(10000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
            continue;
        }

        /* Match v14 reference flow: write common config block at 0x24..0x2F. */
        uint8_t cfg_block[12] = {
            (uint8_t)(TMF8828_MEAS_PERIOD_MS & 0xFFu),
            (uint8_t)((TMF8828_MEAS_PERIOD_MS >> 8) & 0xFFu),
            (uint8_t)(TMF8828_KILO_ITERATIONS & 0xFFu),
            (uint8_t)((TMF8828_KILO_ITERATIONS >> 8) & 0xFFu),
            (uint8_t)(TMF8828_LOW_THRESHOLD_MM & 0xFFu),
            (uint8_t)((TMF8828_LOW_THRESHOLD_MM >> 8) & 0xFFu),
            (uint8_t)(TMF8828_HIGH_THRESHOLD_MM & 0xFFu),
            (uint8_t)((TMF8828_HIGH_THRESHOLD_MM >> 8) & 0xFFu),
            (uint8_t)(TMF8828_INTERRUPT_MASK & 0xFFu),
            (uint8_t)((TMF8828_INTERRUPT_MASK >> 8) & 0xFFu),
            (uint8_t)((TMF8828_INTERRUPT_MASK >> 16) & 0xFFu),
            TMF8828_PERSISTENCE,
        };
        if (!tmf_i2c_write(0x24u, cfg_block, sizeof(cfg_block)))
        {
            return false;
        }

        if (!tmf_wr8(0x34u, 15u)) return false;              /* SPAD_MAP_ID map #15 for 8x8 */
        if (!tmf_wr8(0x39u, 0u)) return false;               /* histogram dump off */
        if (!tmf_wr8(0x35u, TMF8828_ALG_SETTING0)) return false; /* enable logarithmic confidence output */

        if (tmf_send_cmd_expect(TMF8828_CMD_WRITE_CONFIG_PAGE, 0u, 60000u))
        {
            return true;
        }

        SDK_DelayAtLeastUs(10000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }

    return false;
}

static bool tmf_switch_to_8x8_mode(void)
{
    if (!tmf_send_cmd_expect(TMF8828_CMD_SWITCH_TO_8X8, 0u, 50000u))
    {
        return false;
    }

    for (uint32_t i = 0; i < 40u; i++)
    {
        uint8_t mode = 0u;
        if (!tmf_rd8(TMF8828_REG_MODE, &mode))
        {
            return false;
        }
        if (mode == TMF8828_MODE_8X8)
        {
            return true;
        }
        SDK_DelayAtLeastUs(1000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }

    PRINTF("TOF: mode switch to 8x8 did not complete\r\n");
    return false;
}

static bool tmf_enable_short_range_mode(void)
{
    uint8_t active_range = 0u;
    if (!tmf_rd8(TMF8828_REG_ACTIVE_RANGE, &active_range))
    {
        return false;
    }

    if (active_range == 0x00u)
    {
        PRINTF("TOF: short-range mode not supported by current app build\r\n");
        return true;
    }

    if (active_range == TMF8828_ACTIVE_RANGE_SHORT)
    {
        return true;
    }

    if (!tmf_wr8(TMF8828_REG_ACTIVE_RANGE, TMF8828_ACTIVE_RANGE_SHORT))
    {
        return false;
    }

    SDK_DelayAtLeastUs(3000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

    if (!tmf_rd8(TMF8828_REG_ACTIVE_RANGE, &active_range))
    {
        return false;
    }

    if (active_range != TMF8828_ACTIVE_RANGE_SHORT)
    {
        PRINTF("TOF: short-range request not applied (active_range=0x%02x)\r\n", active_range);
    }
    else
    {
        PRINTF("TOF: short-range accuracy mode enabled (active_range=0x%02x)\r\n", active_range);
    }

    return true;
}

static uint16_t tmf_apply_close_calibration(uint16_t raw_mm)
{
    if (raw_mm == 0u || raw_mm > TMF8828_CLOSE_CAL_MAX_MM)
    {
        return raw_mm;
    }

    int32_t corrected = (int32_t)(((int32_t)raw_mm * (int32_t)TMF8828_CLOSE_CAL_SCALE_Q10 + 512) / 1024);
    corrected += (int32_t)TMF8828_CLOSE_CAL_OFFSET_MM;

    if (corrected < 0)
    {
        corrected = 0;
    }
    if (corrected > 65535)
    {
        corrected = 65535;
    }

    return (uint16_t)corrected;
}

static uint16_t tmf_decode_distance_mm(uint8_t b0, uint8_t b1)
{
    /* Some host examples/documentation encode object distance words with
     * inconsistent byte-order descriptions. Prefer little-endian, but fall
     * back to big-endian when LE is implausible.
     */
    const uint16_t raw_le = (uint16_t)b0 | ((uint16_t)b1 << 8);
    const uint16_t raw_be = (uint16_t)b1 | ((uint16_t)b0 << 8);

    const uint16_t d_le = tmf_apply_close_calibration(raw_le);
    if (d_le > 0u && d_le < 12000u)
    {
        return d_le;
    }

    const uint16_t d_be = tmf_apply_close_calibration(raw_be);
    if (d_be > 0u && d_be < 12000u)
    {
        return d_be;
    }

    if (d_le == 0u || d_be == 0u)
    {
        return 0u;
    }

    return 0u;
}

static void tmf_fill_sparse_zones(uint16_t frame[64])
{
    uint16_t src[64];
    memcpy(src, frame, sizeof(src));

    for (uint32_t y = 0u; y < 8u; y++)
    {
        for (uint32_t x = 0u; x < 8u; x++)
        {
            const uint32_t idx = (y * 8u) + x;
            if (src[idx] > 0u && src[idx] < 12000u)
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
                    if (nx < 0 || nx >= 8 || ny < 0 || ny >= 8)
                    {
                        continue;
                    }

                    const uint16_t v = src[(uint32_t)ny * 8u + (uint32_t)nx];
                    if (v > 0u && v < 12000u)
                    {
                        sum += v;
                        count++;
                    }
                }
            }

            /* Corners only have 3 neighbors; allow fill when at least two are valid
             * to avoid persistent dead corner cells under close-range occlusion.
             */
            if (count >= 2u)
            {
                frame[idx] = (uint16_t)(sum / count);
            }
        }
    }
}

static bool tmf_start_measurement(void)
{
    if (!tmf_wr8(TMF8828_REG_INT_STATUS, TMF8828_INT_RESULT_READY))
    {
        return false;
    }
    if (!tmf_wr8(TMF8828_REG_INT_ENAB, TMF8828_INT_RESULT_READY))
    {
        return false;
    }
    return tmf_send_cmd_expect(TMF8828_CMD_MEASURE, 1u, 30000u);
}

bool tmf8828_quick_init(void)
{
    memset(&s_info, 0, sizeof(s_info));
    memset(s_capture_mm, 0, sizeof(s_capture_mm));
    memset(s_last_frame_mm, 0, sizeof(s_last_frame_mm));
    memset(s_zone_invalid_streak, 0, sizeof(s_zone_invalid_streak));
    s_capture_mask = 0u;
    s_capture_sequence = 0u;
    s_capture_sequence_valid = false;
    s_sequence_updated_total = 0u;
    s_sequence_updated_total = 0u;
    s_grid_counter = 0u;
    s_sensor_ready = false;
    s_active_bus = -1;
    s_active_addr = TMF8828_I2C_ADDR;
    tmf_host_force_enable();
    SDK_DelayAtLeastUs(3000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

    uint8_t chip_id = 0u;
    uint8_t rev_id = 0u;
    if (!tmf_select_bus(&chip_id, &rev_id))
    {
        PRINTF("TOF: TMF8828 not responding at 0x41/0x42/0x43 on FC2/FC3\r\n");
        return false;
    }

    s_info.present = true;
    s_info.chip_id = (uint8_t)(chip_id & 0x3Fu);
    s_info.rev_id = (uint8_t)(rev_id & 0x07u);

    uint8_t en = 0u;
    if (!tmf_rd8(TMF8828_REG_ENABLE, &en))
    {
        return false;
    }
    en |= 0x01u;
    if (!tmf_wr8(TMF8828_REG_ENABLE, en))
    {
        return false;
    }

    if (!tmf_wait_cpu_ready())
    {
        PRINTF("TOF: CPU_READY timeout on %s\r\n", s_buses[s_active_bus].name);
        return false;
    }

    uint8_t appid = 0u;
    if (!tmf_rd8(TMF8828_REG_APPID, &appid))
    {
        return false;
    }

    if (appid == TMF8828_APP_ID_BOOTLOADER)
    {
        PRINTF("TOF: bootloader detected on %s, downloading patch (%u bytes)\r\n",
               s_buses[s_active_bus].name,
               (unsigned)TMF8828_PATCH_SIZE);

        if (!tmf_bootloader_download_patch() || !tmf_bootloader_start_ram_app())
        {
            PRINTF("TOF: patch download/start failed\r\n");
            return false;
        }

        if (!tmf_rd8(TMF8828_REG_APPID, &appid))
        {
            return false;
        }
    }

    if (appid != TMF8828_APP_ID_APP)
    {
        PRINTF("TOF: unexpected APPID=0x%02x (expected 0x03)\r\n", appid);
        return false;
    }

    /* Give the RAM app time to settle before page/config commands. */
    SDK_DelayAtLeastUs(100000u, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

    (void)tmf_send_cmd_expect(TMF8828_CMD_STOP, 0u, 30000u);
    (void)tmf_send_cmd_expect(TMF8828_CMD_CLEAR_STATUS, 0u, 30000u);

    if (!tmf_switch_to_8x8_mode())
    {
        return false;
    }

    if (!tmf_enable_short_range_mode())
    {
        PRINTF("TOF: failed to configure short-range mode\r\n");
        return false;
    }

    if (!tmf_load_8x8_config())
    {
        PRINTF("TOF: failed to load 8x8 config\r\n");
        return false;
    }

    if (!tmf_start_measurement())
    {
        PRINTF("TOF: failed to start measurement\r\n");
        return false;
    }

    s_sensor_ready = true;
    PRINTF("TOF: TMF8828 chip=0x%02x rev=%u ready on %s @0x%02x (8x8 mode)\r\n",
           s_info.chip_id,
           s_info.rev_id,
           s_buses[s_active_bus].name,
           s_active_addr);
    PRINTF("TOF: map mode=%u remap=[%u,%u,%u,%u]\r\n",
           (unsigned)TMF8828_ZONE_MAP_MODE,
           (unsigned)(TMF8828_CAPTURE_REMAP_0 & 0x3u),
           (unsigned)(TMF8828_CAPTURE_REMAP_1 & 0x3u),
           (unsigned)(TMF8828_CAPTURE_REMAP_2 & 0x3u),
           (unsigned)(TMF8828_CAPTURE_REMAP_3 & 0x3u));

    return true;
}

bool tmf8828_quick_get_info(tmf8828_info_t *out)
{
    if (!out)
    {
        return false;
    }
    *out = s_info;
    return s_info.present;
}

bool tmf8828_quick_read_8x8(uint16_t out_mm[64], bool *out_complete)
{
    if (out_complete)
    {
        *out_complete = false;
    }

    if (!out_mm || !s_sensor_ready)
    {
        return false;
    }

    uint8_t int_status = 0u;
    if (!tmf_rd8(TMF8828_REG_INT_STATUS, &int_status))
    {
        return false;
    }

    if ((int_status & TMF8828_INT_RESULT_READY) == 0u)
    {
        return false;
    }

    (void)tmf_wr8(TMF8828_REG_INT_STATUS, TMF8828_INT_RESULT_READY);

    uint8_t frame[TMF8828_FRAME_SIZE];
    if (!tmf_i2c_read(TMF8828_REG_CONFIG_RES, frame, sizeof(frame)))
    {
        return false;
    }

    if (frame[0] != TMF8828_CMD_MEASURE)
    {
        return false;
    }

    const uint8_t result_number = frame[4];
    const uint8_t capture = (uint8_t)(result_number & 0x03u);
    const uint8_t sequence = (uint8_t)(result_number >> 2u);

    if (capture >= TMF8828_CAPTURE_COUNT_8X8)
    {
        return false;
    }

    if (!s_capture_sequence_valid)
    {
        s_capture_sequence = sequence;
        s_capture_sequence_valid = true;
        s_capture_mask = 0u;
        s_sequence_updated_total = 0u;
    }
    else if (sequence != s_capture_sequence)
    {
        /* Start a new 8x8 assembly window when sequence id changes. */
        s_capture_sequence = sequence;
        s_capture_mask = 0u;
        s_sequence_updated_total = 0u;
    }

    uint32_t updated_zones = 0u;
    uint32_t zone_out = 0u;
    uint16_t candidate_zone_mm[TMF8828_ZONE_COUNT_8X8];
    uint8_t candidate_mode[TMF8828_ZONE_COUNT_8X8];
    bool packet_any_signal = false;
    bool suppress_empty_packet = false;
    memset(candidate_zone_mm, 0, sizeof(candidate_zone_mm));
    memset(candidate_mode, 0, sizeof(candidate_mode));
#if TMF8828_PACKET_DIAG
    uint32_t raw_slots_used = 0u;
    uint32_t raw_slots_skipped = 0u;
    uint32_t raw_valid_mm_samples = 0u;
    uint16_t raw_min_mm = 0xFFFFu;
    uint16_t raw_max_mm = 0u;
    uint32_t conf_sum = 0u;
    uint32_t conf_samples = 0u;
    uint8_t conf_min = 0xFFu;
    uint8_t conf_max = 0u;
    uint32_t obj0_selected = 0u;
    uint32_t obj1_selected = 0u;
    uint32_t dual_obj_zones = 0u;
    uint32_t dual_obj_split = 0u;
    uint32_t range_glitch_reject = 0u;
#endif

    for (uint32_t raw_idx = 0; raw_idx < TMF8828_OBJ_ENTRIES_RAW; raw_idx++)
    {
        /* In TMF8828 8x8 mode, raw object slots 8 and 17 are unused. */
        if (raw_idx == 8u || raw_idx == 17u)
        {
#if TMF8828_PACKET_DIAG
            raw_slots_skipped++;
#endif
            continue;
        }

        if (zone_out >= TMF8828_ZONE_COUNT_8X8)
        {
            break;
        }

#if TMF8828_PACKET_DIAG
        raw_slots_used++;
#endif

        const uint32_t off = TMF8828_OBJ1_OFFSET + (raw_idx * TMF8828_OBJ_ENTRY_SIZE);
        /* Object-entry layout used by current target firmware:
         * confidence, distance_lsb, distance_msb.
         */
        const uint8_t obj0_conf = frame[off + 0u];
        const uint16_t obj0_dist_raw = (uint16_t)frame[off + 1u] | ((uint16_t)frame[off + 2u] << 8);
        const uint8_t obj1_conf = frame[off + 3u];
        const uint16_t obj1_dist_raw = (uint16_t)frame[off + 4u] | ((uint16_t)frame[off + 5u] << 8);
        if (obj0_conf > 0u || obj1_conf > 0u || obj0_dist_raw > 0u || obj1_dist_raw > 0u)
        {
            packet_any_signal = true;
        }

        const uint16_t d0 = tmf_decode_distance_mm(frame[off + 1u], frame[off + 2u]);
        const uint16_t d1 = tmf_decode_distance_mm(frame[off + 4u], frame[off + 5u]);
        const bool d0_valid = (d0 > 0u && d0 < 12000u);
        const bool d1_valid = (d1 > 0u && d1 < 12000u);

#if TMF8828_PACKET_DIAG
        conf_sum += (uint32_t)obj0_conf + (uint32_t)obj1_conf;
        conf_samples += 2u;
        if (obj0_conf < conf_min)
        {
            conf_min = obj0_conf;
        }
        if (obj1_conf < conf_min)
        {
            conf_min = obj1_conf;
        }
        if (obj0_conf > conf_max)
        {
            conf_max = obj0_conf;
        }
        if (obj1_conf > conf_max)
        {
            conf_max = obj1_conf;
        }

        if (d0_valid)
        {
            raw_valid_mm_samples++;
            if (d0 < raw_min_mm)
            {
                raw_min_mm = d0;
            }
            if (d0 > raw_max_mm)
            {
                raw_max_mm = d0;
            }
        }
        if (d1_valid)
        {
            raw_valid_mm_samples++;
            if (d1 < raw_min_mm)
            {
                raw_min_mm = d1;
            }
            if (d1 > raw_max_mm)
            {
                raw_max_mm = d1;
            }
        }
#endif

        const bool obj0_ok = (d0_valid && d0 > 0u && obj0_conf >= TMF8828_MIN_CONFIDENCE);
        const bool obj1_ok = (d1_valid && d1 > 0u && obj1_conf >= TMF8828_MIN_CONFIDENCE);
        uint16_t chosen = 0u;
        uint8_t chosen_obj = 0u;

        if (obj0_ok && obj1_ok)
        {
#if TMF8828_PACKET_DIAG
            dual_obj_zones++;
            const uint16_t diff = (d0 > d1) ? (uint16_t)(d0 - d1) : (uint16_t)(d1 - d0);
            if (diff >= TMF8828_DUAL_OBJ_SPLIT_MM)
            {
                dual_obj_split++;
            }
#endif
#if (TMF8828_OBJECT_SELECT_POLICY == 0u)
            if (d0 <= d1)
            {
                chosen = d0;
                chosen_obj = 0u;
            }
            else
            {
                chosen = d1;
                chosen_obj = 1u;
            }
#else
            chosen = d0;
            chosen_obj = 0u;
#endif
        }
        else if (obj0_ok)
        {
            chosen = d0;
            chosen_obj = 0u;
        }
#if (TMF8828_OBJECT_SELECT_POLICY != 2u)
        else if (obj1_ok)
        {
            chosen = d1;
            chosen_obj = 1u;
        }
#endif

#if !TMF8828_PACKET_DIAG
        (void)chosen_obj;
#endif

        if (chosen > 0u)
        {
            candidate_zone_mm[zone_out] = chosen;
            candidate_mode[zone_out] = 1u;
#if TMF8828_PACKET_DIAG
            if (chosen_obj == 0u)
            {
                obj0_selected++;
            }
            else
            {
                obj1_selected++;
            }
#endif
        }
        else if (((obj0_dist_raw == 0u) && (obj0_conf > 0u)) ||
                 ((obj1_dist_raw == 0u) && (obj1_conf > 0u)))
        {
            /* Close saturation often reports 0mm with non-zero confidence. */
            candidate_zone_mm[zone_out] = TMF8828_TOO_CLOSE_MM;
            candidate_mode[zone_out] = 1u;
        }
        else
        {
            candidate_mode[zone_out] = 2u;
        }

        zone_out++;
    }

    if (!packet_any_signal && s_sequence_updated_total > 0u)
    {
        /* Occasionally a subcapture payload is all-zero while neighboring
         * subcaptures in the same sequence are valid. Keep the last-good zones.
         */
        suppress_empty_packet = true;
    }

    for (uint32_t z = 0u; z < zone_out; z++)
    {
        const uint32_t dst = tmf_zone_index_8x8(capture, z);
        if (dst >= 64u)
        {
            continue;
        }

        if (candidate_mode[z] == 1u)
        {
            uint16_t mm = candidate_zone_mm[z];
#if TMF8828_RANGE_GLITCH_REJECT
            const uint16_t prev = s_last_frame_mm[dst];
            const bool mm_in_range = (mm >= TMF8828_EXPECTED_MIN_MM && mm <= TMF8828_EXPECTED_MAX_MM);
            const bool prev_in_range = (prev >= TMF8828_EXPECTED_MIN_MM && prev <= TMF8828_EXPECTED_MAX_MM);
            if (!mm_in_range && prev_in_range)
            {
#if TMF8828_PACKET_DIAG
                range_glitch_reject++;
#endif
                continue;
            }
#endif
            s_capture_mm[capture][z] = mm;
            s_last_frame_mm[dst] = mm;
            s_zone_invalid_streak[dst] = 0u;
            updated_zones++;
        }
        else
        {
            s_capture_mm[capture][z] = 0u;
            if (suppress_empty_packet)
            {
                continue;
            }

            if (s_last_frame_mm[dst] > 0u && s_zone_invalid_streak[dst] < TMF8828_ZONE_HOLD_FRAMES)
            {
                s_zone_invalid_streak[dst]++;
            }
            else
            {
                s_last_frame_mm[dst] = 0u;
                if (s_zone_invalid_streak[dst] < 255u)
                {
                    s_zone_invalid_streak[dst]++;
                }
            }
        }
    }

    if (s_sequence_updated_total <= (uint16_t)(0xFFFFu - updated_zones))
    {
        s_sequence_updated_total = (uint16_t)(s_sequence_updated_total + updated_zones);
    }
    else
    {
        s_sequence_updated_total = 0xFFFFu;
    }

    s_capture_mask |= (uint8_t)(1u << capture);
    const uint8_t capture_mask_for_log = s_capture_mask;
    const bool complete_cycle = (capture_mask_for_log == 0x0Fu);
    if (complete_cycle)
    {
        s_capture_mask = 0u;
    }

#if TMF8828_PACKET_DIAG
    uint32_t valid_before_fill = 0u;
    for (uint32_t i = 0u; i < 64u; i++)
    {
        if (s_last_frame_mm[i] > 0u && s_last_frame_mm[i] < 12000u)
        {
            valid_before_fill++;
        }
    }
#endif

    memcpy(out_mm, s_last_frame_mm, sizeof(s_last_frame_mm));
    if (updated_zones > 0u)
    {
        tmf_fill_sparse_zones(out_mm);
        memcpy(s_last_frame_mm, out_mm, sizeof(s_last_frame_mm));
    }

#if TMF8828_PACKET_DIAG
    uint32_t valid_after_fill = 0u;
    for (uint32_t i = 0u; i < 64u; i++)
    {
        if (out_mm[i] > 0u && out_mm[i] < 12000u)
        {
            valid_after_fill++;
        }
    }
#endif

#if TMF8828_PACKET_DIAG
    const uint16_t diag_min_mm = (raw_valid_mm_samples > 0u) ? raw_min_mm : 0u;
    const uint16_t diag_max_mm = (raw_valid_mm_samples > 0u) ? raw_max_mm : 0u;
    const uint8_t diag_conf_min = (conf_samples > 0u) ? conf_min : 0u;
    const uint8_t diag_conf_max = (conf_samples > 0u) ? conf_max : 0u;
    const uint32_t diag_conf_avg =
        (conf_samples > 0u) ? (uint32_t)((conf_sum + (conf_samples / 2u)) / conf_samples) : 0u;

    PRINTF("TOF PKT r=%u s=%u c=%u ru=%u rs=%u vp=%u va=%u d=%u-%u cf=%u-%u/%u u=%u ob=%u/%u ds=%u/%u rg=%u%s%s\r\n",
           (unsigned)result_number,
           (unsigned)sequence,
           (unsigned)capture,
           (unsigned)raw_slots_used,
           (unsigned)raw_slots_skipped,
           (unsigned)valid_before_fill,
           (unsigned)valid_after_fill,
           (unsigned)diag_min_mm,
           (unsigned)diag_max_mm,
           (unsigned)diag_conf_min,
           (unsigned)diag_conf_max,
           (unsigned)diag_conf_avg,
           (unsigned)updated_zones,
           (unsigned)obj0_selected,
           (unsigned)obj1_selected,
           (unsigned)dual_obj_split,
           (unsigned)dual_obj_zones,
           (unsigned)range_glitch_reject,
           complete_cycle ? " complete" : "",
           suppress_empty_packet ? " drop0" : "");
#endif

#if TMF8828_TRACE_GRIDS
    uint32_t valid_zones = 0u;
    for (uint32_t i = 0u; i < 64u; i++)
    {
        const uint16_t v = out_mm[i];
        if (v > 0u && v < 12000u)
        {
            valid_zones++;
        }
    }

    const uint32_t captures_seen =
        ((capture_mask_for_log & 0x1u) ? 1u : 0u) +
        ((capture_mask_for_log & 0x2u) ? 1u : 0u) +
        ((capture_mask_for_log & 0x4u) ? 1u : 0u) +
        ((capture_mask_for_log & 0x8u) ? 1u : 0u);
    const uint32_t packets_seen = captures_seen * TMF8828_ZONE_COUNT_8X8;

    PRINTF("TOF DBG: seq=%u cap=%u mask=0x%01x packets=%u/64 valid=%u updated=%u%s\r\n",
           (unsigned)sequence,
           (unsigned)capture,
           (unsigned)capture_mask_for_log,
           (unsigned)packets_seen,
           (unsigned)valid_zones,
           (unsigned)s_sequence_updated_total,
           complete_cycle ? " complete" : "");

#endif

    if (complete_cycle)
    {
        s_grid_counter++;
        s_sequence_updated_total = 0u;
    }

    if (out_complete)
    {
        *out_complete = complete_cycle;
    }

    return true;
}

bool tmf8828_quick_restart_measurement(void)
{
    if (!s_sensor_ready)
    {
        return false;
    }

    (void)tmf_send_cmd_expect(TMF8828_CMD_STOP, 0u, 30000u);
    (void)tmf_send_cmd_expect(TMF8828_CMD_CLEAR_STATUS, 0u, 30000u);
    memset(s_capture_mm, 0, sizeof(s_capture_mm));
    memset(s_last_frame_mm, 0, sizeof(s_last_frame_mm));
    memset(s_zone_invalid_streak, 0, sizeof(s_zone_invalid_streak));
    s_capture_mask = 0u;
    s_capture_sequence = 0u;
    s_capture_sequence_valid = false;

    if (!tmf_start_measurement())
    {
        return false;
    }

    PRINTF("TOF: stream restarted\r\n");
    return true;
}
