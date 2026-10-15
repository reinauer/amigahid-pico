/* AmigaHID preferences wire format. SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_PREFS_PROTOCOL_H
#define AMIGAHID_PREFS_PROTOCOL_H
#include <stdint.h>
#include <stdbool.h>
#define PREFS_FRAME_SIZE 64u
#define PREFS_PAYLOAD_SIZE 54u
#define PREFS_WIRE_VERSION 1u
#define PREFS_SETTINGS_VERSION 1u
#define PREFS_SETTINGS_SIZE 48u
#define PREFS_CAP_BLUETOOTH 1u
#define PREFS_CAP_PORT2 2u
/* Bytes: 'A','H', version, command, sequence LE16, length, status,
 * 54 zero-padded payload bytes, CRC16-CCITT-FALSE (LE, over bytes 0..61).
 * Responses set command bit 7 and echo the sequence. All requests have status 0.
 * Settings version 1 is the 48 individual bytes of settings_t (no endian fields).
 */
enum prefs_command { PREFS_HELLO = 1, PREFS_READ, PREFS_DEFAULTS,
    PREFS_USE, PREFS_SAVE, PREFS_END };
enum prefs_status { PREFS_OK, PREFS_BAD_FRAME, PREFS_BAD_COMMAND,
    PREFS_BAD_SETTINGS, PREFS_SAVE_FAILED };
uint16_t prefs_crc(uint8_t const *bytes, unsigned length);
void prefs_frame(uint8_t *frame, uint8_t command, uint16_t sequence,
    uint8_t status, uint8_t const *payload, unsigned length);
bool prefs_frame_valid(uint8_t const *frame);
#endif
