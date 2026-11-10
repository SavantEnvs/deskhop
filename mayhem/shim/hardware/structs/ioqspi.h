#pragma once
#include <stdint.h>
enum gpio_override { GPIO_OVERRIDE_NORMAL = 0, GPIO_OVERRIDE_LOW = 2 };
#define IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB  12
#define IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS 0x3000u
typedef struct { struct { uint32_t status; uint32_t ctrl; } io[8]; } ioqspi_hw_t;
extern ioqspi_hw_t mayhem_fake_ioqspi;
#define ioqspi_hw (&mayhem_fake_ioqspi)
#define hw_write_masked(addr, values, mask) \
    (*(addr) = (*(addr) & ~(uint32_t)(mask)) | ((uint32_t)(values) & (uint32_t)(mask)))
