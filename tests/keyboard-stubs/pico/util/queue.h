/* Single-core host queue boundary. SPDX-License-Identifier: EPL-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    unsigned char data[1024];
    size_t size, capacity, count, head;
} queue_t;
void queue_init(queue_t *q, unsigned size, unsigned capacity);
bool queue_try_add(queue_t *q, const void *item);
bool queue_try_peek(queue_t *q, void *item);
bool queue_try_remove(queue_t *q, void *item);
