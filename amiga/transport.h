/* SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_AMIGA_TRANSPORT_H
#define AMIGAHID_AMIGA_TRANSPORT_H
#include <stdbool.h>
#include <stdint.h>
/* Physical Amiga port 2 (gameport.device unit 1). */
char const *transport_open(void);
void transport_close(void);
char const *transport_command(uint8_t command, uint8_t const *payload,
    unsigned length, uint8_t *result, unsigned *result_length);
#endif
