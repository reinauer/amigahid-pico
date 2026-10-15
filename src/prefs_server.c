/* Preferences operations run only in core0's main loop, never in a GPIO IRQ.
 * SPDX-License-Identifier: EPL-2.0 */
#include "prefs_server.h"
#include "platform/amiga/prefs_link.h"
#include "runtime_menu.h"
#include "settings.h"
#include "config.h"
#include "firmware_version.h"
#include <string.h>
_Static_assert(sizeof(settings_t) == PREFS_SETTINGS_SIZE, "Update preferences wire schema");
static bool cached;
static uint8_t previous[PREFS_FRAME_SIZE], response[PREFS_FRAME_SIZE];
void prefs_server_begin(void)
{
    cached = false;
    prefs_link_begin();
}
void prefs_server_task(void)
{
    prefs_packet_t packet;
    if (!prefs_link_receive(&packet)) return;
    if (!prefs_link_current(&packet)) return;
    uint8_t *request = packet.bytes;
    if (cached && memcmp(request, previous, sizeof(previous)) == 0) {
        memcpy(packet.bytes, response, sizeof(response));
        prefs_link_reply(&packet);
        return;
    }
    uint8_t payload[PREFS_PAYLOAD_SIZE] = {0};
    unsigned length = 0;
    uint8_t status = PREFS_OK, command = request[3];
    uint16_t sequence = request[4] | (uint16_t)request[5] << 8;
    settings_t candidate;
    if (!prefs_frame_valid(request) || request[7]) status = PREFS_BAD_FRAME;
    else if ((command == PREFS_USE || command == PREFS_SAVE) ? request[6] != sizeof(candidate) : request[6] != 0)
        status = PREFS_BAD_FRAME;
    else switch (command) {
        case PREFS_HELLO:
            payload[0] = PREFS_SETTINGS_VERSION;
            payload[1] = PREFS_SETTINGS_SIZE;
#ifdef ENABLE_BLUETOOTH_HID
            payload[2] |= PREFS_CAP_BLUETOOTH;
#endif
#ifdef HAS_JOYSTICK_PORT2
            payload[2] |= PREFS_CAP_PORT2;
#endif
            strncpy((char *)payload + 3, AMIGAHID_FIRMWARE_VERSION, sizeof(payload) - 4);
            length = 3 + strlen((char *)payload + 3) + 1;
            break;
        case PREFS_READ:
            memcpy(payload, settings_get(), sizeof(candidate));
            length = sizeof(candidate);
            break;
        case PREFS_DEFAULTS:
            settings_defaults(&candidate);
            memcpy(payload, &candidate, sizeof(candidate));
            length = sizeof(candidate);
            break;
        case PREFS_USE:
        case PREFS_SAVE:
            memcpy(&candidate, request + 8, sizeof(candidate));
            if (!settings_valid(&candidate)) status = PREFS_BAD_SETTINGS;
            else if (!runtime_menu_apply_remote(&candidate, command == PREFS_SAVE))
                status = PREFS_SAVE_FAILED;
            break;
        case PREFS_END: break;
        default: status = PREFS_BAD_COMMAND; break;
    }
    memcpy(previous, request, sizeof(previous));
    prefs_frame(response, command | 0x80, sequence, status, payload, length);
    cached = true;
    memcpy(packet.bytes, response, sizeof(response));
    prefs_link_reply(&packet);
}
