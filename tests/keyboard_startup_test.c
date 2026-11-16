/* Exercise the real keyboard service using a simulated clock and GPIO bus.
 * SPDX-License-Identifier: EPL-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/platform/amiga/keyboard_serial_io.c"

static uint64_t now;
static bool output[32], external_low;
static uint8_t wire, bit_count, frames[256];
static unsigned frame_count;
uint64_t time_us_64(void) { return now; }
void sleep_us(uint64_t us) { now += us; }
void sleep_ms(uint32_t ms) { now += (uint64_t)ms * 1000; }
void gpio_init(uint p) { output[p] = false; }
void gpio_set_function(uint p, uint f) { (void)p; assert(f == GPIO_FUNC_SIO); }
void gpio_pull_up(uint p) { (void)p; }
void gpio_put(uint p, bool value) { (void)p; assert(!value); }
bool gpio_get(uint p) { return !output[p] && !(p == KBD_AMIGA_RST && external_low); }
void gpio_set_dir(uint p, bool out)
{
    output[p] = out;
    if (p == KBD_AMIGA_CLK && out && !reset_output_active) {
        wire = (uint8_t)((wire << 1) | output[KBD_AMIGA_DAT]);
        if (++bit_count == 8) {
            assert(frame_count < sizeof(frames));
            frames[frame_count++] = (uint8_t)((wire >> 1) | (wire << 7));
            bit_count = wire = 0;
        }
    }
}
void queue_init(queue_t *q, unsigned size, unsigned capacity)
{
    assert(size * capacity <= sizeof(q->data));
    memset(q, 0, sizeof(*q)); q->size = size; q->capacity = capacity;
}
bool queue_try_add(queue_t *q, const void *item)
{
    if (q->count == q->capacity) return false;
    memcpy(q->data + ((q->head + q->count) % q->capacity) * q->size, item, q->size);
    q->count++; return true;
}
bool queue_try_peek(queue_t *q, void *item)
{
    if (!q->count) return false;
    memcpy(item, q->data + q->head * q->size, q->size); return true;
}
bool queue_try_remove(queue_t *q, void *item)
{
    if (!queue_try_peek(q, item)) return false;
    q->head = (q->head + 1) % q->capacity; q->count--; return true;
}
void ahprintf(char const *fmt, ...) { (void)fmt; }
void usb_hid_sync_keyboard_leds(void) {}
void dbgcons_amiga_key(uint8_t h, uint8_t a, char *s) { (void)h; (void)a; (void)s; }
uint8_t keyboard_map_key(uint8_t h) { return h; }
uint8_t keyboard_map_modifier(unsigned b) { return (uint8_t)b; }

static void fresh(bool reset_low)
{
    now = 0; frame_count = bit_count = wire = 0; external_low = reset_low;
    caps_lock = ctrl = lamiga = ramiga = in_reset = false;
    local_reset_active = reset_output_active = external_reset_active = false;
    reset_line_was_low = keyboard_resync = false;
    memset(wanted_keys, 0, sizeof(wanted_keys));
    memset(sent_keys, 0, sizeof(sent_keys));
    amiga_init();
    assert(now == 0 && frame_count == 0); // No blocking power-up delay.
}
static void finish_handshake(uint64_t init_at)
{
    unsigned before = frame_count;
    now = init_at - 1; amiga_service(); assert(frame_count == before);
    now = init_at; amiga_service(); assert(frames[before] == AMIGA_INITPOWER);
    now += 199999; amiga_service(); assert(frame_count == before + 1);
    now++; amiga_service(); assert(frames[before + 1] == AMIGA_TERMPOWER);
}
int main(void)
{
    fresh(false);
    amiga_send(AMIGA_A, false); amiga_send(AMIGA_A, true);
    finish_handshake(1000000);
    assert(frame_count == 2);
    amiga_service(); amiga_service();
    assert(frame_count == 4 && frames[2] == AMIGA_A && frames[3] == (AMIGA_A | 0x80));

    // Overflow while waiting must still release the last key after draining.
    fresh(false);
    for (unsigned i = 0; i < 65; i++) amiga_send(AMIGA_A, i % 2 != 0);
    amiga_send(AMIGA_A, true);
    finish_handshake(1000000);
    for (unsigned i = 0; i < 150; i++) amiga_service();
    assert(frames[frame_count - 1] == (AMIGA_A | 0x80));

    // A held reset chord is processed immediately, even with queued keys.
    fresh(false);
    amiga_send(AMIGA_A, false);
    amiga_send(AMIGA_CTRL, false); amiga_send(AMIGA_LAMIGA, false);
    amiga_send(AMIGA_RAMIGA, false); amiga_service();
    assert(output[KBD_AMIGA_RST] && output[KBD_AMIGA_CLK] && frame_count == 0);
    now = 2000000; amiga_service(); assert(frame_count == 0);
    amiga_send(AMIGA_CTRL, true); amiga_service();
    assert(!output[KBD_AMIGA_RST] && !output[KBD_AMIGA_CLK]);
    finish_handshake(now + 1000000);
    amiga_service(); assert(frames[2] == (AMIGA_CTRL | 0x80));

    // An external reset between the two startup codes restarts the sequence.
    fresh(false);
    now = 1000000; amiga_service(); assert(frames[0] == AMIGA_INITPOWER);
    external_low = true; amiga_service();
    now = 3000000; amiga_service(); assert(frame_count == 1);
    external_low = false; amiga_service();
    finish_handshake(now + 1000000); assert(frame_count == 3);

    // Reset may already be low at power-up; wait for its release too.
    fresh(true);
    now = 2000000; amiga_service(); assert(frame_count == 0);
    external_low = false; amiga_service(); finish_handshake(now + 1000000);

    // Once ready, a normal reset does not replay cold-start codes.
    amiga_assert_reset(); amiga_release_reset();
    unsigned before = frame_count;
    now += 2000000; amiga_service(); assert(frame_count == before);
    puts("keyboard startup: PASS");
}
