/* DualShock 4 Bluetooth input. SPDX-License-Identifier: EPL-2.0
 * Wire format references: hid-tools hidtools/device/sony_gamepad.py and
 * Bluepad32 src/components/bluepad32/parser/uni_hid_parser_ds4.c.
 */
#include "bt_ds4.h"
#include "hid_gamepad.h"
#include "gamepad_config.h"
#include "settings.h"

#include <string.h>

bool bt_ds4_matches_descriptor(uint8_t const *descriptor, size_t length)
{
    // Match the DS4's complete simple input layout and calibration feature,
    // plus its vendor-defined 78-byte Bluetooth report. A device name alone
    // is insufficient: DualSense also advertises as Wireless Controller.
    static const uint8_t simple[] = {
        0x05,0x01,0x09,0x05,0xa1,0x01,0x85,0x01,0x09,0x30,0x09,0x31,
        0x09,0x32,0x09,0x35,0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,
        0x04,0x81,0x02,0x09,0x39,0x15,0x00,0x25,0x07,0x75,0x04,0x95,
        0x01,0x81,0x42,0x05,0x09,0x19,0x01,0x29,0x0e,0x15,0x00,0x25,
        0x01,0x75,0x01,0x95,0x0e,0x81,0x02,0x75,0x06,0x95,0x01,0x81,
        0x01,0x05,0x01,0x09,0x33,0x09,0x34,0x15,0x00,0x26,0xff,0x00,
        0x75,0x08,0x95,0x02,0x81,0x02,0x06,0x04,0xff,0x85,0x02,0x09,
        0x24,0x95,0x24,0xb1,0x02,
    };
    static const uint8_t extended[] = {
        0x06,0x00,0xff,0x85,0x11,0x09,0x20,0x15,0x00,0x26,0xff,0x00,
        0x75,0x08,0x95,0x4d,0x81,0x02,
    };
    return descriptor && length > 180 + sizeof(extended) &&
        descriptor[length - 1] == 0xc0 &&
        !memcmp(descriptor, simple, sizeof(simple)) &&
        !memcmp(descriptor + 180, extended, sizeof(extended));
}

static uint32_t crc_byte(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; bit++)
        crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0);
    return crc;
}

static bool valid_extended_report(uint8_t const *report)
{
    uint32_t crc = crc_byte(UINT32_MAX, 0xa1);
    for (unsigned i = 0; i < 74; i++)
        crc = crc_byte(crc, report[i]);
    uint32_t received = (uint32_t)report[74] | (uint32_t)report[75] << 8 |
        (uint32_t)report[76] << 16 | (uint32_t)report[77] << 24;
    return ~crc == received && report[35] <= 4;
}

static int8_t mouse_delta(int value, int16_t *remainder)
{
    // Four touchpad units per mouse count; keep slow sub-count movement.
    value += *remainder;
    int delta = value / 4;
    *remainder = (int16_t)(value - delta * 4);
    if (delta < -127) delta = -127;
    if (delta > 127) delta = 127;
    return (int8_t)delta;
}

static void touchpad_decode(bt_ds4_t *pad, uint8_t const *report, bt_ds4_report_t *decoded)
{
    // Count zero means no new touch sample. Button state is still current.
    if (!report[35])
        return;
    uint8_t const *point = report + 37; // first (newest) touch sample
    uint8_t const *second = point + 4;
    // Follow the same contact if the controller changes the two point slots.
    if (!(second[0] & 0x80) && ((point[0] & 0x80) ||
        (pad->touching && (second[0] & 0x7f) == pad->contact)))
        point = second;
    uint16_t x = point[1] | (uint16_t)(point[2] & 0x0f) << 8;
    uint16_t y = (point[2] >> 4) | (uint16_t)point[3] << 4;
    if ((point[0] & 0x80) || x >= 1920 || y >= 943) {
        memset(pad, 0, sizeof(*pad));
        return;
    }
    uint8_t contact = point[0] & 0x7f;
    if (pad->touching && pad->contact == contact) {
        decoded->mouse_x = mouse_delta((int)x - pad->x, &pad->remainder_x);
        decoded->mouse_y = mouse_delta((int)y - pad->y, &pad->remainder_y);
    } else {
        pad->remainder_x = pad->remainder_y = 0;
    }
    pad->touching = true;
    pad->contact = contact;
    pad->x = x;
    pad->y = y;
}

bool bt_ds4_decode(bt_ds4_t *pad, uint8_t const *report, size_t length,
    bt_ds4_report_t *decoded)
{
    if (!pad || !report || !decoded)
        return false;
    size_t offset;
    if (length == 10 && report[0] == 0x01) {
        offset = 1;
    } else if (length == 78 && report[0] == 0x11 && valid_extended_report(report)) {
        offset = 3;
    } else {
        return false;
    }
    memset(decoded, 0, sizeof(*decoded));
    static const uint8_t hats[] = {
        GAMEPAD_UP, GAMEPAD_UP | GAMEPAD_RIGHT, GAMEPAD_RIGHT,
        GAMEPAD_RIGHT | GAMEPAD_DOWN, GAMEPAD_DOWN,
        GAMEPAD_DOWN | GAMEPAD_LEFT, GAMEPAD_LEFT, GAMEPAD_LEFT | GAMEPAD_UP,
    };
    uint8_t hat = report[offset + 4] & 0x0f;
    uint8_t dpad = hat < sizeof(hats) ? hats[hat] : 0;
    uint8_t stick = gamepad_axis(report[offset], 0, 255, GAMEPAD_LEFT, GAMEPAD_RIGHT) |
        gamepad_axis(report[offset + 1], 0, 255, GAMEPAD_UP, GAMEPAD_DOWN);
    uint16_t buttons = (report[offset + 4] >> 4) | (uint16_t)report[offset + 5] << 4;
    static uint8_t const defaults[] = {2, 3, 1, 4, 5, 6, 10};
    decoded->gamepad = gamepad_map(dpad, stick, buttons, defaults);
    if (settings_get()->ds4_mouse == 2) {
        memset(pad, 0, sizeof(*pad));
        return true;
    }
    if (report[offset + 6] & 0x02) decoded->mouse_buttons = 1; // pad click
    // HID mouse mask: left=1, right=2, middle=4. Keep these independent of
    // the three joystick buttons, including simultaneous presses and releases.
    if (settings_get()->ds4_mouse == 1 && (report[offset + 5] & 0x01)) decoded->mouse_buttons |= 1; // L1
    if (settings_get()->ds4_mouse == 1 && (report[offset + 5] & 0x02)) decoded->mouse_buttons |= 2; // R1
    if (settings_get()->ds4_mouse == 1 && (report[offset + 5] & 0x0c)) decoded->mouse_buttons |= 4; // L2 or R2
    if (offset == 3)
        touchpad_decode(pad, report, decoded);
    else
        memset(pad, 0, sizeof(*pad));
    return true;
}
