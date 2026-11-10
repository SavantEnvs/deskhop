#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef unsigned int uint;

#define PICO_DEFAULT_LED_PIN 25
#define XIP_BASE 0x10000000u
#define GPIO_OUT 1
#define GPIO_IN  0

extern uint8_t mayhem_fake_ppb[0x20000];
#define PPB_BASE ((uintptr_t)mayhem_fake_ppb)

uint64_t time_us_64(void);
uint32_t time_us_32(void);
void sleep_us(uint64_t us);
void sleep_ms(uint32_t ms);
void gpio_init(uint gpio);
void gpio_set_dir(uint gpio, bool out);
void gpio_put(uint gpio, bool value);
bool gpio_get(uint gpio);
