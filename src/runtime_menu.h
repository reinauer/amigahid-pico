/* Local configuration UI. SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_RUNTIME_MENU_H
#define AMIGAHID_RUNTIME_MENU_H

#include "input_bridge.h"

void runtime_menu_init(void);
void runtime_menu_task(void);
bool runtime_menu_keyboard(uint8_t slot, hid_keyboard_report_t const *report);
void runtime_menu_disconnect(uint8_t slot);
void runtime_menu_filter_keyboard(hid_keyboard_report_t *report);
void runtime_menu_watchdog_task(void);

#endif
