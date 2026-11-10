/* Host-test SDK boundary. SPDX-License-Identifier: EPL-2.0 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
void watchdog_disable(void);
void watchdog_enable(uint32_t delay_ms, bool pause_on_debug);
void watchdog_update(void);
bool watchdog_caused_reboot(void);
