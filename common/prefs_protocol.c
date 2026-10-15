/* SPDX-License-Identifier: EPL-2.0 */
#include "prefs_protocol.h"
#include <string.h>
uint16_t prefs_crc(uint8_t const *bytes, unsigned length)
{
    uint16_t crc = 0xffff;
    while (length--) {
        crc ^= (uint16_t)*bytes++ << 8;
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    }
    return crc;
}
void prefs_frame(uint8_t *frame, uint8_t command, uint16_t sequence,
    uint8_t status, uint8_t const *payload, unsigned length)
{
    memset(frame, 0, PREFS_FRAME_SIZE);
    frame[0] = 'A'; frame[1] = 'H'; frame[2] = PREFS_WIRE_VERSION;
    frame[3] = command; frame[4] = sequence; frame[5] = sequence >> 8;
    frame[6] = length <= PREFS_PAYLOAD_SIZE ? length : 0;
    frame[7] = status;
    if (frame[6]) memcpy(frame + 8, payload, frame[6]);
    uint16_t crc = prefs_crc(frame, PREFS_FRAME_SIZE - 2);
    frame[62] = crc; frame[63] = crc >> 8;
}
bool prefs_frame_valid(uint8_t const *frame)
{
    if (frame[0] != 'A' || frame[1] != 'H' || frame[2] != PREFS_WIRE_VERSION ||
        frame[6] > PREFS_PAYLOAD_SIZE || prefs_crc(frame, 62) != (uint16_t)(frame[62] | frame[63] << 8))
        return false;
    for (unsigned i = 8 + frame[6]; i < 62; i++)
        if (frame[i]) return false;
    return true;
}
