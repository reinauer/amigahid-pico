/* Host-test HID boundary, USB HID keyboard usages. SPDX-License-Identifier: EPL-2.0 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint8_t modifier, reserved, keycode[6]; } hid_keyboard_report_t;
typedef struct { uint8_t buttons; int8_t x, y, wheel, pan; } hid_mouse_report_t;
#define HID_KEY_ENTER 0x28
#define HID_KEY_ESCAPE 0x29
#define HID_KEY_F12 0x45
#define HID_KEY_ARROW_RIGHT 0x4f
#define HID_KEY_ARROW_LEFT 0x50
#define HID_KEY_ARROW_DOWN 0x51
#define HID_KEY_ARROW_UP 0x52
#define KEYBOARD_MODIFIER_RIGHTGUI 0x80
