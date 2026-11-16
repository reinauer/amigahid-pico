/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * main entry point for amigahid-pico.
 */

// these reside within the tinyusb sdk and are not part of this project source
#include "bsp/board.h"
#include "tusb.h"

#include "display/disp_ssd.h"
#include "platform/amiga/keyboard_serial_io.h"
#include "platform/amiga/quad_mouse.h"
#include "util/debug_cons.h"
#include "util/output.h"

#include "config.h"
#include "tusb_config.h"
#include "settings.h"
#include "runtime_menu.h"
#include "usb_hid.h"

#ifdef ENABLE_BLUETOOTH_HID
#include "bt_hid.h"
#endif

// main entry point
int main(void)
{
    // tinyusb board init; led, uart, button, usb
    board_init();
    settings_init();

    // Release the Amiga-facing pins before bringing up input devices.
    // The keyboard power-up handshake advances later in amiga_service().
    amiga_init();
    amiga_quad_mouse_init();

    // initialise the usb host stack on the rhport from tusb_config.h
    const tusb_rhport_init_t rhport_init = {
        .role = TUSB_ROLE_HOST,
        .speed = TUH_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL,
    };
    bool usb_host_ready = tusb_init(BOARD_TUH_RHPORT, &rhport_init);

    disp_ssd_init();
    dbgcons_init();
    if (!usb_host_ready)
        ahprintf("[usb] host initialization failed on rhport %d; USB input disabled\n",
            BOARD_TUH_RHPORT);

#ifdef ENABLE_BLUETOOTH_HID
    bt_hid_init();
#endif
    runtime_menu_init();

    // Input callbacks can use the display, menu and routing from this point.
    while (1) {
        // run host mode jobs (hotplug events, packet io callbacks)
        if (usb_host_ready) {
            tuh_task();
            hid_app_task();
        }

#ifdef ENABLE_BLUETOOTH_HID
        bt_hid_task();
#endif

        // amiga keyboard service routine
        amiga_service();
        runtime_menu_task();

        // Render deferred Bluetooth status and reclaim completed OLED DMA buffers.
        dbgcons_task();
        disp_ssd_task();
        runtime_menu_watchdog_task();
    }

    return 0;
}
