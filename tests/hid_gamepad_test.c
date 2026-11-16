/* Repeated HID usages must not turn inert axis bytes into held directions.
 * SPDX-License-Identifier: EPL-2.0 */
#include <assert.h>
#include <stdio.h>
#include "hid_gamepad.h"
#include "settings.h"
static settings_t settings = {.gamepad_deadzone = 1};
settings_t const *settings_get(void) { return &settings; }
#define PAD 0x05,0x01,0x09,0x05,0xa1,0x01
#define AXIS_RANGE 0x15,0x00,0x26,0xff,0x00,0x75,0x08
#define INPUT 0x81,0x02
#define END 0xc0
static void decode(hid_gamepad_t *pad, uint8_t const *data, size_t length, uint16_t expected)
{
    uint16_t state = 0xffff;
    assert(hid_gamepad_decode(pad, data, length, &state));
    assert(state == expected);
}
int main(void)
{
    hid_gamepad_t pad;
    uint8_t repeated[] = {PAD, AXIS_RANGE, 0x95,0x05,
        0x09,0x30,0x09,0x30,0x09,0x30,0x09,0x30,0x09,0x31,INPUT,END};
    assert(hid_gamepad_parse(&pad,repeated,sizeof repeated));
    assert(pad.field_count == 2);
    decode(&pad,(uint8_t[]){0,0,0,128,128},5,0);
    decode(&pad,(uint8_t[]){0,0,0,255,128},5,GAMEPAD_RIGHT);
    decode(&pad,(uint8_t[]){255,255,255,0,128},5,GAMEPAD_LEFT);
    // Short reports must still be rejected even though fewer fields are kept.
    uint16_t state = 0;
    assert(!hid_gamepad_decode(&pad,(uint8_t[]){128,128},2,&state));

    uint8_t repeated_y[] = {PAD,AXIS_RANGE,0x95,0x03,
        0x09,0x31,0x09,0x30,0x09,0x31,INPUT,END};
    assert(hid_gamepad_parse(&pad,repeated_y,sizeof repeated_y));
    decode(&pad,(uint8_t[]){0,128,128},3,0);
    decode(&pad,(uint8_t[]){0,128,255},3,GAMEPAD_DOWN);

    uint8_t hats[] = {PAD,0x15,0x00,0x25,0x07,0x75,0x04,0x95,0x02,
        0x09,0x39,0x09,0x39,0x81,0x42,END};
    assert(hid_gamepad_parse(&pad,hats,sizeof hats));
    decode(&pad,(uint8_t[]){0x80},1,0); // Last hat is null, first is inert Up.
    decode(&pad,(uint8_t[]){0x20},1,GAMEPAD_RIGHT);

    // HID repeats the final usage when Report Count exceeds the usage list.
    // Repeated fields must not exhaust the bounded field storage.
    uint8_t implicit[] = {PAD,AXIS_RANGE,0x95,0x28,0x09,0x30,INPUT,END};
    uint8_t report[40] = {0}; report[39] = 128;
    assert(hid_gamepad_parse(&pad,implicit,sizeof implicit));
    assert(pad.field_count == 1);
    decode(&pad,report,sizeof report,0);

    // Independent Input items retain their existing combined-axis behavior.
    uint8_t separate[] = {PAD,AXIS_RANGE,0x95,0x01,
        0x09,0x30,INPUT,0x09,0x30,INPUT,END};
    assert(hid_gamepad_parse(&pad,separate,sizeof separate));
    assert(pad.field_count == 2);
    decode(&pad,(uint8_t[]){0,128},2,GAMEPAD_LEFT);
    decode(&pad,(uint8_t[]){128,255},2,GAMEPAD_RIGHT);

    // Report IDs remain independent, including held state in another report.
    uint8_t ids[] = {PAD,AXIS_RANGE,0x95,0x02,0x85,0x01,
        0x09,0x30,0x09,0x30,INPUT,0x85,0x02,
        0x09,0x31,0x09,0x31,INPUT,END};
    assert(hid_gamepad_parse(&pad,ids,sizeof ids));
    decode(&pad,(uint8_t[]){1,0,128},3,0);
    decode(&pad,(uint8_t[]){2,255,0},3,GAMEPAD_UP);
    decode(&pad,(uint8_t[]){1,0,255},3,GAMEPAD_UP|GAMEPAD_RIGHT);
    decode(&pad,(uint8_t[]){2,255,128},3,GAMEPAD_RIGHT);

    // Ordinary axes and buttons retain their mappings.
    uint8_t standard[] = {PAD,AXIS_RANGE,0x95,0x02,
        0x09,0x30,0x09,0x31,INPUT,0x05,0x09,0x19,0x01,0x29,0x02,
        0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x02,INPUT,
        0x95,0x06,0x81,0x03,END};
    assert(hid_gamepad_parse(&pad,standard,sizeof standard));
    decode(&pad,(uint8_t[]){128,128,0},3,0);
    decode(&pad,(uint8_t[]){255,0,3},3,GAMEPAD_RIGHT|GAMEPAD_UP|GAMEPAD_FIRE|GAMEPAD_FIRE2);
    puts("HID gamepad: PASS");
}
