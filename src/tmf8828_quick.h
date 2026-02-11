#pragma once

#include <stdbool.h>
#include <stdint.h>

#define TMF8828_I2C_ADDR 0x41u

typedef struct
{
    bool present;
    uint8_t chip_id;
    uint8_t rev_id;
} tmf8828_info_t;

bool tmf8828_quick_init(void);
bool tmf8828_quick_get_info(tmf8828_info_t *out);
bool tmf8828_quick_read_8x8(uint16_t out_mm[64]);
