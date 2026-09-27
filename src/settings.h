/* Runtime settings for AmigaHID-Pico. SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_SETTINGS_H
#define AMIGAHID_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

enum settings_menu_key { SETTINGS_MENU_F12, SETTINGS_MENU_F11, SETTINGS_MENU_APPLICATION, SETTINGS_MENU_KEY_COUNT };
enum settings_menu_entry { SETTINGS_MENU_HOLD, SETTINGS_MENU_BOOT, SETTINGS_MENU_ENTRY_COUNT };
enum settings_right_gui { SETTINGS_GUI_AMIGA, SETTINGS_GUI_MENU, SETTINGS_GUI_OFF, SETTINGS_GUI_COUNT };
enum settings_display { SETTINGS_DISPLAY_STATUS, SETTINGS_DISPLAY_HID, SETTINGS_DISPLAY_MOUSE, SETTINGS_DISPLAY_OFF, SETTINGS_DISPLAY_COUNT };

/* Persist fixed-width fields, never compiler-dependent enums or pointers.
 * Changing this layout requires a new on-flash schema version. */
typedef struct {
    uint8_t menu_key;
    uint8_t menu_entry;
    uint8_t right_gui;
    uint8_t wheel_enabled;
    uint8_t wheel_reverse;
    uint8_t mouse_speed;      /* step interval: 300, 200, 150, 100 us */
    uint8_t display;
    uint8_t watchdog;         /* off, 2 seconds, 5 seconds */
} settings_t;

void settings_defaults(settings_t *settings);
bool settings_valid(settings_t const *settings);
void settings_init(void);
settings_t const *settings_get(void);
bool settings_set(settings_t const *settings);
bool settings_save(settings_t const *settings);
uint16_t settings_mouse_interval_us(void);
uint32_t settings_watchdog_ms(void);
uint8_t settings_menu_hid_key(void);

#endif
