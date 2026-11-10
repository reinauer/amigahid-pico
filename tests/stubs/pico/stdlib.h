/* Host-test SDK boundary. SPDX-License-Identifier: EPL-2.0 */
#pragma once
#include "pico.h"
#define GPIO_IN false
uint64_t time_us_64(void);
void gpio_init(uint pin);
void gpio_set_dir(uint pin, bool output);
void gpio_pull_up(uint pin);
bool gpio_get(uint pin);
