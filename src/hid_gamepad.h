/* Bounded HID gamepad decoding. SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_HID_GAMEPAD_H
#define AMIGAHID_HID_GAMEPAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum gamepad_state {
    GAMEPAD_UP = 1u << 0,
    GAMEPAD_DOWN = 1u << 1,
    GAMEPAD_LEFT = 1u << 2,
    GAMEPAD_RIGHT = 1u << 3,
    GAMEPAD_FIRE = 1u << 4,
    GAMEPAD_FIRE2 = 1u << 5,
    GAMEPAD_FIRE3 = 1u << 6,
};

#define GAMEPAD_MAX_REPORTS 4
#define GAMEPAD_MAX_FIELDS 16
#define GAMEPAD_MAX_REPORT_BYTES 64

typedef struct {
    uint16_t offset;
    uint8_t size, report, kind;
    int32_t minimum, maximum;
} gamepad_field_t;

typedef struct {
    uint16_t bits;
    uint8_t id, state;
} gamepad_report_t;

typedef struct {
    gamepad_report_t reports[GAMEPAD_MAX_REPORTS];
    gamepad_field_t fields[GAMEPAD_MAX_FIELDS];
    uint8_t report_count, field_count;
    bool report_ids;
} hid_gamepad_t;

bool hid_gamepad_parse(hid_gamepad_t *pad, uint8_t const *descriptor, size_t length);
bool hid_gamepad_decode(hid_gamepad_t *pad, uint8_t const *report, size_t length, uint8_t *state);

#endif
