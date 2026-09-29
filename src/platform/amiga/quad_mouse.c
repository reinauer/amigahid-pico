/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga quadrature mouse interface.
 */

#include "config.h"
#include "quad_mouse.h"
#include "util/debug_cons.h"
#include "util/output.h"
#include "hid_gamepad.h"

#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/gpio.h"

#include "pico/flash.h"
#include "pico/sem.h"
static semaphore_t mouse_core_ready;

#define AQM_MOTION_QUEUE_DEPTH 32
#define AQM_WHEEL_QUEUE_DEPTH 16
#define AQM_WHEEL_STEP_US 4
#define AQM_WHEEL_HOLD_US 40
#define AQM_WHEEL_RESPONSE_TIMEOUT_US 2000
#define AQM_TANKMOUSE_WHEEL_UP 0x0a
#define AQM_TANKMOUSE_WHEEL_DOWN 0x09
#define JOYSTICK_PORT1 1u
#define JOYSTICK_PORT2 2u

typedef struct
{
    int16_t x;
    int16_t y;
} aqm_motion_t;

typedef struct
{
    uint8_t steps;
    int8_t direction;
} aqm_axis_move_t;

static queue_t motion_queue;
static queue_t wheel_queue;
static volatile bool button_pressed[3];
static volatile uint16_t wheel_queued;
static volatile uint16_t wheel_requests;
static volatile uint16_t wheel_responses;
volatile uint8_t motion_divider = 2;
static volatile uint32_t step_interval_us = 300;
static volatile bool wheel_enabled = true;
static bool wheel_reverse;
static volatile bool input_captured;
static volatile uint8_t joystick_ports;
static volatile uint8_t joystick_state;

enum _mouse_pin_state { LOW, HIGH };

static inline int16_t _aqm_add_clamped(int16_t value, int16_t delta)
{
    int32_t sum = value + delta;

    if (sum > INT16_MAX)
        sum = INT16_MAX;
    else if (sum < INT16_MIN)
        sum = INT16_MIN;

    return (int16_t)sum;
}

static inline void _aqm_gpio_set(uint gpio, enum _mouse_pin_state state)
{
    if (state == LOW) {
        gpio_put(gpio, 0);
        gpio_set_dir(gpio, GPIO_OUT);
        return;
    }

    // assume it's high otherwise
    gpio_set_dir(gpio, GPIO_IN);
}

static inline bool _aqm_gpio_active(uint gpio)
{
    return !gpio_get(gpio);
}

static inline void _aqm_set_quad_state(uint gpio_main, uint gpio_quad, uint8_t state)
{
    // Set both destination levels so negative starts and reversals emit every
    // step. States (main, quadrature): 0 = 10, 1 = 11, 2 = 01, 3 = 00.
    switch (state & 3u) {
        case 0:
            _aqm_gpio_set(gpio_main, HIGH);
            _aqm_gpio_set(gpio_quad, LOW);
            break;
        case 1:
            _aqm_gpio_set(gpio_main, HIGH);
            _aqm_gpio_set(gpio_quad, HIGH);
            break;
        case 2:
            _aqm_gpio_set(gpio_main, LOW);
            _aqm_gpio_set(gpio_quad, HIGH);
            break;
        case 3:
            _aqm_gpio_set(gpio_main, LOW);
            _aqm_gpio_set(gpio_quad, LOW);
            break;
    }
}

static inline void _aqm_step_quad_axis(
    uint gpio_main,
    uint gpio_quad,
    uint8_t *state,
    uint8_t *phase,
    int8_t direction)
{
    if (direction < 0) {
        (*state)--;
        (*phase)--;
    } else {
        (*state)++;
        (*phase)++;
    }

    *state &= 3u;
    *phase &= 3u;
    _aqm_set_quad_state(gpio_main, gpio_quad, *state);
}

/*
 * TankMouse/Cocolino reads the low two bits of each Amiga mouse counter and
 * Gray-decodes them. Convert one 2-bit protocol axis value back to the counter
 * phase the driver expects to sample.
 */
static uint8_t _aqm_tankmouse_axis_code_to_phase(uint8_t code_axis)
{
    uint8_t code_bit0 = code_axis & 1u;
    uint8_t code_bit1 = (code_axis >> 1) & 1u;

    return ((code_bit0 ^ code_bit1) | (code_bit1 << 1)) & 3u;
}

static aqm_axis_move_t _aqm_step_quad_axis_to(
    uint gpio_main,
    uint gpio_quad,
    uint8_t *state,
    uint8_t *phase,
    uint8_t target)
{
    uint8_t forward = (target - *phase) & 3u;
    uint8_t backward = (*phase - target) & 3u;
    aqm_axis_move_t move = { 0, 1 };

    if (backward < forward) {
        move.steps = backward;
        move.direction = -1;
    } else {
        move.steps = forward;
        move.direction = 1;
    }

    for (uint8_t i = 0; i < move.steps; i++) {
        _aqm_step_quad_axis(gpio_main, gpio_quad, state, phase, move.direction);
        busy_wait_us_32(AQM_WHEEL_STEP_US);
    }

    return move;
}

static void _aqm_undo_quad_axis_move(
    uint gpio_main,
    uint gpio_quad,
    uint8_t *state,
    uint8_t *phase,
    aqm_axis_move_t move)
{
    for (uint8_t i = 0; i < move.steps; i++) {
        _aqm_step_quad_axis(gpio_main, gpio_quad, state, phase, -move.direction);
        busy_wait_us_32(AQM_WHEEL_STEP_US);
    }
}

static bool _aqm_wheel_dequeue(uint8_t *code)
{
    return queue_try_remove(&wheel_queue, code);
}

static void _aqm_wheel_clear(void)
{
    uint8_t dropped;

    while (queue_try_remove(&wheel_queue, &dropped)) {
    }
}

static void _aqm_wheel_enqueue(uint8_t code)
{
    if (!queue_try_add(&wheel_queue, &code)) {
        uint8_t dropped;

        if (queue_try_remove(&wheel_queue, &dropped))
            queue_try_add(&wheel_queue, &code);
    }

    wheel_queued++;
}

static void _aqm_tankmouse_respond(
    uint8_t code,
    uint8_t *quad_mx_state,
    uint8_t *quad_my_state,
    uint8_t *quad_mx_phase,
    uint8_t *quad_my_phase)
{
    uint8_t target_x = _aqm_tankmouse_axis_code_to_phase(code & 3u);
    uint8_t target_y = _aqm_tankmouse_axis_code_to_phase((code >> 2) & 3u);
    aqm_axis_move_t x_move;
    aqm_axis_move_t y_move;
    uint32_t started_at;

    // The TankMouse driver treats left-button high as "no middle button edge".
    _aqm_gpio_set(QM1_AMIGA_B1, HIGH);

    // The TankMouse driver treats right-button low as the wheel-code valid flag.
    _aqm_gpio_set(QM1_AMIGA_B2, LOW);

    x_move = _aqm_step_quad_axis_to(QM1_AMIGA_H, QM1_AMIGA_HQ, quad_mx_state, quad_mx_phase, target_x);
    y_move = _aqm_step_quad_axis_to(QM1_AMIGA_V, QM1_AMIGA_VQ, quad_my_state, quad_my_phase, target_y);

    busy_wait_us_32(AQM_WHEEL_HOLD_US);

    _aqm_undo_quad_axis_move(QM1_AMIGA_V, QM1_AMIGA_VQ, quad_my_state, quad_my_phase, y_move);
    _aqm_undo_quad_axis_move(QM1_AMIGA_H, QM1_AMIGA_HQ, quad_mx_state, quad_mx_phase, x_move);

    started_at = time_us_32();
    while (_aqm_gpio_active(QM1_AMIGA_B3)
        && ((uint32_t)(time_us_32() - started_at) < AQM_WHEEL_RESPONSE_TIMEOUT_US)) {
        tight_loop_contents();
    }

    _aqm_gpio_set(QM1_AMIGA_B1, button_pressed[AQM_LEFT] ? LOW : HIGH);
    _aqm_gpio_set(QM1_AMIGA_B2, button_pressed[AQM_RIGHT] ? LOW : HIGH);
}

static void _aqm_handle_tankmouse_request(
    uint8_t *quad_mx_state,
    uint8_t *quad_my_state,
    uint8_t *quad_mx_phase,
    uint8_t *quad_my_phase)
{
    uint8_t code;

    wheel_requests++;

    if (button_pressed[AQM_LEFT] || button_pressed[AQM_MIDDLE] || button_pressed[AQM_RIGHT]) {
        _aqm_wheel_clear();
        return;
    }

    if (_aqm_wheel_dequeue(&code)) {
        wheel_responses++;
        _aqm_tankmouse_respond(code, quad_mx_state, quad_my_state, quad_mx_phase, quad_my_phase);
    }
}

void amiga_quad_mouse_init()
{
    uint const pins[] = {QM1_AMIGA_H, QM1_AMIGA_V, QM1_AMIGA_HQ, QM1_AMIGA_VQ,
        QM1_AMIGA_B1, QM1_AMIGA_B2, QM1_AMIGA_B3,
#ifdef HAS_JOYSTICK_PORT2
        QM2_AMIGA_H, QM2_AMIGA_V, QM2_AMIGA_HQ, QM2_AMIGA_VQ,
        QM2_AMIGA_B1, QM2_AMIGA_B2, QM2_AMIGA_B3,
#endif
    };
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        gpio_init(pins[i]);
        gpio_set_function(pins[i], GPIO_FUNC_SIO);
        // Active low: release every line before starting core1.
        _aqm_gpio_set(pins[i], HIGH);
    }

    queue_init(&motion_queue, sizeof(aqm_motion_t), AQM_MOTION_QUEUE_DEPTH);
    queue_init(&wheel_queue, sizeof(uint8_t), AQM_WHEEL_QUEUE_DEPTH);

    // Both runtime settings and Bluetooth can write flash. Wait until core1
    // can be paused safely, including in USB-only builds.
    sem_init(&mouse_core_ready, 0, 1);

    // start the mouse motion loop on core1
    multicore_launch_core1(amiga_quad_mouse_motion);
    sem_acquire_blocking(&mouse_core_ready);
}

void amiga_quad_mouse_configure(uint16_t step_us, bool enabled, bool reverse)
{
    if (step_us == 300 || step_us == 200 || step_us == 150 || step_us == 100)
        step_interval_us = step_us;
    wheel_enabled = enabled;
    wheel_reverse = reverse;
}

void amiga_quad_mouse_capture(bool capture)
{
    input_captured = capture;
}

void amiga_quad_mouse_set_joystick_ports(bool port1, bool port2)
{
#ifndef HAS_JOYSTICK_PORT2
    port2 = false;
#endif
    for (unsigned i = 0; i < 3; i++)
        button_pressed[i] = false;
    joystick_state = 0;
    // Publish both choices together so core1 never sees a half-updated route.
    joystick_ports = (port1 ? JOYSTICK_PORT1 : 0) | (port2 ? JOYSTICK_PORT2 : 0);
}

void amiga_quad_mouse_joystick(uint8_t state)
{
    joystick_state = state;
}

static void _aqm_release_port(void)
{
    _aqm_gpio_set(QM1_AMIGA_H, HIGH);
    _aqm_gpio_set(QM1_AMIGA_V, HIGH);
    _aqm_gpio_set(QM1_AMIGA_HQ, HIGH);
    _aqm_gpio_set(QM1_AMIGA_VQ, HIGH);
    _aqm_gpio_set(QM1_AMIGA_B1, HIGH);
    _aqm_gpio_set(QM1_AMIGA_B2, HIGH);
    _aqm_gpio_set(QM1_AMIGA_B3, HIGH);
}

static void _aqm_joystick_output(uint8_t state, uint up, uint down, uint left, uint right,
    uint fire, uint fire2, uint fire3)
{
    // Amiga DE-9: pins 1/2/3/4 = up/down/left/right; buttons 1/2/3 = 6/9/5.
    // Use the same open-drain convention as the mouse output.
    _aqm_gpio_set(up, state & GAMEPAD_UP ? LOW : HIGH);
    _aqm_gpio_set(down, state & GAMEPAD_DOWN ? LOW : HIGH);
    _aqm_gpio_set(left, state & GAMEPAD_LEFT ? LOW : HIGH);
    _aqm_gpio_set(right, state & GAMEPAD_RIGHT ? LOW : HIGH);
    _aqm_gpio_set(fire, state & GAMEPAD_FIRE ? LOW : HIGH);
    _aqm_gpio_set(fire2, state & GAMEPAD_FIRE2 ? LOW : HIGH);
    _aqm_gpio_set(fire3, state & GAMEPAD_FIRE3 ? LOW : HIGH);
}

void amiga_quad_mouse_button(enum amiga_quad_mouse_buttons button, bool pressed)
{
    // ahprintf("[aqm] button %s state %s\n",
    //     (button == AQM_LEFT) ? "left" :
    //         (button == AQM_MIDDLE) ? "middle" :
    //         (button == AQM_RIGHT) ? "right" : "<unknown?!>",
    //     pressed ? "down" : "up"
    // );

    // Core1 owns every controller-port pin, including mode transitions.
    if (!(joystick_ports & JOYSTICK_PORT1) && (unsigned)button < 3)
        button_pressed[button] = pressed;
}

void amiga_quad_mouse_wheel(int8_t wheel)
{
    dbgcons_mouse_wheel(wheel);

    if (!wheel_enabled || input_captured || (joystick_ports & JOYSTICK_PORT1) ||
        button_pressed[AQM_LEFT] || button_pressed[AQM_MIDDLE] || button_pressed[AQM_RIGHT])
        return;

    // Promote before negating: a HID wheel report can contain -128.
    int16_t delta = wheel_reverse ? -(int16_t)wheel : wheel;
    while (delta > 0) {
        _aqm_wheel_enqueue(AQM_TANKMOUSE_WHEEL_UP);
        delta--;
    }

    while (delta < 0) {
        _aqm_wheel_enqueue(AQM_TANKMOUSE_WHEEL_DOWN);
        delta++;
    }
}

void amiga_quad_mouse_set_motion(int16_t in_x, int16_t in_y)
{
    if ((joystick_ports & JOYSTICK_PORT1) || input_captured)
        return;
    aqm_motion_t motion = { in_x, in_y };

    if (!queue_try_add(&motion_queue, &motion)) {
        aqm_motion_t oldest;

        // Coalesce the oldest sample with the new delta on overflow. The
        // consumer sums all queued samples before its next output step.
        if (queue_try_remove(&motion_queue, &oldest)) {
            motion.x = _aqm_add_clamped(oldest.x, in_x);
            motion.y = _aqm_add_clamped(oldest.y, in_y);
        }

        // Core 0 is the only producer: either we freed a slot above, or core 1
        // emptied the queue before our remove. In either case this add fits.
        queue_try_add(&motion_queue, &motion);
    }
}

void amiga_quad_mouse_motion()
{
    if (!flash_safe_execute_core_init())
        panic("Mouse core flash lockout initialization failed");
    sem_release(&mouse_core_ready);

    // ahprintf("[aqm] hello from core1, mouse motion output loop starting\n");
    aqm_motion_t motion;
    int16_t out_x = 0, out_y = 0;
    int16_t x_residue = 0, y_residue = 0;
    uint8_t quad_mx_state = 1, quad_my_state = 1;
    uint8_t quad_mx_phase = 0, quad_my_phase = 0;
    uint8_t divider;
    bool previous_mode = false;
    uint8_t previous_joystick = 0xff, previous_buttons = 0xff;
#ifdef HAS_JOYSTICK_PORT2
    uint8_t previous_joystick2 = 0xff;
#endif
    bool last_mmb_state = _aqm_gpio_active(QM1_AMIGA_B3);
    // This deadline stays unchanged while idle. A signed 32-bit comparison
    // would treat it as a future deadline after about 36 minutes of inactivity.
    uint64_t next_motion_at = time_us_64();

    /**
     * a little note about quadrature motion state.
     *
     * quadrature motion works by having a hardware-side counter for each axis and two signal
     * lines per axis. motion is signalled in an offset time division; the main axis pulse
     * changes state on time 0 and time 1, and the second signal line at time interval 0.5 and
     * 1.5, giving four possible states for each t/2. this occurs on both x and y axis.
     *
     * adcd has a crude ascii timing diagram but it explains it better:
     * https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node017F.html
     */

    while (1) {
        uint8_t ports = joystick_ports;
        bool joystick = (ports & JOYSTICK_PORT1) != 0;
#ifdef HAS_JOYSTICK_PORT2
        // Port 2 runs alongside mouse quadrature, buttons and wheel on port 1.
        uint8_t state2 = (ports & JOYSTICK_PORT2) && !input_captured ? joystick_state : 0;
        if (state2 != previous_joystick2) {
            _aqm_joystick_output(state2, QM2_AMIGA_V, QM2_AMIGA_H, QM2_AMIGA_VQ, QM2_AMIGA_HQ,
                QM2_AMIGA_B1, QM2_AMIGA_B2, QM2_AMIGA_B3);
            previous_joystick2 = state2;
        }
#endif
        if (joystick != previous_mode) {
            _aqm_release_port();
            while (queue_try_remove(&motion_queue, &motion))
                ;
            _aqm_wheel_clear();
            out_x = out_y = x_residue = y_residue = 0;
            quad_mx_state = quad_my_state = 1;
            quad_mx_phase = quad_my_phase = 0;
            previous_joystick = previous_buttons = 0xff;
            previous_mode = joystick;
            last_mmb_state = _aqm_gpio_active(QM1_AMIGA_B3);
        }
        if (joystick) {
            uint8_t state = input_captured ? 0 : joystick_state;
            if (state != previous_joystick) {
                _aqm_joystick_output(state, QM1_AMIGA_V, QM1_AMIGA_H, QM1_AMIGA_VQ, QM1_AMIGA_HQ,
                    QM1_AMIGA_B1, QM1_AMIGA_B2, QM1_AMIGA_B3);
                previous_joystick = state;
            }
            tight_loop_contents();
            continue;
        }
        uint8_t buttons = (button_pressed[AQM_LEFT] ? 1u : 0u) |
            (button_pressed[AQM_MIDDLE] ? 2u : 0u) | (button_pressed[AQM_RIGHT] ? 4u : 0u);
        if (buttons != previous_buttons) {
            _aqm_gpio_set(QM1_AMIGA_B1, buttons & 1u ? LOW : HIGH);
            _aqm_gpio_set(QM1_AMIGA_B3, buttons & 2u ? LOW : HIGH);
            _aqm_gpio_set(QM1_AMIGA_B2, buttons & 4u ? LOW : HIGH);
            previous_buttons = buttons;
        }
        bool mmb_state = _aqm_gpio_active(QM1_AMIGA_B3);

        if (input_captured) {
            while (queue_try_remove(&motion_queue, &motion))
                ;
            uint8_t wheel;
            while (queue_try_remove(&wheel_queue, &wheel))
                ;
            out_x = out_y = x_residue = y_residue = 0;
            last_mmb_state = mmb_state;
            tight_loop_contents();
            continue;
        }

        if (wheel_enabled && mmb_state && !last_mmb_state)
            _aqm_handle_tankmouse_request(&quad_mx_state, &quad_my_state, &quad_mx_phase, &quad_my_phase);
        last_mmb_state = mmb_state;

        // Merge newly queued USB deltas between quadrature steps so we do not
        // lose motion across cores or wait for an entire stale batch to drain.
        divider = motion_divider ? motion_divider : 1;

        while (queue_try_remove(&motion_queue, &motion)) {
            x_residue = _aqm_add_clamped(x_residue, motion.x);
            y_residue = _aqm_add_clamped(y_residue, motion.y);
        }

        // Opposite deltas cancel pending steps, preserving net displacement.
        // A large backlog can therefore delay a physical direction change.
        out_x = _aqm_add_clamped(out_x, x_residue / divider);
        out_y = _aqm_add_clamped(out_y, y_residue / divider);
        // Retain sub-step motion for the next report, but do not add the
        // already-consumed whole steps again on the next loop iteration.
        x_residue %= divider;
        y_residue %= divider;

        if ((out_x == 0) && (out_y == 0)) {
            tight_loop_contents();
            continue;
        }

        if (time_us_64() < next_motion_at) {
            tight_loop_contents();
            continue;
        }

        if (out_x != 0) {
            // handle x-axis motion
            if (out_x < 0)
                _aqm_step_quad_axis(QM1_AMIGA_H, QM1_AMIGA_HQ, &quad_mx_state, &quad_mx_phase, -1);
            else if (out_x > 0)
                _aqm_step_quad_axis(QM1_AMIGA_H, QM1_AMIGA_HQ, &quad_mx_state, &quad_mx_phase, 1);
        }

        if (out_x < 0) out_x++;
        if (out_x > 0) out_x--;

        if (out_y != 0) {
            // handle y-axis motion
            if (out_y < 0)
                _aqm_step_quad_axis(QM1_AMIGA_V, QM1_AMIGA_VQ, &quad_my_state, &quad_my_phase, -1);
            else if (out_y > 0)
                _aqm_step_quad_axis(QM1_AMIGA_V, QM1_AMIGA_VQ, &quad_my_state, &quad_my_phase, 1);
        }

        if (out_y < 0) out_y++;
        if (out_y > 0) out_y--;

        next_motion_at = time_us_64() + step_interval_us;
    }
}
