/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * debug console routines.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pico/time.h"

#ifdef ENABLE_BLUETOOTH_HID
#include "hardware/sync.h"
#include "bt_hid.h"
#endif

#include "debug_cons.h"
#include "display/disp_ssd.h"
#include "firmware_version.h"
#include "input_bridge.h"
#include "settings.h"
#include "usb_hid.h"
#include "output.h"

#define DBGCONS_OLED_COLS 21

struct
{
    uint8_t hid_keyboard, hid_mouse, hid_controller;
    uint8_t plug_events, unplug_events;
} debug_counters;

static char gamepad_lines[2][DBGCONS_OLED_COLS + 1];
static bool gamepad_visible;
static uint64_t gamepad_next_update;
static void dbgcons_gamepad_task(void);

#ifdef ENABLE_BLUETOOTH_HID
static bool bt_passkey_active;
static char bt_pending_lines[2][DBGCONS_OLED_COLS + 1];
static bool bt_pending_dirty[2];

static void dbgcons_write_padded_line(uint8_t line, char const *message)
{
    char linebuf[DBGCONS_OLED_COLS + 1] = "";

    snprintf(linebuf, sizeof(linebuf), "%-*.*s", DBGCONS_OLED_COLS, DBGCONS_OLED_COLS,
        message != NULL ? message : "");

    // CYW43 can call this from its background IRQ. UGUI and the display
    // allocator must only run in core0's main context.
    uint8_t slot = line == 1 ? 0 : 1;
    uint32_t irq_state = save_and_disable_interrupts();
    memcpy(bt_pending_lines[slot], linebuf, sizeof(linebuf));
    bt_pending_dirty[slot] = true;
    restore_interrupts(irq_state);

    // Preserve startup-stage messages before the main loop starts.
    if (!__get_current_exception())
        dbgcons_task();
}
#endif

void dbgcons_task(void)
{
#ifdef ENABLE_BLUETOOTH_HID
    for (uint8_t slot = 0; slot < 2; slot++) {
        char linebuf[DBGCONS_OLED_COLS + 1];
        uint32_t irq_state = save_and_disable_interrupts();
        bool dirty = bt_pending_dirty[slot];
        if (dirty) {
            memcpy(linebuf, bt_pending_lines[slot], sizeof(linebuf));
            bt_pending_dirty[slot] = false;
        }
        restore_interrupts(irq_state);
        if (dirty) {
            disp_write(0, slot == 0 ? 1 : 3, linebuf);
            if (slot == 1)
                gamepad_lines[1][0] = '\0';
        }
    }
#endif
    dbgcons_gamepad_task();
}

static void dbgcons_gamepad_line(unsigned row, char const *message)
{
    char line[DBGCONS_OLED_COLS + 1];
    snprintf(line, sizeof(line), "%-*.*s", DBGCONS_OLED_COLS, DBGCONS_OLED_COLS, message);
    if (strcmp(line, gamepad_lines[row]) != 0) {
        memcpy(gamepad_lines[row], line, sizeof(line));
        disp_write(0, row + 2, line);
    }
}

static void dbgcons_gamepad_output(uint8_t slot, bool decoded, uint16_t state)
{
#ifdef ENABLE_BLUETOOTH_HID
    if (bt_passkey_active)
        return;
#endif
    input_bridge_gamepad_status_t output = input_bridge_gamepad_status(slot);
    char input[4] = "---", line[32];
    if (decoded)
        snprintf(input, sizeof(input), "%03x", state & 0x7ffu);
    snprintf(line, sizeof(line), "in%s out%03x p%u%s", input, output.state,
        output.ports, output.waiting ? " wait" : "");
    dbgcons_gamepad_line(1, line);
}

static void dbgcons_gamepad_task(void)
{
    if (settings_get()->display != SETTINGS_DISPLAY_HID)
        return;
    uint64_t now = time_us_64();
    if (now < gamepad_next_update)
        return;
    gamepad_next_update = now + 100000;

    usb_hid_gamepad_status_t usb;
    if (!usb_hid_gamepad_status(&usb)) {
#ifdef ENABLE_BLUETOOTH_HID
        bt_hid_gamepad_status_t bt;
        if (bt_hid_gamepad_status(&bt)) {
            char line[32];
            gamepad_visible = true;
            snprintf(line, sizeof(line), "b%04x l%02u/%02u d%c", bt.reports,
                bt.length, bt.expected_length, bt.decoded ? '+' : '-');
            dbgcons_gamepad_line(0, line);
            dbgcons_gamepad_output(bt.slot, bt.decoded, bt.state);
            return;
        }
#endif
        if (gamepad_visible) {
            // Preserve a newer USB connect/disconnect message, which clears
            // this cache. Otherwise remove stale Bluetooth diagnostics.
            if (gamepad_lines[0][0])
                dbgcons_gamepad_line(0, "");
#ifdef ENABLE_BLUETOOTH_HID
            if (!bt_passkey_active)
#endif
                dbgcons_gamepad_line(1, "");
            gamepad_visible = false;
        }
        return;
    }
    gamepad_visible = true;
    char line[32];
    snprintf(line, sizeof(line), "j%04x l%02u/%02u d%c r%c", usb.reports,
        usb.length, usb.expected_length, usb.decoded ? '+' : '-', usb.receive_ok ? '+' : '-');
    dbgcons_gamepad_line(0, line);
#ifdef ENABLE_BLUETOOTH_HID
    if (bt_passkey_active)
        return;
#endif
    if (usb.reports == 0 && usb.length == 0) {
        char const initialization[] = "-p?+!";
        snprintf(line, sizeof(line), "usb %04x:%04x s%c", usb.vid, usb.pid,
            usb.initialization < sizeof(initialization) - 1 ? initialization[usb.initialization] : '!');
        dbgcons_gamepad_line(1, line);
        return;
    }
    dbgcons_gamepad_output(usb.slot, usb.decoded, usb.state);
}

void dbgcons_init()
{
    ahprintf(
        VT_ED_CLS "amigahid-pico " AMIGAHID_FIRMWARE_VERSION
        " by nine <nine@aphlor.org>, https://github.com/borb/amigahid-pico"
    );

    disp_ssd_version(AMIGAHID_FIRMWARE_VERSION);

    debug_counters.hid_keyboard = 0;
    debug_counters.hid_mouse = 0;
    debug_counters.hid_controller = 0;
    debug_counters.plug_events = 0;
    debug_counters.unplug_events = 0;

    dbgcons_print_counters();
#ifdef ENABLE_BLUETOOTH_HID
    dbgcons_bt_status("bt off");
#endif
#ifdef DEBUG_HID_STATUS
    disp_write(0, 2, "hid --");
#endif
#ifdef ENABLE_BLUETOOTH_HID
    dbgcons_bt_passkey_clear();
#elif defined(DEBUG_MOUSE)
    disp_write(0, 3, "mouse --");
#endif
}

void dbgcons_print_counters()
{
    char linebuf[32] = "";

    ahprintf(
        VT_CUP_POS VT_EL_LIN
        "[system] key: %02x mouse: %02x joy: %02x total plug: %02x total unplug: %02x\n",
        3, 1,
        debug_counters.hid_keyboard,
        debug_counters.hid_mouse,
        debug_counters.hid_controller,
        debug_counters.plug_events,
        debug_counters.unplug_events
    );

    sprintf(
        linebuf,
        "usb    k:%02x m:%02x j:%02x",
        debug_counters.hid_keyboard,
        debug_counters.hid_mouse,
        debug_counters.hid_controller
    );

    disp_write(0, 0, linebuf);
}

void dbgcons_plug(enum debug_plug_types devtype)
{
    switch (devtype) {
        case AP_H_KEYBOARD:
            debug_counters.hid_keyboard++;
            break;
        case AP_H_MOUSE:
            debug_counters.hid_mouse++;
            break;
        case AP_H_CONTROLLER:
            debug_counters.hid_controller++;
            break;
        case AP_H_UNKNOWN:
        default:
            break;
    }
    debug_counters.plug_events++;
    dbgcons_print_counters();
}

void dbgcons_unplug(enum debug_plug_types devtype)
{
    switch (devtype) {
        case AP_H_KEYBOARD:
            debug_counters.hid_keyboard--;
            break;
        case AP_H_MOUSE:
            debug_counters.hid_mouse--;
            break;
        case AP_H_CONTROLLER:
            debug_counters.hid_controller--;
            break;
        case AP_H_UNKNOWN:
        default:
            break;
    }
    debug_counters.unplug_events++;
    dbgcons_print_counters();
}

void dbgcons_amiga_key(uint8_t incode, uint8_t outcode, char *updown)
{
#ifdef DEBUG_KEYBOARD
    char linebuf[32] = "";

    ahprintf(
        VT_CUP_POS VT_EL_LIN
        "[amigak] hid in: %02x amiga out: %02x up/down: %s\n",
        4, 1,
        incode, outcode, updown
    );

    sprintf(
        linebuf,
        "amikb hid:%02x ami:%02x %s",
        incode, outcode, updown
    );

#ifndef ENABLE_BLUETOOTH_HID
    disp_write(0, 1, linebuf);
#endif
#else
    (void)incode;
    (void)outcode;
    (void)updown;
#endif
}

void dbgcons_amiga_mod(uint8_t outcode, char updown)
{
    // ls rs cl ct la ra lam ram
}

void dbgcons_hid_status(uint8_t dev_addr, uint8_t instance, uint8_t hid_protocol, bool receive_ok, uint8_t report_count, bool mounted)
{
#ifdef DEBUG_HID_STATUS
    ahprintf("[hid] %s addr:%02x inst:%02x proto:%02x reports:%u rx:%s\n",
        mounted ? "mount" : "umount", dev_addr, instance, hid_protocol, report_count, receive_ok ? "ok" : "fail");
#endif
    if (settings_get()->display != SETTINGS_DISPLAY_HID)
        return;
    char linebuf[22];
    snprintf(linebuf, sizeof(linebuf), "hid%c a%02x i%02x p%02x %s",
        mounted ? '+' : '-', dev_addr, instance, hid_protocol, receive_ok ? "ok" : "err");
    disp_write(0, 2, "                     ");
    disp_write(0, 2, linebuf);
    gamepad_lines[0][0] = '\0';
}

void dbgcons_mouse_report(int16_t x, int16_t y, uint8_t buttons)
{
#ifdef DEBUG_MOUSE
    ahprintf("[mouse] x:%d y:%d buttons:%02x\n", x, y, buttons);
#endif
    if (settings_get()->display != SETTINGS_DISPLAY_MOUSE)
        return;
#ifdef ENABLE_BLUETOOTH_HID
    if (bt_passkey_active)
        return;
#endif
    // Diagnostics should not enqueue a display frame for every HID packet.
    static uint64_t next_update;
    uint64_t now = time_us_64();
    if (now < next_update)
        return;
    next_update = now + 100000;
    char linebuf[32];
    snprintf(linebuf, sizeof(linebuf), "m x%+04d y%+04d b%02x", x, y, buttons);
    disp_write(0, 3, linebuf);
}

void dbgcons_settings_changed(void)
{
    memset(gamepad_lines, 0, sizeof(gamepad_lines));
    gamepad_visible = false;
    gamepad_next_update = 0;
    disp_write(0, 2, "                     ");
    disp_write(0, 3, "                     ");
#ifdef ENABLE_BLUETOOTH_HID
    uint32_t irq_state = save_and_disable_interrupts();
    bt_pending_dirty[0] = true;
    bt_pending_dirty[1] = true;
    restore_interrupts(irq_state);
    dbgcons_task();
#endif
}

void dbgcons_mouse_wheel(int8_t wheel)
{
#ifdef DEBUG_MOUSE
    char linebuf[32] = "";

    snprintf(linebuf, sizeof(linebuf), "wheel %+d", wheel);

    ahprintf(
        VT_CUP_POS VT_EL_LIN
        "[mouse] wheel:%d\n",
        7, 1,
        wheel
    );

#ifdef ENABLE_BLUETOOTH_HID
    if (!bt_passkey_active)
        disp_write(0, 4, linebuf);
#else
    disp_write(0, 4, linebuf);
#endif
#else
    (void)wheel;
#endif
}

void dbgcons_tankmouse_status(uint16_t queued, uint16_t requests, uint16_t responses)
{
#ifdef DEBUG_MOUSE
    char linebuf[32] = "";

    snprintf(
        linebuf,
        sizeof(linebuf),
        "tm q%u rq%u rs%u",
        queued,
        requests,
        responses
    );

    ahprintf(
        VT_CUP_POS VT_EL_LIN
        "[tankmouse] queued:%u requests:%u responses:%u\n",
        8, 1,
        queued,
        requests,
        responses
    );

#ifdef ENABLE_BLUETOOTH_HID
    if (!bt_passkey_active)
        disp_write(0, 5, linebuf);
#else
    disp_write(0, 5, linebuf);
#endif
#else
    (void)queued;
    (void)requests;
    (void)responses;
#endif
}

void dbgcons_bt_status(char const *status)
{
#ifdef ENABLE_BLUETOOTH_HID
    dbgcons_write_padded_line(1, status);
#else
    (void)status;
#endif
}

void dbgcons_bt_passkey(char const *message)
{
#ifdef ENABLE_BLUETOOTH_HID
    bt_passkey_active = true;
    dbgcons_write_padded_line(3, message);
#else
    (void)message;
#endif
}

void dbgcons_bt_passkey_clear(void)
{
#ifdef ENABLE_BLUETOOTH_HID
    bt_passkey_active = false;
#ifdef DEBUG_MOUSE
    dbgcons_write_padded_line(3, "mouse --");
#else
    dbgcons_write_padded_line(3, "");
#endif
#endif
}
