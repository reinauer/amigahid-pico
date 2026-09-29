/* USB HID 1.11 short-item parser for digital controls in Game Pad/Joystick
 * application collections. SPDX-License-Identifier: EPL-2.0 */
#include "hid_gamepad.h"

#include <limits.h>
#include <string.h>

#define DESKTOP_PAGE 1u
#define BUTTON_PAGE 9u
#define USAGE(page, id) (((page) << 16) | (id))
#define FIELD_HAT 0x80u
#define FIELD_X 0x81u
#define FIELD_Y 0x82u
#define FIELD_BUTTON4 0x83u
#define MAX_USAGES 32u
#define MAX_DEPTH 8

typedef struct {
    uint32_t page, size, count;
    int32_t minimum, maximum;
    uint8_t id;
} globals_t;

typedef struct {
    uint32_t usages[MAX_USAGES], minimum;
    uint8_t count;
    bool range;
} locals_t;

static int32_t signed_value(uint32_t value, unsigned bits)
{
    if (bits == 0)
        return 0;
    if (bits < 32 && (value & (1u << (bits - 1))))
        value |= UINT32_MAX << bits;
    return (int32_t)value;
}

static int report_index(hid_gamepad_t *pad, uint8_t id)
{
    for (unsigned i = 0; i < pad->report_count; i++)
        if (pad->reports[i].id == id)
            return (int)i;
    if (pad->report_count == GAMEPAD_MAX_REPORTS)
        return -1;
    unsigned i = pad->report_count++;
    pad->reports[i].id = id;
    return (int)i;
}

static uint8_t field_kind(uint32_t usage)
{
    switch (usage) {
        case USAGE(DESKTOP_PAGE, 0x30): return FIELD_X;
        case USAGE(DESKTOP_PAGE, 0x31): return FIELD_Y;
        case USAGE(DESKTOP_PAGE, 0x39): return FIELD_HAT;
        case USAGE(DESKTOP_PAGE, 0x90): return GAMEPAD_UP;
        case USAGE(DESKTOP_PAGE, 0x91): return GAMEPAD_DOWN;
        case USAGE(DESKTOP_PAGE, 0x92): return GAMEPAD_RIGHT;
        case USAGE(DESKTOP_PAGE, 0x93): return GAMEPAD_LEFT;
        case USAGE(BUTTON_PAGE, 1): return GAMEPAD_FIRE;
        case USAGE(BUTTON_PAGE, 2): return GAMEPAD_FIRE2;
        case USAGE(BUTTON_PAGE, 3): return GAMEPAD_FIRE3;
        case USAGE(BUTTON_PAGE, 4): return FIELD_BUTTON4;
        default: return 0;
    }
}

static bool parse(hid_gamepad_t *pad, uint8_t const *descriptor, size_t length)
{
    globals_t global = {0}, stack[MAX_DEPTH];
    locals_t local = {0};
    bool collections[MAX_DEPTH], controller = false;
    unsigned global_depth = 0, depth = 0;
    size_t cursor = 0;
    while (cursor < length) {
        uint8_t prefix = descriptor[cursor++];
        if (prefix == 0xfe) // Long items and alternative usage sets are unsupported.
            return false;
        unsigned size = prefix & 3u;
        if (size == 3)
            size = 4;
        if (size > length - cursor)
            return false;
        uint32_t value = 0;
        for (unsigned byte = 0; byte < size; byte++)
            value |= (uint32_t)descriptor[cursor++] << (8 * byte);
        unsigned type = (prefix >> 2) & 3u, tag = prefix >> 4;
        if (type == 1) {
            switch (tag) {
                case 0:
                    if (value > UINT16_MAX) return false;
                    global.page = value;
                    break;
                case 1: global.minimum = signed_value(value, size * 8); break;
                case 2:
                    if (global.minimum >= 0 && value > INT32_MAX) return false;
                    global.maximum = global.minimum < 0 ? signed_value(value, size * 8) : (int32_t)value;
                    break;
                case 7: global.size = value; break;
                case 8:
                    if (value == 0 || value > UINT8_MAX) return false;
                    global.id = (uint8_t)value;
                    pad->report_ids = true;
                    break;
                case 9: global.count = value; break;
                case 10:
                    if (global_depth == MAX_DEPTH) return false;
                    stack[global_depth++] = global;
                    break;
                case 11:
                    if (!global_depth) return false;
                    global = stack[--global_depth];
                    break;
                default: break;
            }
        } else if (type == 2) {
            uint32_t usage = size == 4 ? value : USAGE(global.page, value);
            switch (tag) {
                case 0:
                    if (local.count == MAX_USAGES) return false;
                    local.usages[local.count++] = usage;
                    break;
                case 1: local.minimum = usage; local.range = true; break;
                case 2:
                    if (!local.range || usage < local.minimum ||
                        usage - local.minimum >= MAX_USAGES - local.count)
                        return false;
                    for (uint32_t i = local.minimum;; i++) {
                        local.usages[local.count++] = i;
                        if (i == usage) break;
                    }
                    local.range = false;
                    break;
                case 10: return false;
                default: break;
            }
        } else if (type == 0) {
            if (local.range)
                return false;
            if (tag == 10) {
                if (depth == MAX_DEPTH) return false;
                collections[depth++] = controller;
                if (value == 1) { // An Application collection starts a new device context.
                    uint32_t usage = local.count ? local.usages[0] : 0;
                    controller = usage == USAGE(DESKTOP_PAGE, 4) || usage == USAGE(DESKTOP_PAGE, 5);
                }
            } else if (tag == 12) {
                if (!depth) return false;
                controller = collections[--depth];
            } else if (tag == 8) { // Input offsets include constants and unrelated controls.
                if (!depth || global.size == 0 || global.size > 32 || global.count == 0 ||
                    global.count > GAMEPAD_MAX_REPORT_BYTES * 8u / global.size)
                    return false;
                int report = report_index(pad, global.id);
                if (report < 0) return false;
                gamepad_report_t *layout = &pad->reports[report];
                uint32_t bits = global.size * global.count;
                if (layout->bits + bits > GAMEPAD_MAX_REPORT_BYTES * 8u)
                    return false;
                if (controller && (value & 7u) == 2u && local.count) {
                    for (uint32_t i = 0; i < global.count; i++) {
                        uint32_t usage = local.usages[i < local.count ? i : local.count - 1u];
                        uint8_t kind = field_kind(usage);
                        if (!kind) continue;
                        int64_t range = (int64_t)global.maximum - global.minimum;
                        if (range < 0 || (kind == FIELD_HAT && range != 3 && range != 7) ||
                            ((kind == FIELD_X || kind == FIELD_Y) && range < 2))
                            continue;
                        if (pad->field_count == GAMEPAD_MAX_FIELDS) return false;
                        pad->fields[pad->field_count++] = (gamepad_field_t) {
                            .offset = (uint16_t)(layout->bits + i * global.size),
                            .size = (uint8_t)global.size,
                            .report = (uint8_t)report,
                            .kind = kind,
                            .minimum = global.minimum,
                            .maximum = global.maximum,
                        };
                    }
                }
                layout->bits += bits;
            }
            // HID local items never carry over to the next Main item.
            memset(&local, 0, sizeof(local));
        }
    }
    if (depth || global_depth || local.range || !pad->field_count)
        return false;
    // Some HID gamepads skip Button 3 (Stadia uses 1=A, 2=B, 4=X).
    // Prefer Button 3 when present; otherwise use Button 4 as the third fire.
    bool has_button3 = false;
    for (unsigned i = 0; i < pad->field_count; i++)
        has_button3 |= pad->fields[i].kind == GAMEPAD_FIRE3;
    unsigned fields = 0;
    for (unsigned i = 0; i < pad->field_count; i++) {
        gamepad_field_t field = pad->fields[i];
        if (field.kind == FIELD_BUTTON4) {
            if (has_button3) continue;
            field.kind = GAMEPAD_FIRE3;
        }
        pad->fields[fields++] = field;
    }
    pad->field_count = (uint8_t)fields;
    for (unsigned i = 0; i < pad->report_count; i++)
        if ((pad->report_ids && !pad->reports[i].id) ||
            (pad->reports[i].bits + 7u) / 8u + (pad->report_ids ? 1u : 0u) > GAMEPAD_MAX_REPORT_BYTES)
            return false;
    return true;
}

bool hid_gamepad_parse(hid_gamepad_t *pad, uint8_t const *descriptor, size_t length)
{
    if (!pad)
        return false;
    memset(pad, 0, sizeof(*pad));
    if (!descriptor || !parse(pad, descriptor, length)) {
        memset(pad, 0, sizeof(*pad));
        return false;
    }
    return true;
}

bool hid_gamepad_decode(hid_gamepad_t *pad, uint8_t const *report, size_t length, uint8_t *state)
{
    if (!pad || !report || !state || !length || !pad->field_count)
        return false;
    uint8_t id = pad->report_ids ? *report++ : 0;
    if (pad->report_ids)
        length--;
    int index = -1;
    for (unsigned i = 0; i < pad->report_count; i++)
        if (pad->reports[i].id == id) index = (int)i;
    if (index < 0 || length < (pad->reports[index].bits + 7u) / 8u)
        return false;
    uint8_t result = 0;
    bool relevant = false;
    for (unsigned i = 0; i < pad->field_count; i++) {
        gamepad_field_t const *field = &pad->fields[i];
        if (field->report != index) continue;
        relevant = true;
        uint32_t bits = 0;
        for (unsigned bit = 0; bit < field->size; bit++) {
            unsigned pos = field->offset + bit;
            bits |= (uint32_t)((report[pos / 8] >> (pos % 8)) & 1u) << bit;
        }
        int64_t value = field->minimum < 0 ? signed_value(bits, field->size) : (int64_t)bits;
        if (value < field->minimum || value > field->maximum)
            continue; // Includes a hat's null (centred) value.
        if (field->kind == FIELD_X || field->kind == FIELD_Y) {
            // Digital sticks can encode their switches as absolute X/Y axes
            // (e.g. Competition Pro: 0, 128, 255). Use the descriptor's range
            // with a central deadzone so centred sticks never hold a direction.
            int64_t position = 4 * (value - field->minimum);
            int64_t range = (int64_t)field->maximum - field->minimum;
            if (position < range)
                result |= field->kind == FIELD_X ? GAMEPAD_LEFT : GAMEPAD_UP;
            else if (position > 3 * range)
                result |= field->kind == FIELD_X ? GAMEPAD_RIGHT : GAMEPAD_DOWN;
        } else if (field->kind == FIELD_HAT) {
            static uint8_t const directions[] = {GAMEPAD_UP, GAMEPAD_UP | GAMEPAD_RIGHT,
                GAMEPAD_RIGHT, GAMEPAD_RIGHT | GAMEPAD_DOWN, GAMEPAD_DOWN,
                GAMEPAD_DOWN | GAMEPAD_LEFT, GAMEPAD_LEFT, GAMEPAD_LEFT | GAMEPAD_UP};
            unsigned hat = (unsigned)(value - field->minimum);
            if (field->maximum - field->minimum == 3)
                hat *= 2;
            result |= directions[hat];
        } else if (value != 0) {
            result |= field->kind;
        }
    }
    if (!relevant)
        return false;
    pad->reports[index].state = result;
    *state = 0;
    for (unsigned i = 0; i < pad->report_count; i++)
        *state |= pad->reports[i].state;
    return true;
}
