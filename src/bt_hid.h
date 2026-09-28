/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * bluetooth hid host integration for pico w builds.
 */

#ifndef _BT_HID_H
#define _BT_HID_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t reports, length;
    uint8_t slot, expected_length, state;
    bool decoded;
} bt_hid_gamepad_status_t;

void bt_hid_init(void);
void bt_hid_task(void);
// Main-context snapshot of the LE gamepad; false if none is ready.
bool bt_hid_gamepad_status(bt_hid_gamepad_status_t *status);

#endif // _BT_HID_H
