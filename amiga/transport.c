/* Native AmigaOS 2.04+ transport. SPDX-License-Identifier: EPL-2.0 */
#include "transport.h"
#include "prefs_protocol.h"
#include <exec/types.h>
#include <devices/gameport.h>
#include <devices/timer.h>
#include <hardware/cia.h>
#include <hardware/custom.h>
#include <resources/potgo.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/potgo.h>
#include <proto/timer.h>
#include <string.h>

struct Library *PotgoBase;
struct Device *TimerBase;
static volatile struct CIA *const cia = (struct CIA *)0xbfe001;
static volatile struct Custom *const custom = (struct Custom *)0xdff000;
static struct MsgPort *port;
static struct IOStdReq *game;
static struct timerequest *timer;
static bool game_open, timer_open, claimed, pins_owned;
static UWORD pot_bits;
static UBYTE old_fire_dir;
static ULONG frequency;
static uint16_t sequence;
#define POT_MASK 0xf000u
#define CLOCK_OUT 0x2000u
#define CLOCK_HIGH 0x1000u
#define DATA_OUT 0x8000u
#define DATA_HIGH 0x4000u
#define FIRE_BIT 0x80u

static ULONG ticks(void)
{
    struct EClockVal now;
    ReadEClock(&now);
    return now.ev_lo;
}
static void settle(void)
{
    ULONG started = ticks();
    ULONG delay = (frequency + 19999u) / 20000u; // >= 50 us, independent of CPU speed.
    while ((ULONG)(ticks() - started) < delay) { }
}
static bool wait_ack(bool high, ULONG timeout_ms)
{
    ULONG started = ticks(), limit = frequency / 1000u * timeout_ms;
    while (!!(cia->ciapra & FIRE_BIT) != high) {
        if ((ULONG)(ticks() - started) >= limit) return false;
    }
    return true;
}
static void pins(bool clock, bool data)
{
    /* Amiga POT outputs are current-limited. Leaving DATA high while receiving
     * supplies a pull-up and avoids periodic POT counter discharge. The Pico
     * only sinks/release this line, just like a CD32 pad's serial output. */
    WritePotgo(CLOCK_OUT | DATA_OUT | (clock ? CLOCK_HIGH : 0) |
        (data ? DATA_HIGH : 0), POT_MASK);
}
static bool game_type(UWORD command, BYTE *type)
{
    game->io_Command = command;
    game->io_Flags = IOF_QUICK;
    game->io_Data = type;
    game->io_Length = 1;
    return DoIO((struct IORequest *)game) == 0;
}
char const *transport_open(void)
{
    if (pins_owned) return NULL;
    char const *error = "Cannot open timer/gameport resources.";
    struct EClockVal now;
    if (!(PotgoBase = OpenResource(POTGONAME))) goto fail;
    if (!(port = CreateMsgPort())) goto fail;
    if (!(game = (struct IOStdReq *)CreateIORequest(port, sizeof(*game)))) goto fail;
    if (!(timer = (struct timerequest *)CreateIORequest(port, sizeof(*timer)))) goto fail;
    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)timer, 0)) goto fail;
    timer_open = true;
    TimerBase = timer->tr_node.io_Device;
    frequency = ReadEClock(&now);
    if (!frequency) goto fail;
    if (OpenDevice("gameport.device", 1, (struct IORequest *)game, 0)) goto fail;
    game_open = true;
    error = "Amiga port 2 is in use by another program.";
    BYTE type = GPCT_NOCONTROLLER;
    Forbid();
    if (game_type(GPD_ASKCTYPE, &type) && type == GPCT_NOCONTROLLER) {
        type = GPCT_ALLOCATED;
        claimed = game_type(GPD_SETCTYPE, &type);
    }
    Permit();
    if (!claimed) goto fail;
    pot_bits = AllocPotBits(POT_MASK);
    if ((pot_bits & 0x5000u) != 0x5000u) goto fail; // OUT bits are unspecified.
    /* Only the port-2 fire direction bit is changed; preserve disk and port-1
     * CIA bits. Interrupts remain enabled throughout all communication. */
    Disable();
    old_fire_dir = cia->ciaddra & FIRE_BIT;
    cia->ciaddra &= ~FIRE_BIT;
    Enable();
    pins_owned = true;
    pins(false, true);
    return NULL;
fail:
    transport_close();
    return error;
}
void transport_close(void)
{
    if (pins_owned) {
        WritePotgo(POT_MASK, POT_MASK); // Return the POT lines to released/high.
        Disable();
        cia->ciaddra = (cia->ciaddra & ~FIRE_BIT) | old_fire_dir;
        Enable();
        pins_owned = false;
    }
    if (pot_bits) { FreePotBits(pot_bits); pot_bits = 0; }
    if (claimed) {
        BYTE type = GPCT_NOCONTROLLER;
        game_type(GPD_SETCTYPE, &type);
        claimed = false;
    }
    if (game_open) { CloseDevice((struct IORequest *)game); game_open = false; }
    if (timer_open) { CloseDevice((struct IORequest *)timer); timer_open = false; }
    if (game) { DeleteIORequest((struct IORequest *)game); game = NULL; }
    if (timer) { DeleteIORequest((struct IORequest *)timer); timer = NULL; }
    if (port) { DeleteMsgPort(port); port = NULL; }
    TimerBase = NULL;
}
static bool exchange(uint8_t const *request, uint8_t *response)
{
    pins(false, true);
    if (!wait_ack(false, 100)) return false;
    for (unsigned i = 0; i < PREFS_FRAME_SIZE * 8; i++) {
        bool data = !!(request[i / 8] & (0x80u >> (i % 8)));
        pins(false, data);
        settle();
        pins(true, data);
        if (!wait_ack(true, 100)) return false;
        pins(false, data);
        if (!wait_ack(false, 100)) return false;
    }
    memset(response, 0, PREFS_FRAME_SIZE);
    pins(false, true); // Hand data back to Pico before its first response bit.
    settle();
    for (unsigned i = 0; i < PREFS_FRAME_SIZE * 8; i++) {
        pins(true, true);
        if (!wait_ack(true, i ? 100 : 3000)) return false;
        settle();
        if (custom->potinp & DATA_HIGH) response[i / 8] |= 0x80u >> (i % 8);
        pins(false, true);
        if (!wait_ack(false, 100)) return false;
    }
    return true;
}
char const *transport_command(uint8_t command, uint8_t const *payload,
    unsigned length, uint8_t *result, unsigned *result_length)
{
    if (!pins_owned) return "Preferences link is not open.";
    if (length > PREFS_PAYLOAD_SIZE) return "Request too large.";
    uint8_t request[PREFS_FRAME_SIZE], response[PREFS_FRAME_SIZE];
    uint16_t id = ++sequence;
    prefs_frame(request, command, id, 0, payload, length);
    for (unsigned attempt = 0; attempt < 2; attempt++) {
        if (attempt) {
            pins(false, true);
            Delay(35); // >= 583 ms (NTSC), beyond Pico's partial-frame timeout.
        }
        if (!exchange(request, response) || !prefs_frame_valid(response) ||
            response[3] != (command | 0x80) || response[4] != (uint8_t)id || response[5] != (uint8_t)(id >> 8))
            continue;
        if (response[7] == PREFS_BAD_SETTINGS) return "Firmware rejected these settings.";
        if (response[7] == PREFS_SAVE_FAILED) return "Flash save failed. Previous saved settings remain.";
        if (response[7] != PREFS_OK) return "Firmware rejected the request/version.";
        if (result && response[6]) memcpy(result, response + 8, response[6]);
        if (result_length) *result_length = response[6];
        return NULL;
    }
    return "No valid reply. Enable Amiga preferences on the Pico; check port 2 cable.";
}
