/* Local configuration UI. SPDX-License-Identifier: EPL-2.0 */
#include "runtime_menu.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

#include "display/disp_ssd.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "platform/amiga/quad_mouse.h"
#include "settings.h"
#ifdef ENABLE_BLUETOOTH_HID
#include "bt_hid.h"
#endif
#include "util/debug_cons.h"
#include "util/output.h"

#define MENU_HOLD_US 1000000u
#define RECOVERY_HOLD_US 2000000u
#define BOOT_MENU_US 10000000u

enum menu_item { MENU_KEY, MENU_ENTRY, MENU_RIGHT_GUI, MENU_WHEEL, MENU_REVERSE,
    MENU_SPEED, MENU_DISPLAY, MENU_WATCHDOG, MENU_PORT,
#ifdef HAS_JOYSTICK_PORT2
    MENU_PORT2,
#endif
#ifdef ENABLE_BLUETOOTH_HID
    MENU_BT, MENU_BT_POLICY, MENU_BT_PAIR, MENU_BT_FORGET,
#endif
    MENU_SAVE, MENU_DEFAULTS, MENU_CANCEL, MENU_ITEM_COUNT };

static hid_keyboard_report_t keyboards[INPUT_BRIDGE_MAX_SLOTS];
static bool quarantine[INPUT_BRIDGE_MAX_SLOTS];
static settings_t edited;
static bool opened, redraw, save_pending;
#ifdef ENABLE_BLUETOOTH_HID
static bool pair_pending, forget_pending, forget_confirm;
#endif
static bool holding, recovering;
static uint64_t hold_started, recovery_started, boot_deadline;
static unsigned item;
static uint32_t watchdog_timeout;
static char const *notice;

static bool key_down(hid_keyboard_report_t const *report, uint8_t key)
{
    for (unsigned i = 0; i < 6; i++)
        if (report->keycode[i] == key)
            return true;
    return false;
}

static bool report_empty(hid_keyboard_report_t const *report)
{
    if (report->modifier)
        return false;
    for (unsigned i = 0; i < 6; i++)
        if (report->keycode[i])
            return false;
    return true;
}

static void apply_settings(void)
{
    settings_t const *settings = settings_get();
    input_bridge_set_port_modes(settings->port_mode == SETTINGS_PORT_JOYSTICK, settings->joystick_port2 != 0);
    amiga_quad_mouse_configure(settings_mouse_interval_us(), settings->wheel_enabled, settings->wheel_reverse);
    dbgcons_settings_changed();
    disp_ssd_set_enabled(settings->display != SETTINGS_DISPLAY_OFF);
    uint32_t timeout = settings_watchdog_ms();
    if (timeout != watchdog_timeout) {
        watchdog_disable();
        watchdog_timeout = timeout;
        if (timeout)
            watchdog_enable(timeout, true);
    }
}

static void open_menu(bool recovery)
{
#ifdef ENABLE_BLUETOOTH_HID
    pair_pending = forget_pending = forget_confirm = false;
#endif
    edited = *settings_get();
    if (recovery) {
        settings_defaults(&edited);
        settings_set(&edited);
        apply_settings();
    }
    opened = true;
    item = 0;
    notice = recovery ? "Defaults (unsaved)" : NULL;
    redraw = true;
    holding = recovering = false;
    input_bridge_capture(true);
    disp_ssd_set_overlay(true);
}

static void close_menu(void)
{
    opened = false;
    holding = recovering = false;
    save_pending = false;
    for (unsigned slot = 0; slot < INPUT_BRIDGE_MAX_SLOTS; slot++)
        quarantine[slot] = !report_empty(&keyboards[slot]);
    input_bridge_capture(false);
    disp_ssd_set_overlay(false);
}

static void render_menu(void)
{
    static char const *const names[] = {"Menu key", "Menu entry", "Right GUI key", "Mouse wheel",
        "Wheel direction", "Mouse step interval", "Display", "Watchdog", "Controller port 1",
#ifdef HAS_JOYSTICK_PORT2
        "Controller port 2",
#endif
#ifdef ENABLE_BLUETOOTH_HID
        "Bluetooth", "BT pairing policy", "Pair for 2 minutes", "Forget BT devices",
#endif
        "Save and exit", "Factory defaults", "Cancel changes"};
    static char const *const keys[] = {"F12", "F11", "Application/Menu"};
    static char const *const entry[] = {"Hold for 1 second", "Boot only"};
    static char const *const gui[] = {"Right Amiga", "Hold to open menu", "Disabled"};
    static char const *const speed[] = {"300 us (default)", "200 us", "150 us", "100 us"};
    static char const *const display[] = {"Status", "HID diagnostics", "Mouse diagnostics", "Off"};
    static char const *const watchdog[] = {"Off (default)", "2 seconds", "5 seconds"};
    char heading[22];
    char const *value = "Enter to select";
    switch (item) {
        case MENU_KEY: value = keys[edited.menu_key]; break;
        case MENU_ENTRY: value = entry[edited.menu_entry]; break;
        case MENU_RIGHT_GUI: value = gui[edited.right_gui]; break;
        case MENU_WHEEL: value = edited.wheel_enabled ? "TankMouse/Cocolino" : "Off"; break;
        case MENU_REVERSE: value = edited.wheel_reverse ? "Reverse vertical" : "Normal"; break;
        case MENU_SPEED: value = speed[edited.mouse_speed]; break;
        case MENU_DISPLAY: value = display[edited.display]; break;
        case MENU_WATCHDOG: value = watchdog[edited.watchdog]; break;
        case MENU_PORT: value = edited.port_mode == SETTINGS_PORT_MOUSE ? "Mouse" : "Joystick"; break;
#ifdef HAS_JOYSTICK_PORT2
        case MENU_PORT2: value = edited.joystick_port2 ? "Joystick" : "Off"; break;
#endif
#ifdef ENABLE_BLUETOOTH_HID
        case MENU_BT: value = edited.bluetooth_enabled ? "On" : "Off"; break;
        case MENU_BT_POLICY: value = edited.bluetooth_pairing ? "Paired devices only" : "Automatic (default)"; break;
#endif
        default: break;
    }
    snprintf(heading, sizeof(heading), "Settings %u/%u", item + 1, MENU_ITEM_COUNT);
    disp_ssd_menu(heading, names[item], value, notice ? notice : "Arrows Enter Esc");
    redraw = false;
}

static void change_value(int direction)
{
    uint8_t *value;
    unsigned count;
    switch (item) {
        case MENU_KEY: value = &edited.menu_key; count = SETTINGS_MENU_KEY_COUNT; break;
        case MENU_ENTRY: value = &edited.menu_entry; count = SETTINGS_MENU_ENTRY_COUNT; break;
        case MENU_RIGHT_GUI: value = &edited.right_gui; count = SETTINGS_GUI_COUNT; break;
        case MENU_WHEEL: value = &edited.wheel_enabled; count = 2; break;
        case MENU_REVERSE: value = &edited.wheel_reverse; count = 2; break;
        case MENU_SPEED: value = &edited.mouse_speed; count = 4; break;
        case MENU_DISPLAY: value = &edited.display; count = SETTINGS_DISPLAY_COUNT; break;
        case MENU_WATCHDOG: value = &edited.watchdog; count = 3; break;
        case MENU_PORT: value = &edited.port_mode; count = SETTINGS_PORT_COUNT; break;
#ifdef HAS_JOYSTICK_PORT2
        case MENU_PORT2: value = &edited.joystick_port2; count = 2; break;
#endif
#ifdef ENABLE_BLUETOOTH_HID
        case MENU_BT: value = &edited.bluetooth_enabled; count = 2; break;
        case MENU_BT_POLICY: value = &edited.bluetooth_pairing; count = 2; break;
#endif
        default: return;
    }
    *value = (uint8_t)((*value + count + direction) % count);
}

static void menu_key(uint8_t key)
{
    notice = NULL;
#ifdef ENABLE_BLUETOOTH_HID
    if (key != HID_KEY_ENTER) forget_confirm = false;
#endif
    switch (key) {
        case HID_KEY_ARROW_UP: item = (item + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT; break;
        case HID_KEY_ARROW_DOWN: item = (item + 1) % MENU_ITEM_COUNT; break;
        case HID_KEY_ARROW_LEFT: change_value(-1); break;
        case HID_KEY_ARROW_RIGHT: change_value(1); break;
        case HID_KEY_ENTER:
            if (item == MENU_SAVE)
                save_pending = true;
#ifdef ENABLE_BLUETOOTH_HID
            else if (item == MENU_BT_PAIR) {
                pair_pending = true;
            } else if (item == MENU_BT_FORGET) {
                if (forget_confirm) { forget_pending = true; forget_confirm = false; }
                else { forget_confirm = true; notice = "Enter again to forget"; }
            }
#endif
            else if (item == MENU_DEFAULTS) {
                settings_defaults(&edited);
                notice = "Defaults (unsaved)";
            } else if (item == MENU_CANCEL)
                close_menu();
            else
                change_value(1);
            break;
        case HID_KEY_ESCAPE: close_menu(); break;
        default: return;
    }
    redraw = true;
}

void runtime_menu_init(void)
{
    boot_deadline = time_us_64() + BOOT_MENU_US;
    apply_settings();
    if (watchdog_caused_reboot()) {
        ahprintf("[system] watchdog reboot\n");
        disp_write(0, 2, "Watchdog reboot");
    }
}

bool runtime_menu_keyboard(uint8_t slot, hid_keyboard_report_t const *report)
{
    if (slot >= INPUT_BRIDGE_MAX_SLOTS)
        return true;
    hid_keyboard_report_t previous = keyboards[slot];
    keyboards[slot] = *report;
    if (opened) {
        for (unsigned i = 0; i < 6 && opened; i++)
            if (report->keycode[i] && !key_down(&previous, report->keycode[i]))
                menu_key(report->keycode[i]);
        return true;
    }
    if (quarantine[slot]) {
        if (report_empty(report))
            quarantine[slot] = false;
        return true;
    }
    return false;
}

void runtime_menu_disconnect(uint8_t slot)
{
    if (slot >= INPUT_BRIDGE_MAX_SLOTS)
        return;
    memset(&keyboards[slot], 0, sizeof(keyboards[slot]));
    quarantine[slot] = false;
}

void runtime_menu_filter_keyboard(hid_keyboard_report_t *report)
{
    settings_t const *settings = settings_get();
    bool display = disp_ssd_available();
    bool boot = time_us_64() < boot_deadline;
    bool recovery = boot && key_down(report, HID_KEY_F12);
    for (unsigned i = 0; i < 6; i++) {
        uint8_t key = report->keycode[i];
        if (display && (key == settings_menu_hid_key() || (boot && key == HID_KEY_F12) ||
            (recovery && key == HID_KEY_ESCAPE)))
            report->keycode[i] = 0;
    }
    /* With no working display, keep Right Amiga available if the saved
     * setting would otherwise redirect it to an inaccessible menu. */
    if (settings->right_gui == SETTINGS_GUI_OFF || (settings->right_gui == SETTINGS_GUI_MENU && display))
        report->modifier &= ~KEYBOARD_MODIFIER_RIGHTGUI;
}

void runtime_menu_task(void)
{
    if (opened) {
#ifdef ENABLE_BLUETOOTH_HID
        if (pair_pending) {
            pair_pending = false;
            if (settings_get()->bluetooth_enabled) {
                bt_hid_pair();
                notice = "Pairing: 2 minutes";
            } else notice = "Save Bluetooth On";
            redraw = true;
        }
        if (forget_pending) {
            forget_pending = false;
            if (settings_get()->bluetooth_enabled) {
                bt_hid_forget();
                notice = "Forgetting devices";
            } else notice = "Save Bluetooth On";
            redraw = true;
        }
#endif
        if (!disp_ssd_available()) {
            close_menu();
            return;
        }
        if (save_pending) {
            save_pending = false;
            if (settings_save(&edited)) {
                settings_set(&edited);
                apply_settings();
                close_menu();
                return;
            }
            notice = "Save failed; retry";
            redraw = true;
        }
        if (redraw)
            render_menu();
        return;
    }
    if (!disp_ssd_available())
        return;

    uint64_t now = time_us_64();
    bool boot = now < boot_deadline;
    bool entry = false, recovery = false;
    settings_t const *settings = settings_get();
    for (unsigned slot = 0; slot < INPUT_BRIDGE_MAX_SLOTS; slot++) {
        if (quarantine[slot])
            continue;
        hid_keyboard_report_t const *report = &keyboards[slot];
        if (boot && key_down(report, HID_KEY_F12)) {
            entry = true;
            recovery |= key_down(report, HID_KEY_ESCAPE);
        }
        if (boot || settings->menu_entry == SETTINGS_MENU_HOLD)
            entry |= key_down(report, settings_menu_hid_key()) ||
                (settings->right_gui == SETTINGS_GUI_MENU && (report->modifier & KEYBOARD_MODIFIER_RIGHTGUI));
    }
    if (recovery && !recovering)
        recovery_started = now;
    recovering = recovery;
    if (recovering && now - recovery_started >= RECOVERY_HOLD_US) {
        open_menu(true);
    } else {
        if (entry && !holding)
            hold_started = now;
        holding = entry;
        if (holding && !recovering && now - hold_started >= MENU_HOLD_US)
            open_menu(false);
    }
    if (opened)
        render_menu();
}

void runtime_menu_watchdog_task(void)
{
    /* Only feed after all main-loop services have returned. */
    if (watchdog_timeout)
        watchdog_update();
}
