/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * shared hid input bridging for usb and bluetooth sources.
 */

#include "input_bridge.h"
#include "config.h"

#include <stdbool.h>
#include <string.h>

#include "platform/amiga/keyboard_serial_io.h"
#include "platform/amiga/quad_mouse.h"
#include "util/debug_cons.h"
#include "runtime_menu.h"
#include "hid_gamepad.h"
#include "keyboard_map.h"

typedef struct
{
    hid_keyboard_report_t keyboard;
    hid_mouse_report_t mouse;
    uint8_t led_report;
    bool mouse_quarantine;
    uint16_t gamepad;
    bool gamepad_quarantine;
} input_bridge_state_t;

static input_bridge_state_t bridge_state[INPUT_BRIDGE_MAX_SLOTS];
static bool input_captured;
static bool joystick_mode;
static bool joystick_port2;
static uint8_t controller_mode1, controller_mode2;
static uint16_t gamepad_output;
static uint8_t mouse_buttons;

input_bridge_gamepad_status_t input_bridge_gamepad_status(uint8_t slot)
{
    return (input_bridge_gamepad_status_t) {
        .state = gamepad_output,
        .ports = (joystick_mode ? 1u : 0u) | (joystick_port2 ? 2u : 0u),
        .waiting = input_captured ||
            (slot < INPUT_BRIDGE_MAX_SLOTS && bridge_state[slot].gamepad_quarantine),
    };
}

static void _ib_sync_gamepads(void)
{
    uint16_t state = 0;
    if ((joystick_mode || joystick_port2) && !input_captured)
        for (unsigned slot = 0; slot < INPUT_BRIDGE_MAX_SLOTS; slot++)
            state |= bridge_state[slot].gamepad;
    // Opposing directions cancel, including across separate controllers.
    if ((state & (GAMEPAD_UP | GAMEPAD_DOWN)) == (GAMEPAD_UP | GAMEPAD_DOWN))
        state &= ~(GAMEPAD_UP | GAMEPAD_DOWN);
    if ((state & (GAMEPAD_LEFT | GAMEPAD_RIGHT)) == (GAMEPAD_LEFT | GAMEPAD_RIGHT))
        state &= ~(GAMEPAD_LEFT | GAMEPAD_RIGHT);
    gamepad_output = state;
    amiga_quad_mouse_joystick(state);
}

static bool keyboard_output[128];

static void _ib_sync_keyboard(void)
{
    bool keys[128] = {0};
    if (!input_captured) {
        for (unsigned slot = 0; slot < INPUT_BRIDGE_MAX_SLOTS; slot++) {
            hid_keyboard_report_t const *report = &bridge_state[slot].keyboard;
            for (unsigned i = 0; i < 6; i++) {
                uint8_t code = keyboard_map_key(report->keycode[i]);
                if (code < sizeof(keys)) keys[code] = true;
            }
            for (unsigned bit = 0; bit < 8; bit++)
                if (report->modifier & (1u << bit)) keys[keyboard_map_modifier(bit)] = true;
        }
    }
    // Aggregate after translation: two remaps (or keyboards) can hold the
    // same Amiga key. Release old targets before pressing new targets.
    for (unsigned code = 0; code < sizeof(keys); code++)
        if (keyboard_output[code] && !keys[code]) amiga_send(code, true);
    for (unsigned code = 0; code < sizeof(keys); code++)
        if (!keyboard_output[code] && keys[code]) amiga_send(code, false);
    memcpy(keyboard_output, keys, sizeof(keys));
}

static void _ib_update_keyboard_leds(input_bridge_state_t *state, input_bridge_keyboard_sink_t const *sink)
{
    uint8_t led_report = amiga_caps_lock() ? KEYBOARD_LED_CAPSLOCK : 0;

    if ((sink != NULL) && (sink->set_leds != NULL) && (state->led_report != led_report))
        sink->set_leds(sink->ctx, led_report);

    state->led_report = led_report;
}

static void _ib_sync_mouse_buttons(void)
{
    uint8_t buttons = 0;
    if (!input_captured && !joystick_mode)
        for (unsigned slot = 0; slot < INPUT_BRIDGE_MAX_SLOTS; slot++)
            buttons |= bridge_state[slot].mouse.buttons;
    uint8_t changed = buttons ^ mouse_buttons;
    if (changed & MOUSE_BUTTON_LEFT)
        amiga_quad_mouse_button(AQM_LEFT, (buttons & MOUSE_BUTTON_LEFT) != 0);
    if (changed & MOUSE_BUTTON_MIDDLE)
        amiga_quad_mouse_button(AQM_MIDDLE, (buttons & MOUSE_BUTTON_MIDDLE) != 0);
    if (changed & MOUSE_BUTTON_RIGHT)
        amiga_quad_mouse_button(AQM_RIGHT, (buttons & MOUSE_BUTTON_RIGHT) != 0);
    mouse_buttons = buttons;
}

void input_bridge_reset(uint8_t slot)
{
    if (slot >= INPUT_BRIDGE_MAX_SLOTS)
        return;

    memset(&bridge_state[slot], 0, sizeof(bridge_state[slot]));
    runtime_menu_disconnect(slot);
    _ib_sync_keyboard();
    _ib_sync_mouse_buttons();
    _ib_sync_gamepads();
}

void input_bridge_disconnect(uint8_t slot)
{

    if (slot >= INPUT_BRIDGE_MAX_SLOTS)
        return;


    memset(&bridge_state[slot], 0, sizeof(bridge_state[slot]));
    runtime_menu_disconnect(slot);
    _ib_sync_keyboard();
    _ib_sync_mouse_buttons();
    _ib_sync_gamepads();
}

void input_bridge_capture(bool capture)
{
    input_captured = capture;
    if (capture) {
        for (unsigned slot = 0; slot < INPUT_BRIDGE_MAX_SLOTS; slot++) {
            input_bridge_state_t *state = &bridge_state[slot];


            memset(&state->keyboard, 0, sizeof(state->keyboard));
            memset(&state->mouse, 0, sizeof(state->mouse));
            state->mouse_quarantine = true;
            state->gamepad = 0;
            state->gamepad_quarantine = true;
        }
    }
    _ib_sync_keyboard();
    amiga_quad_mouse_capture(capture);
    _ib_sync_mouse_buttons();
    _ib_sync_gamepads();
}

void input_bridge_set_port_modes(uint8_t mode1, uint8_t mode2)
{
#ifndef HAS_JOYSTICK_PORT2
    mode2 = 0;
#endif
    if (controller_mode1 == mode1 && controller_mode2 == mode2) return;
    controller_mode1 = mode1;
    controller_mode2 = mode2;
    joystick_mode = mode1 != 0;
    joystick_port2 = mode2 != 0;
    for (unsigned slot = 0; slot < INPUT_BRIDGE_MAX_SLOTS; slot++) {
        memset(&bridge_state[slot].mouse, 0, sizeof(bridge_state[slot].mouse));
        bridge_state[slot].mouse_quarantine = true;
        bridge_state[slot].gamepad = 0;
        bridge_state[slot].gamepad_quarantine = true;
    }
    amiga_quad_mouse_set_controller_ports(mode1, mode2);
    _ib_sync_mouse_buttons();
    _ib_sync_gamepads();
}

void input_bridge_handle_gamepad(uint8_t slot, uint16_t state)
{
    if (slot >= INPUT_BRIDGE_MAX_SLOTS)
        return;
    input_bridge_state_t *source = &bridge_state[slot];
    if (input_captured) {
        source->gamepad = 0;
        source->gamepad_quarantine = true;
    } else if (source->gamepad_quarantine) {
        if (state == 0)
            source->gamepad_quarantine = false;
    } else {
        source->gamepad = state;
    }
    _ib_sync_gamepads();
}

void input_bridge_handle_keyboard(uint8_t slot, hid_keyboard_report_t const *report,
    input_bridge_keyboard_sink_t const *sink)
{
    input_bridge_state_t *state;

    if ((slot >= INPUT_BRIDGE_MAX_SLOTS) || (report == NULL))
        return;

    state = &bridge_state[slot];

    // Rollover/error reports do not describe released keys or menu actions.
    for (unsigned i = 0; i < 6; i++)
        if (report->keycode[i] >= 1 && report->keycode[i] <= 3)
            return;
    if (runtime_menu_keyboard(slot, report)) {
        _ib_update_keyboard_leds(state, sink);
        return;
    }
    hid_keyboard_report_t filtered = *report;
    runtime_menu_filter_keyboard(&filtered);


    state->keyboard = filtered;
    _ib_sync_keyboard();
    _ib_update_keyboard_leds(state, sink);
}

void input_bridge_handle_keyboard_boot(uint8_t slot, uint8_t modifier, uint8_t const keycode[6])
{
    hid_keyboard_report_t report = { 0, 0, {0} };

    report.modifier = modifier;
    memcpy(report.keycode, keycode, sizeof(report.keycode));
    input_bridge_handle_keyboard(slot, &report, NULL);
}

void input_bridge_handle_mouse(uint8_t slot, hid_mouse_report_t const *report)
{
    input_bridge_state_t *state;

    if ((slot >= INPUT_BRIDGE_MAX_SLOTS) || (report == NULL))
        return;

    state = &bridge_state[slot];

    if (input_captured || joystick_mode) {
        // Also covers a mouse connected after the menu was opened.
        state->mouse_quarantine = true;
        return;
    }
    if (state->mouse_quarantine) {
        if (report->buttons == 0)
            state->mouse_quarantine = false;
        return;
    }

    state->mouse = *report;
    // Releasing one device must not release a button still held on another.
    // Each source retains its own button state.
    _ib_sync_mouse_buttons();

    dbgcons_mouse_report(report->x, report->y, report->buttons);

    if (report->x || report->y)
        amiga_quad_mouse_set_motion(report->x, report->y);

    if (report->wheel)
        amiga_quad_mouse_wheel(report->wheel);
}

void input_bridge_handle_mouse_boot(uint8_t slot, uint8_t buttons, int8_t x, int8_t y, int8_t wheel)
{
    hid_mouse_report_t report = { 0 };

    report.buttons = buttons;
    report.x = x;
    report.y = y;
    report.wheel = wheel;
    input_bridge_handle_mouse(slot, &report);
}
