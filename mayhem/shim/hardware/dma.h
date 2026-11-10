#pragma once
#include <stdbool.h>
#include <stdint.h>
bool dma_channel_is_busy(uint32_t channel);
void dma_channel_transfer_from_buffer_now(uint32_t channel, const void *buf, uint32_t count);
typedef struct { uint32_t transfer_count; } dma_channel_hw_t;
extern dma_channel_hw_t mayhem_fake_dma[16];
#define dma_channel_hw_addr(c) (&mayhem_fake_dma[c])
