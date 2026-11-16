/* Host keyboard GPIO/time boundary. SPDX-License-Identifier: EPL-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef unsigned int uint;
#define GPIO_IN false
#define GPIO_OUT true
#define GPIO_FUNC_SIO 5
uint64_t time_us_64(void);
void sleep_ms(uint32_t ms);
void sleep_us(uint64_t us);
void gpio_init(uint pin);
void gpio_set_function(uint pin, uint function);
void gpio_pull_up(uint pin);
void gpio_put(uint pin, bool value);
void gpio_set_dir(uint pin, bool output);
bool gpio_get(uint pin);
