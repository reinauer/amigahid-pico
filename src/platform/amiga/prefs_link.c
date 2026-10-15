/* SPDX-License-Identifier: EPL-2.0
 * Pin 5: Amiga clock. Pin 9: half-duplex data. Pin 6: Pico acknowledge.
 * Pico outputs only sink current or release. Each rising/falling clock edge
 * is acknowledged; the Amiga owns clock and never drives pin 6.
 */
#include "prefs_link.h"
#include "config.h"
#include "cd32.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include <string.h>
#ifdef HAS_JOYSTICK_PORT2
#define FRAME_TIMEOUT_US 500000u
#define SESSION_TIMEOUT_US 300000000u
static queue_t requests, replies;
static volatile uint32_t requested_session;
static volatile bool active;
static uint32_t next_session, seen_session;
static volatile uint32_t generation;
static uint64_t last_edge, last_frame, closing_at;
static bool clock_high, transmitting, waiting, end_after_reply;
static unsigned bit;
static prefs_packet_t rx, tx;

static void sink(unsigned pin, bool low)
{
    gpio_put(pin, 0);
    gpio_set_dir(pin, low ? GPIO_OUT : GPIO_IN);
}
static void release_port(void)
{
    unsigned const pins[] = {QM2_AMIGA_H, QM2_AMIGA_V, QM2_AMIGA_HQ,
        QM2_AMIGA_VQ, QM2_AMIGA_B1, QM2_AMIGA_B2, QM2_AMIGA_B3};
    for (unsigned i = 0; i < sizeof(pins)/sizeof(pins[0]); i++) {
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_set_function(pins[i], GPIO_FUNC_SIO);
    }
}
static void reset_frame(uint64_t now)
{
    generation++;
    memset(&rx, 0, sizeof(rx));
    rx.generation = generation;
    bit = 0;
    closing_at = 0;
    transmitting = waiting = end_after_reply = false;
    clock_high = gpio_get(QM2_AMIGA_B3);
    sink(QM2_AMIGA_B2, false);
    sink(QM2_AMIGA_B1, !clock_high);
    last_edge = now;
}
void prefs_link_init(void)
{
    queue_init(&requests, sizeof(prefs_packet_t), 2);
    queue_init(&replies, sizeof(prefs_packet_t), 2);
}
void prefs_link_begin(void)
{
    if (!++next_session) ++next_session;
    requested_session = next_session;
}
void prefs_link_end(void) { requested_session = 0; }
bool prefs_link_active(void) { return active; }
bool prefs_link_current(prefs_packet_t const *packet) { return active && packet->generation == generation; }
bool prefs_link_receive(prefs_packet_t *packet) { return queue_try_remove(&requests, packet); }
void prefs_link_reply(prefs_packet_t const *packet) { queue_try_add(&replies, packet); }

bool prefs_link_tick(void)
{
    uint64_t now = time_us_64();
    uint32_t wanted = requested_session;
    if (wanted != seen_session) {
        seen_session = wanted;
        if (active) release_port();
        active = wanted != 0;
        if (active) {
            cd32_enable(1, false);
            release_port();
            last_frame = now;
            reset_frame(now);
        }
    }
    if (!active) return false;
    if (now - last_frame >= SESSION_TIMEOUT_US) {
        release_port();
        active = false;
        return false;
    }
    if (closing_at) {
        if (now >= closing_at) {
            release_port();
            active = false;
            return false;
        }
        return true;
    }
    /* A quiet low clock is a resynchronization point after a broken frame.
     * Core0 can take up to 500 ms for flash lockout; allow 2 s for its reply. */
    if ((bit || transmitting || waiting) && now - last_edge > (waiting ? 2000000u : FRAME_TIMEOUT_US))
        reset_frame(now);
    prefs_packet_t reply;
    while (queue_try_remove(&replies, &reply)) {
        if (waiting && reply.generation == generation) {
            tx = reply;
            waiting = false;
            transmitting = true;
            bit = 0;
            end_after_reply = tx.bytes[3] == (PREFS_END | 0x80) && tx.bytes[7] == PREFS_OK;
        }
    }
    bool level = gpio_get(QM2_AMIGA_B3);
    if (level == clock_high) return true;
    if (level && waiting) return true; // Stretch the first response bit until core0 is ready.
    if (level) {
        if (transmitting) {
            sink(QM2_AMIGA_B2, !(tx.bytes[bit / 8] & (0x80u >> (bit % 8))));
        } else if (gpio_get(QM2_AMIGA_B2)) {
            rx.bytes[bit / 8] |= 0x80u >> (bit % 8);
        }
        bit++;
        sink(QM2_AMIGA_B1, false);
    } else {
        sink(QM2_AMIGA_B1, true);
        if (bit == PREFS_FRAME_SIZE * 8) {
            if (transmitting) {
                if (end_after_reply) {
                    /* Hold the final ACK until the host releases its pins. */
                    closing_at = now + 250000u;
                    return true;
                }
                reset_frame(now);
            } else {
                queue_try_add(&requests, &rx);
                waiting = true;
            }
            last_frame = now;
        }
    }
    clock_high = level;
    last_edge = now;
    return true;
}
#else
void prefs_link_init(void) {}
void prefs_link_begin(void) {}
void prefs_link_end(void) {}
bool prefs_link_active(void) { return false; }
bool prefs_link_current(prefs_packet_t const *packet) { (void)packet; return false; }
bool prefs_link_receive(prefs_packet_t *packet) { (void)packet; return false; }
void prefs_link_reply(prefs_packet_t const *packet) { (void)packet; }
bool prefs_link_tick(void) { return false; }
#endif
