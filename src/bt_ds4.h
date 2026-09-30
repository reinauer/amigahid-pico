/* DualShock 4 Bluetooth input. SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_BT_DS4_H
#define AMIGAHID_BT_DS4_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool touching;
    uint8_t contact;
    uint16_t x, y;
    int16_t remainder_x, remainder_y;
} bt_ds4_t;

typedef struct {
    uint16_t gamepad;
    uint8_t mouse_buttons;
    int8_t mouse_x, mouse_y;
} bt_ds4_report_t;

bool bt_ds4_matches_descriptor(uint8_t const *descriptor, size_t length);
// Reports include the report ID but exclude the Bluetooth HID 0xa1 header.
bool bt_ds4_decode(bt_ds4_t *pad, uint8_t const *report, size_t length,
    bt_ds4_report_t *decoded);

#endif
