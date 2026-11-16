/* USB host boundary for the entry-point failure-path test.
 * SPDX-License-Identifier: EPL-2.0 */
#pragma once
#include "class/hid/hid.h"
bool tusb_init(uint8_t rhport, tusb_rhport_init_t const *init);
void tuh_task(void);
