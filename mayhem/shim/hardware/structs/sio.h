#pragma once
#include <stdint.h>
typedef struct { uint32_t gpio_hi_in; } sio_hw_t;
extern sio_hw_t mayhem_fake_sio;
#define sio_hw (&mayhem_fake_sio)
