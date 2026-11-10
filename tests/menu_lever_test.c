/* Contact bounce and gesture tests. SPDX-License-Identifier: EPL-2.0 */
#include <assert.h>
#include <stdio.h>
#include "menu_lever.h"

int main(void)
{
    menu_lever_t l;
    menu_lever_init(&l, MENU_LEVER_PUSH, 0);
    assert(menu_lever_update(&l, MENU_LEVER_PUSH, 2000000) == MENU_LEVER_NONE);
    assert(menu_lever_update(&l, 0, 2010000) == MENU_LEVER_NONE);
    assert(menu_lever_update(&l, 0, 2030000) == MENU_LEVER_NONE);
    /* Contact bounce must not generate a click or navigation. */
    assert(menu_lever_update(&l, MENU_LEVER_LEFT, 2040000) == MENU_LEVER_NONE);
    assert(menu_lever_update(&l, 0, 2045000) == MENU_LEVER_NONE);
    assert(menu_lever_update(&l, MENU_LEVER_LEFT, 2050000) == MENU_LEVER_NONE);
    assert(menu_lever_update(&l, MENU_LEVER_LEFT, 2069999) == MENU_LEVER_NONE);
    assert(menu_lever_update(&l, MENU_LEVER_LEFT, 2070000) == MENU_LEVER_PREVIOUS);
    assert(menu_lever_update(&l, MENU_LEVER_LEFT, 2519999) == MENU_LEVER_NONE);
    assert(menu_lever_update(&l, MENU_LEVER_LEFT, 2520000) == MENU_LEVER_PREVIOUS);
    assert(menu_lever_update(&l, MENU_LEVER_LEFT, 4000000) == MENU_LEVER_PREVIOUS);
    assert(menu_lever_update(&l, MENU_LEVER_LEFT, 4000001) == MENU_LEVER_NONE);
    /* Suppress ambiguous simultaneous contacts until neutral. */
    menu_lever_update(&l, MENU_LEVER_LEFT | MENU_LEVER_PUSH, 4010000);
    assert(menu_lever_update(&l, MENU_LEVER_LEFT | MENU_LEVER_PUSH, 4030000) == MENU_LEVER_NONE);
    menu_lever_update(&l, MENU_LEVER_PUSH, 4040000);
    assert(menu_lever_update(&l, MENU_LEVER_PUSH, 6040000) == MENU_LEVER_NONE);
    menu_lever_update(&l, 0, 6050000);
    assert(menu_lever_update(&l, 0, 6070000) == MENU_LEVER_NONE);
    menu_lever_update(&l, MENU_LEVER_PUSH, 6080000);
    menu_lever_update(&l, MENU_LEVER_PUSH, 6100000);
    assert(menu_lever_update(&l, MENU_LEVER_PUSH, 7099999) == MENU_LEVER_NONE);
    assert(menu_lever_update(&l, MENU_LEVER_PUSH, 7100000) == MENU_LEVER_HOLD);
    assert(menu_lever_update(&l, MENU_LEVER_PUSH, 9000000) == MENU_LEVER_NONE);
    menu_lever_update(&l, 0, 9010000);
    assert(menu_lever_update(&l, 0, 9030000) == MENU_LEVER_NONE);
    /* A delayed poll at release must still classify a long push as a hold. */
    menu_lever_update(&l, MENU_LEVER_PUSH, 9040000);
    menu_lever_update(&l, MENU_LEVER_PUSH, 9060000);
    menu_lever_update(&l, 0, 11060000);
    assert(menu_lever_update(&l, 0, 11080000) == MENU_LEVER_HOLD);
    menu_lever_update(&l, MENU_LEVER_PUSH, 11100000);
    menu_lever_update(&l, MENU_LEVER_PUSH, 11120000);
    menu_lever_update(&l, 0, 11200000);
    assert(menu_lever_update(&l, 0, 11220000) == MENU_LEVER_CLICK);
    assert(menu_lever_update(&l, 0, 11240000) == MENU_LEVER_NONE);
    /* Time comparisons remain correct beyond the 32-bit microsecond wrap. */
    uint64_t late = UINT64_C(1) << 32;
    menu_lever_init(&l, 0, late);
    menu_lever_update(&l, 0, late + 20000);
    menu_lever_update(&l, MENU_LEVER_RIGHT, late + 30000);
    assert(menu_lever_update(&l, MENU_LEVER_RIGHT, late + 50000) == MENU_LEVER_NEXT);
    puts("menu lever timing: PASS");
}
