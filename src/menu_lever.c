/* Debounced three-contact menu lever. SPDX-License-Identifier: EPL-2.0 */
#include "menu_lever.h"

#define DEBOUNCE_US 20000u
#define HOLD_US 1000000u
#define REPEAT_DELAY_US 450000u
#define REPEAT_US 150000u

void menu_lever_init(menu_lever_t *lever, uint8_t contacts, uint64_t now)
{
    *lever = (menu_lever_t) {
        .raw = contacts, .stable = contacts, .changed_at = now,
    };
}

enum menu_lever_event menu_lever_update(menu_lever_t *lever,
    uint8_t contacts, uint64_t now)
{
    if (contacts != lever->raw) {
        lever->raw = contacts;
        lever->changed_at = now;
    }
    /* Do not emit holds/repeats while a release is being debounced. */
    if (now - lever->changed_at < DEBOUNCE_US)
        return MENU_LEVER_NONE;

    uint8_t previous = lever->stable;
    lever->stable = contacts;
    if (!contacts) {
        bool click = lever->armed && previous == MENU_LEVER_PUSH &&
            !lever->hold_sent;
        bool held = click && lever->changed_at - lever->pressed_at >= HOLD_US;
        lever->armed = true;
        lever->hold_sent = false;
        return held ? MENU_LEVER_HOLD : click ? MENU_LEVER_CLICK : MENU_LEVER_NONE;
    }
    if (contacts != MENU_LEVER_LEFT && contacts != MENU_LEVER_RIGHT &&
        contacts != MENU_LEVER_PUSH) {
        lever->armed = false;
        return MENU_LEVER_NONE;
    }
    if (!lever->armed)
        return MENU_LEVER_NONE;
    if (contacts != previous) {
        /* Changing directly between push and rock is not a click. */
        lever->hold_sent = false;
        lever->pressed_at = now;
        lever->repeat_at = now + REPEAT_DELAY_US;
        if (contacts != MENU_LEVER_PUSH)
            return contacts == MENU_LEVER_LEFT ? MENU_LEVER_PREVIOUS : MENU_LEVER_NEXT;
    }
    if (contacts == MENU_LEVER_PUSH) {
        if (!lever->hold_sent && now - lever->pressed_at >= HOLD_US) {
            lever->hold_sent = true;
            return MENU_LEVER_HOLD;
        }
    } else if (now >= lever->repeat_at) {
        /* One repeat per poll, even after a slow flash or USB operation. */
        lever->repeat_at = now + REPEAT_US;
        return contacts == MENU_LEVER_LEFT ? MENU_LEVER_PREVIOUS : MENU_LEVER_NEXT;
    }
    return MENU_LEVER_NONE;
}
