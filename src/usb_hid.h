/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * usb hid host helpers.
 */

#ifndef _USB_HID_H
#define _USB_HID_H

#include <stdbool.h>
#include <stdint.h>

enum usb_gamepad_initialization {
    USB_GAMEPAD_INIT_NONE, USB_GAMEPAD_INIT_PENDING, USB_GAMEPAD_INIT_WAITING,
    USB_GAMEPAD_INIT_READY, USB_GAMEPAD_INIT_FAILED,
};

typedef struct {
    uint16_t reports, length, vid, pid;
    uint8_t slot, expected_length, state, initialization;
    bool decoded, receive_ok;
} usb_hid_gamepad_status_t;

void hid_app_task(void);
void usb_hid_sync_keyboard_leds(void);
// Main-context snapshot of the first mounted gamepad; false if none is present.
bool usb_hid_gamepad_status(usb_hid_gamepad_status_t *status);

#endif // _USB_HID_H
