/* Explicitly armed port-2 preferences transport. SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_PREFS_LINK_H
#define AMIGAHID_PREFS_LINK_H
#include <stdbool.h>
#include <stdint.h>
#include "prefs_protocol.h"
typedef struct { uint32_t generation; uint8_t bytes[PREFS_FRAME_SIZE]; } prefs_packet_t;
/* Init/begin/end and mailbox access: core0. Tick/owns_port: core1. */
void prefs_link_init(void);
void prefs_link_begin(void);
void prefs_link_end(void);
bool prefs_link_active(void);
bool prefs_link_current(prefs_packet_t const *packet);
bool prefs_link_receive(prefs_packet_t *packet);
void prefs_link_reply(prefs_packet_t const *packet);
bool prefs_link_tick(void);
#endif
