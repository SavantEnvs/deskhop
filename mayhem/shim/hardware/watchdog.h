#pragma once
#include <stdint.h>
typedef struct { uint32_t scratch[8]; } watchdog_hw_t;
extern watchdog_hw_t mayhem_fake_watchdog;
#define watchdog_hw (&mayhem_fake_watchdog)
void watchdog_update(void);
