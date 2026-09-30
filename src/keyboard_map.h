/* Runtime keyboard mapping. SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_KEYBOARD_MAP_H
#define AMIGAHID_KEYBOARD_MAP_H
#include <stdint.h>
uint8_t keyboard_map_key(uint8_t hid);
uint8_t keyboard_map_modifier(unsigned bit);
char const *keyboard_map_name(uint8_t amiga);
char const *keyboard_map_source_name(uint8_t hid);
#endif
