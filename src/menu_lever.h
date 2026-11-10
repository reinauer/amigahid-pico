/* Debounced three-contact menu lever. SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_MENU_LEVER_H
#define AMIGAHID_MENU_LEVER_H

#include <stdbool.h>
#include <stdint.h>

enum menu_lever_contact {
    MENU_LEVER_LEFT = 1u,
    MENU_LEVER_RIGHT = 2u,
    MENU_LEVER_PUSH = 4u,
};

enum menu_lever_event {
    MENU_LEVER_NONE, MENU_LEVER_PREVIOUS, MENU_LEVER_NEXT,
    MENU_LEVER_CLICK, MENU_LEVER_HOLD,
};

typedef struct {
    uint8_t raw, stable;
    bool armed, hold_sent;
    uint64_t changed_at, pressed_at, repeat_at;
} menu_lever_t;

/* Contacts are active bits here; the GPIO reader converts active-low inputs.
 * Require a neutral position after init or an invalid contact combination. */
void menu_lever_init(menu_lever_t *lever, uint8_t contacts, uint64_t now);
enum menu_lever_event menu_lever_update(menu_lever_t *lever,
    uint8_t contacts, uint64_t now);

#endif
