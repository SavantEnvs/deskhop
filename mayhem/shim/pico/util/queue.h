/* Host stand-in for the pico-sdk queue: a plain bounded FIFO (non-blocking subset). */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    uint32_t element_size;
    uint32_t element_count;
    uint32_t head;
    uint32_t used;
} queue_t;

void queue_init(queue_t *q, uint element_size, uint element_count);
void queue_free(queue_t *q);
bool queue_try_add(queue_t *q, const void *data);
bool queue_try_remove(queue_t *q, void *data);
bool queue_try_peek(queue_t *q, void *data);
bool queue_is_empty(queue_t *q);
bool queue_is_full(queue_t *q);
