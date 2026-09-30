/* SPDX-License-Identifier: EPL-2.0 */
#include "keyboard_map.h"
#include "settings.h"
#include "platform/amiga/keyboard.h"

uint8_t keyboard_map_key(uint8_t hid)
{
    settings_t const *settings = settings_get();
    for (unsigned i = 0; i < 8; i++)
        if (hid && settings->keymap[i].source == hid) return settings->keymap[i].target;
    static uint8_t const help[] = {HID_KEY_INSERT, HID_KEY_HOME, HID_KEY_PAGE_UP, HID_KEY_F11, 0};
    static uint8_t const del[] = {HID_KEY_DELETE, HID_KEY_BACKSPACE, 0};
    if (hid && hid == help[settings->keyboard_help]) return AMIGA_HELP;
    if (hid && hid == del[settings->keyboard_delete]) return AMIGA_DELETE;
    if (hid == HID_KEY_INSERT || hid == HID_KEY_DELETE) return AMIGA_UNKNOWN;
    if (settings->keyboard_iso && hid == HID_KEY_EUROPE_2) return AMIGA_INTLSHIFT;
    if (settings->keyboard_layout == 1) {
        if (hid == HID_KEY_Y) return AMIGA_Z;
        if (hid == HID_KEY_Z) return AMIGA_Y;
    } else if (settings->keyboard_layout == 2) {
        if (hid == HID_KEY_A) return AMIGA_Q;
        if (hid == HID_KEY_Q) return AMIGA_A;
        if (hid == HID_KEY_W) return AMIGA_Z;
        if (hid == HID_KEY_Z) return AMIGA_W;
    }
    return mapHidToAmiga[hid];
}

uint8_t keyboard_map_modifier(unsigned bit)
{
    static uint8_t const modifiers[] = {AMIGA_CTRL, AMIGA_LSHIFT, AMIGA_LALT, AMIGA_LAMIGA,
        AMIGA_CTRL, AMIGA_RSHIFT, AMIGA_RALT, AMIGA_RAMIGA};
    return bit < 8 ? modifiers[bit] : AMIGA_UNKNOWN;
}

char const *keyboard_map_name(uint8_t amiga)
{
    static char const *const names[0x68] = {
        [0x00] = "`",
        [0x01] = "1",
        [0x02] = "2",
        [0x03] = "3",
        [0x04] = "4",
        [0x05] = "5",
        [0x06] = "6",
        [0x07] = "7",
        [0x08] = "8",
        [0x09] = "9",
        [0x0a] = "0",
        [0x0b] = "-",
        [0x0c] = "=",
        [0x0d] = "Backslash",
        [0x0f] = "KPZERO",
        [0x10] = "Q",
        [0x11] = "W",
        [0x12] = "E",
        [0x13] = "R",
        [0x14] = "T",
        [0x15] = "Y",
        [0x16] = "U",
        [0x17] = "I",
        [0x18] = "O",
        [0x19] = "P",
        [0x1a] = "[",
        [0x1b] = "]",
        [0x1d] = "KPONE",
        [0x1e] = "KPTWO",
        [0x1f] = "KPTHREE",
        [0x20] = "A",
        [0x21] = "S",
        [0x22] = "D",
        [0x23] = "F",
        [0x24] = "G",
        [0x25] = "H",
        [0x26] = "J",
        [0x27] = "K",
        [0x28] = "L",
        [0x29] = ";",
        [0x2a] = "Quote",
        [0x2b] = "ISO Return key",
        [0x2d] = "KPFOUR",
        [0x2e] = "KPFIVE",
        [0x2f] = "KPSIX",
        [0x30] = "ISO Shift key",
        [0x31] = "Z",
        [0x32] = "X",
        [0x33] = "C",
        [0x34] = "V",
        [0x35] = "B",
        [0x36] = "N",
        [0x37] = "M",
        [0x38] = ",",
        [0x39] = ".",
        [0x3a] = "/",
        [0x3c] = "KPPERIOD",
        [0x3d] = "KPSEVEN",
        [0x3e] = "KPEIGHT",
        [0x3f] = "KPNINE",
        [0x40] = "Space",
        [0x41] = "Backspace",
        [0x42] = "Tab",
        [0x43] = "KPENTER",
        [0x44] = "Return",
        [0x45] = "Esc",
        [0x46] = "Delete",
        [0x4a] = "KPDASH",
        [0x4c] = "Up",
        [0x4d] = "Down",
        [0x4e] = "Right",
        [0x4f] = "Left",
        [0x50] = "F1",
        [0x51] = "F2",
        [0x52] = "F3",
        [0x53] = "F4",
        [0x54] = "F5",
        [0x55] = "F6",
        [0x56] = "F7",
        [0x57] = "F8",
        [0x58] = "F9",
        [0x59] = "F10",
        [0x5a] = "KPOPAREN",
        [0x5b] = "KPCPAREN",
        [0x5c] = "KPSLASH",
        [0x5d] = "KPAST",
        [0x5e] = "KPPLUS",
        [0x5f] = "Help",
        [0x60] = "Left Shift",
        [0x61] = "Right Shift",
        [0x62] = "Caps Lock",
        [0x63] = "Ctrl",
        [0x64] = "Left Alt",
        [0x65] = "Right Alt",
        [0x66] = "Left Amiga",
        [0x67] = "Right Amiga",
    };
    if (amiga == 0xff) return "Disabled";
    return amiga < 0x68 && names[amiga] ? names[amiga] : "Unused code";
}

char const *keyboard_map_source_name(uint8_t hid)
{
    if (!hid) return "None";
    switch (hid) {
        case HID_KEY_INSERT: return "Insert";
        case HID_KEY_HOME: return "Home";
        case HID_KEY_PAGE_UP: return "Page Up";
        case HID_KEY_PAGE_DOWN: return "Page Down";
        case HID_KEY_END: return "End";
        case HID_KEY_F11: return "F11";
        case HID_KEY_F12: return "F12";
        default: return keyboard_map_name(mapHidToAmiga[hid]);
    }
}
