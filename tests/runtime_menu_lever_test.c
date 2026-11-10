/* Exercise the real menu with simulated GPIO/time and device boundaries.
 * SPDX-License-Identifier: EPL-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/runtime_menu.c"

static uint64_t now;
static uint8_t contacts;
static settings_t active;
static bool display_ok = true, captured, overlay, save_ok = true, link;
static unsigned saves, initialized;
#ifdef ENABLE_BLUETOOTH_HID
static unsigned pairs, forgets;
#endif
static char shown_hint[64];
uint64_t time_us_64(void) { return now; }
void gpio_init(uint p) { assert(p >= 14 && p <= 16); initialized |= 1u << p; }
void gpio_set_dir(uint p, bool out) { assert(p >= 14 && p <= 16 && !out); }
void gpio_pull_up(uint p) { assert(p >= 14 && p <= 16); }
bool gpio_get(uint p) { assert(p >= 14 && p <= 16); return !(contacts & (1u << (p - 14))); }
void watchdog_disable(void) {}
void watchdog_enable(uint32_t ms, bool debug) { (void)ms; (void)debug; }
void watchdog_update(void) {}
bool watchdog_caused_reboot(void) { return false; }
void ahprintf(char const *fmt, ...) { (void)fmt; }
void (*disp_write)(uint8_t, uint8_t, char *);
bool disp_ssd_available(void) { return display_ok; }
void disp_ssd_set_enabled(bool on) { (void)on; }
void disp_ssd_set_overlay(bool on) { overlay = on; }
void disp_ssd_menu(char const *h, char const *i, char const *v, char const *hint)
{ (void)h; (void)i; (void)v; snprintf(shown_hint, sizeof(shown_hint), "%s", hint); }
void input_bridge_capture(bool on) { captured = on; }
void input_bridge_set_port_modes(uint8_t a, uint8_t b) { (void)a; (void)b; }
void amiga_quad_mouse_configure(uint16_t a, bool b, bool c) { (void)a; (void)b; (void)c; }
void dbgcons_settings_changed(void) {}
settings_t const *settings_get(void) { return &active; }
bool settings_set(settings_t const *s) { active = *s; return true; }
bool settings_save(settings_t const *s) { (void)s; saves++; return save_ok; }
void settings_defaults(settings_t *s) { memset(s, 0, sizeof(*s)); s->bluetooth_enabled = 1; }
bool settings_valid(settings_t const *s) { (void)s; return true; }
uint16_t settings_mouse_interval_us(void) { return 300; }
uint32_t settings_watchdog_ms(void) { return 0; }
uint8_t settings_menu_hid_key(void) { return HID_KEY_F12; }
void prefs_link_end(void) { link = false; }
bool prefs_link_active(void) { return link; }
void prefs_server_begin(void) { link = true; }
void prefs_server_task(void) {}
char const *keyboard_map_source_name(uint8_t h) { (void)h; return "source"; }
char const *keyboard_map_name(uint8_t h) { (void)h; return "target"; }
#ifdef ENABLE_BLUETOOTH_HID
void bt_hid_pair(void) { pairs++; }
void bt_hid_forget(void) { forgets++; }
#endif

static void poll(uint8_t mask, uint64_t elapsed)
{ contacts = mask; now += elapsed; runtime_menu_task(); }
static void settle(uint8_t mask) { poll(mask, 0); poll(mask, 20000); }
static void tap(void) { settle(MENU_LEVER_PUSH); poll(MENU_LEVER_PUSH, 50000); settle(0); }
static void hold(void) { settle(MENU_LEVER_PUSH); poll(MENU_LEVER_PUSH, 1000000); settle(0); }
static void rock(uint8_t mask) { settle(mask); settle(0); }
static void select_item(unsigned target)
{
    unsigned tries = 0;
    while (item != target) { assert(++tries <= MENU_ITEM_COUNT); rock(MENU_LEVER_RIGHT); }
}

int main(void)
{
    settings_defaults(&active);
    active.menu_entry = SETTINGS_MENU_BOOT;
    active.display = SETTINGS_DISPLAY_OFF;
    contacts = MENU_LEVER_PUSH;
    runtime_menu_init();
    assert(initialized == ((1u << 14) | (1u << 15) | (1u << 16)));
    poll(MENU_LEVER_PUSH, 2000000);
    assert(!opened && !captured);
    settle(0);
    now = boot_deadline + 1;
    hold();
    assert(opened && captured && overlay && item == MENU_KEY && !lever_editing);
    tap();
    assert(lever_editing && strstr(shown_hint, "+/-"));
    rock(MENU_LEVER_RIGHT);
    assert(edited.menu_key == SETTINGS_MENU_F11 && active.menu_key == SETTINGS_MENU_F12);
    tap();
    assert(!lever_editing);
    hold();
    assert(!opened && !captured && !overlay && !saves);
    hold();
    tap(); rock(MENU_LEVER_RIGHT); tap();
    select_item(MENU_SAVE);
    save_ok = false; tap();
    assert(opened && saves == 1 && active.menu_key == SETTINGS_MENU_F12);
    save_ok = true; tap();
    assert(!opened && saves == 2 && active.menu_key == SETTINGS_MENU_F11);
    hold(); select_item(MENU_REMAP_SOURCE); tap();
    assert(remap_learning);
    rock(MENU_LEVER_RIGHT);
    assert(remap_learning && edited.keymap[0].source == 0);
    tap();
    assert(!remap_learning && opened);
    tap();
    hid_keyboard_report_t r = {.keycode = {4}};
    assert(runtime_menu_keyboard(0, &r));
    assert(!remap_learning && edited.keymap[0].source == 4 && item == MENU_REMAP_TARGET);
    tap(); assert(lever_editing);
    r.keycode[0] = 5; /* An unrelated key still leaves the hint in browse mode. */
    runtime_menu_keyboard(0, &r);
    poll(0, 20000);
    assert(!lever_editing && strstr(shown_hint, "browse"));
    tap(); assert(lever_editing);
    r.keycode[0] = HID_KEY_ARROW_DOWN;
    runtime_menu_keyboard(0, &r);
    assert(!lever_editing && item == MENU_REMAP_CLEAR);
    hold();
    assert(!opened && quarantine[0]);
    r = (hid_keyboard_report_t){0};
    assert(runtime_menu_keyboard(0, &r));
    assert(!quarantine[0]);
    display_ok = false; hold();
    assert(!opened && !captured);
    display_ok = true; hold();
    assert(opened);
#ifdef ENABLE_BLUETOOTH_HID
    select_item(MENU_BT_FORGET); tap();
    assert(forget_confirm && !forgets);
    tap(); assert(!forget_confirm && forgets == 1);
    select_item(MENU_BT_PAIR); tap(); assert(pairs == 1);
#endif
    select_item(MENU_AMIGA_PREFS); tap();
    assert(!opened && remote_overlay && link);
    hold();
    assert(opened && !remote_overlay && !link);
    display_ok = false; poll(0, 20000);
    assert(!opened && !captured && !overlay);
    puts("runtime menu lever integration: PASS");
}
