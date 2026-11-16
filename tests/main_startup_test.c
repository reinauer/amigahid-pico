/* Exercise initialization failure without stopping non-USB services.
 * SPDX-License-Identifier: EPL-2.0 */
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define main firmware_main
#include "../src/main.c"
#undef main
static jmp_buf stop;
static bool host_ok, board, settings, keyboard, mouse, display, debug, menu;
static unsigned usb_calls, hid_calls, keyboard_calls, menu_calls, display_calls;
static unsigned debug_calls, watchdog_calls, bt_calls;
static char diagnostic[160];
void board_init(void) { board = true; }
void settings_init(void) { assert(board); settings = true; }
void amiga_init(void) { assert(settings); keyboard = true; }
void amiga_quad_mouse_init(void) { assert(settings); mouse = true; }
bool tusb_init(uint8_t port, tusb_rhport_init_t const *init)
{
    assert(keyboard && mouse && port == BOARD_TUH_RHPORT);
    assert(init->role == TUSB_ROLE_HOST);
    return host_ok;
}
void disp_ssd_init(void) { display = true; }
void dbgcons_init(void) { assert(display); debug = true; }
void runtime_menu_init(void) { assert(debug); menu = true; }
void ahprintf(char const *fmt, ...)
{
    assert(debug);
    va_list args; va_start(args, fmt);
    vsnprintf(diagnostic, sizeof(diagnostic), fmt, args);
    va_end(args);
}
void tuh_task(void) { assert(host_ok && menu); usb_calls++; }
void hid_app_task(void) { assert(host_ok && menu); hid_calls++; }
void amiga_service(void) { keyboard_calls++; }
void runtime_menu_task(void) { menu_calls++; }
void dbgcons_task(void) { debug_calls++; }
void disp_ssd_task(void) { display_calls++; }
void runtime_menu_watchdog_task(void) { if (++watchdog_calls == 2) longjmp(stop, 1); }
#ifdef ENABLE_BLUETOOTH_HID
void bt_hid_init(void) { assert(debug); }
void bt_hid_task(void) { bt_calls++; }
#endif
static void run(bool ready)
{
    host_ok = ready;
    board = settings = keyboard = mouse = display = debug = menu = false;
    usb_calls = hid_calls = keyboard_calls = menu_calls = display_calls = 0;
    debug_calls = watchdog_calls = bt_calls = 0; diagnostic[0] = 0;
    if (!setjmp(stop)) firmware_main();
    assert(usb_calls == (ready ? 2u : 0u));
    assert(hid_calls == usb_calls);
    assert(keyboard_calls == 2 && menu_calls == 2 && display_calls == 2);
    assert(debug_calls == 2 && watchdog_calls == 2);
#ifdef ENABLE_BLUETOOTH_HID
    assert(bt_calls == 2);
#endif
    if (ready) assert(!diagnostic[0]);
    else assert(strstr(diagnostic, "host initialization failed") &&
                strstr(diagnostic, "USB input disabled"));
}
int main(void)
{
    run(false); run(true);
    puts("main startup and USB failure: PASS");
}
