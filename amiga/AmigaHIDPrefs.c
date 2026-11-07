/* Native Workbench preferences editor. SPDX-License-Identifier: EPL-2.0 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/dos.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "settings.h"
#include "prefs_protocol.h"
#include "transport.h"

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
static char const version[] __attribute__((used)) = "$VER: AmigaHIDPrefs 0.1 (30.9.2026)";
static struct TextAttr font = {(STRPTR)"topaz.font", 8, 0, 0};
static struct TextFont *text_font;
static struct Screen *screen;
static struct Window *window;
static APTR visual;
static struct Gadget *gadgets, *controls[8];
static settings_t edited;
static bool loaded, linked, done;
static uint8_t capabilities;
static unsigned group, field;
static char status_line[150] = "Enable Amiga preferences in the Pico menu, then click Read.";
static char firmware[60] = "AmigaHID-Pico preferences - REV5 / port 2";

static char *groups[] = {"General", "Mouse", "Keyboard", "Gamepad", "Bluetooth", NULL};
static char *off_on[] = {"Off", "On", NULL};
static char *menu_keys[] = {"F12", "F11", "Application/Menu", NULL};
static char *menu_entry[] = {"Hold for one second", "Boot only", NULL};
static char *gui_keys[] = {"Right Amiga", "Hold to open OLED menu", "Disabled", NULL};
static char *displays[] = {"Status", "HID diagnostics", "Mouse diagnostics", "Off", NULL};
static char *watchdogs[] = {"Off", "2 seconds", "5 seconds", NULL};
static char *port1[] = {"Mouse", "Joystick", "CD32 pad", NULL};
static char *port2[] = {"Off", "Joystick", "CD32 pad", NULL};
static char *speeds[] = {"300 us (default)", "200 us", "150 us", "100 us", NULL};
static char *directions[] = {"Normal", "Reverse vertical", NULL};
static char *presets[] = {"Original", "Y/Z swap (QWERTZ letters)", "A/Q and W/Z swap (AZERTY letters)", NULL};
static char *helps[] = {"Insert", "Home", "Page Up", "F11", "None", NULL};
static char *deletes[] = {"Delete", "Backspace", "None", NULL};
static char *iso[] = {"Backslash (original)", "Amiga extra key beside left Shift", NULL};
static char *sources[] = {"D-pad and stick", "D-pad only", "Stick only", NULL};
static char *zones[] = {"25% (sensitive)", "50% (default)", "75% (wide)", NULL};
static char *buttons[] = {"Auto (controller default)", "HID button 1", "HID button 2", "HID button 3",
    "HID button 4", "HID button 5", "HID button 6", "HID button 7", "HID button 8",
    "HID button 9", "HID button 10", "HID button 11", "HID button 12", "HID button 13",
    "HID button 14", "HID button 15", "HID button 16", "Disabled", NULL};
static char *ps4mouse[] = {"Touchpad only", "Touchpad and shoulders", "Off", NULL};
static char *pairing[] = {"Automatic", "Paired devices only", NULL};
struct field { unsigned group; char *name; size_t offset; char **labels; };
#define FIELD(group, name, member, labels) {group, name, offsetof(settings_t, member), labels}
static struct field const fields[] = {
    FIELD(0, "Menu key", menu_key, menu_keys),
    FIELD(0, "Menu entry", menu_entry, menu_entry),
    FIELD(0, "Right GUI key", right_gui, gui_keys),
    FIELD(0, "Display", display, displays),
    FIELD(0, "Watchdog", watchdog, watchdogs),
    FIELD(0, "Controller port 1", port_mode, port1),
    FIELD(0, "Controller port 2", joystick_port2, port2),
    FIELD(1, "Mouse wheel", wheel_enabled, off_on),
    FIELD(1, "Wheel direction", wheel_reverse, directions),
    FIELD(1, "Mouse step interval", mouse_speed, speeds),
    FIELD(2, "Letter preset", keyboard_layout, presets),
    FIELD(2, "Amiga Help key", keyboard_help, helps),
    FIELD(2, "Amiga Delete key", keyboard_delete, deletes),
    FIELD(2, "Extra ISO key", keyboard_iso, iso),
    FIELD(3, "Directions", gamepad_directions, sources),
    FIELD(3, "Stick deadzone", gamepad_deadzone, zones),
    FIELD(3, "Fire 1 / CD32 Red", gamepad_buttons[0], buttons),
    FIELD(3, "Fire 2 / CD32 Blue", gamepad_buttons[1], buttons),
    FIELD(3, "Fire 3 / CD32 Green", gamepad_buttons[2], buttons),
    FIELD(3, "CD32 Yellow", gamepad_buttons[3], buttons),
    FIELD(3, "CD32 Rewind", gamepad_buttons[4], buttons),
    FIELD(3, "CD32 Forward", gamepad_buttons[5], buttons),
    FIELD(3, "CD32 Play", gamepad_buttons[6], buttons),
    FIELD(3, "PS4 mouse controls", ds4_mouse, ps4mouse),
    FIELD(4, "Bluetooth radio", bluetooth_enabled, off_on),
    FIELD(4, "Pairing policy", bluetooth_pairing, pairing),
};
#define FIELD_COUNT (sizeof(fields) / sizeof(fields[0]))
static char *field_labels[FIELD_COUNT + 1];
static unsigned field_indices[FIELD_COUNT], field_count;

static unsigned choices(char **labels)
{
    unsigned n = 0;
    while (labels[n]) n++;
    return n;
}
static uint8_t *value(void) { return (uint8_t *)&edited + fields[field_indices[field]].offset; }
static void text_line(int x, int y, char const *text, unsigned length)
{
    Move(window->RPort, window->BorderLeft + x, window->BorderTop + y);
    Text(window->RPort, text, length);
}
static void draw_status(void)
{
    SetAPen(window->RPort, 0);
    RectFill(window->RPort, window->BorderLeft + 8, window->BorderTop + 2,
        window->Width - window->BorderRight - 8, window->BorderTop + 16);
    RectFill(window->RPort, window->BorderLeft + 8, window->BorderTop + 99,
        window->Width - window->BorderRight - 8, window->BorderTop + 124);
    SetAPen(window->RPort, 1);
    text_line(10, 12, firmware, strlen(firmware));
    unsigned length = strlen(status_line), first = length > 68 ? 68 : length;
    if (length > 68) while (first && status_line[first] != ' ') first--;
    if (!first) first = 68;
    text_line(10, 109, status_line, first);
    if (length > first) {
        unsigned start = first + (status_line[first] == ' ');
        text_line(10, 120, status_line + start, length - start > 68 ? 68 : length - start);
    }
}
static void status(char const *message)
{
    snprintf(status_line, sizeof(status_line), "%s", message);
    draw_status();
}
static void select_field(void)
{
    struct field const *f = &fields[field_indices[field]];
    GT_SetGadgetAttrs(controls[2], window, NULL, GTCY_Labels, f->labels,
        GTCY_Active, loaded ? *value() : 0,
        GA_Disabled, !loaded || (group == 4 && !(capabilities & PREFS_CAP_BLUETOOTH)), TAG_DONE);
}
static void select_group(void)
{
    field_count = 0;
    for (unsigned i = 0; i < FIELD_COUNT; i++) if (fields[i].group == group) {
        field_indices[field_count] = i;
        field_labels[field_count++] = fields[i].name;
    }
    field_labels[field_count] = NULL;
    field = 0;
    GT_SetGadgetAttrs(controls[1], window, NULL, GTCY_Labels, field_labels, GTCY_Active, 0, TAG_DONE);
    select_field();
}
static char const *read_settings(uint8_t command)
{
    uint8_t payload[PREFS_PAYLOAD_SIZE];
    unsigned length;
    char const *error = transport_command(command, NULL, 0, payload, &length);
    if (error) return error;
    if (length != sizeof(edited)) return "Settings format differs; update this utility.";
    /* Validate every editable choice before using it as a gadget index. Leave
     * custom keymaps/reserved bytes intact; firmware validates the full record. */
    for (unsigned i = 0; i < FIELD_COUNT; i++)
        if (payload[fields[i].offset] >= choices(fields[i].labels)) return "Unrecognized setting value.";
    memcpy(&edited, payload, sizeof(edited));
    loaded = true;
    return NULL;
}
static void connect_read(void)
{
    status("Reading Pico settings...");
    loaded = linked = false;
    char const *error = transport_open();
    uint8_t payload[PREFS_PAYLOAD_SIZE];
    unsigned length;
    if (!error) error = transport_command(PREFS_HELLO, NULL, 0, payload, &length);
    if (!error) {
        if (length < 4 || payload[0] != PREFS_SETTINGS_VERSION || payload[1] != sizeof(edited) ||
            !(payload[2] & PREFS_CAP_PORT2) || !memchr(payload + 3, 0, length - 3))
            error = "Incompatible firmware. Install the matching REV5 image.";
        else {
            capabilities = payload[2];
            snprintf(firmware, sizeof(firmware), "Firmware: %.48s", (char *)payload + 3);
            linked = true;
            error = read_settings(PREFS_READ);
        }
    }
    select_field();
    status(error ? error : "Settings loaded. Use applies temporarily; Save keeps them after power-off.");
}
static void action(unsigned id)
{
    char const *error = NULL;
    if (id == 3) { connect_read(); return; }
    if (id == 7) { done = true; return; }
    if (!loaded) { status("Click Read to load the Pico settings first."); return; }
    if (id == 4) {
        status("Reading factory defaults...");
        error = read_settings(PREFS_DEFAULTS);
        select_field();
        status(error ? error : "Defaults loaded into the editor. Click Use or Save to apply them.");
    } else if (id == 5 || id == 6) {
        status(id == 6 ? "Saving settings to Pico flash..." : "Applying settings...");
        error = transport_command(id == 6 ? PREFS_SAVE : PREFS_USE, (uint8_t const *)&edited, sizeof(edited), NULL, NULL);
        status(error ? error : id == 6 ? "Settings saved to Pico flash." : "Settings in use until power-off. Click Save to keep them.");
    }
}
static void key(unsigned char key)
{
    if (key == 27) { done = true; return; }
    if (key == 'r' || key == 'R') action(3);
    else if (key == 'd' || key == 'D') action(4);
    else if (key == 'u' || key == 'U') action(5);
    else if (key == 's' || key == 'S') action(6);
    else if (key == 'g' || key == 'G') {
        group = (group + 1) % 5;
        GT_SetGadgetAttrs(controls[0], window, NULL, GTCY_Active, group, TAG_DONE);
        select_group();
    } else if (key == 'n' || key == 'N') {
        field = (field + 1) % field_count;
        GT_SetGadgetAttrs(controls[1], window, NULL, GTCY_Active, field, TAG_DONE);
        select_field();
    } else if (loaded && (key == '+' || key == '-' || key == '=')) {
        if (group == 4 && !(capabilities & PREFS_CAP_BLUETOOTH)) return;
        unsigned count = choices(fields[field_indices[field]].labels);
        *value() = (*value() + count + (key == '-' ? -1 : 1)) % count;
        select_field();
    }
}
static bool open_window(void)
{
    screen = LockPubScreen(NULL);
    if (!screen) return false;
    visual = GetVisualInfo(screen, TAG_DONE);
    if (!visual) return false;
    text_font = OpenFont(&font);
    if (!text_font) return false;
    struct NewGadget ng = {0};
    struct Gadget *last = CreateContext(&gadgets);
    ng.ng_TextAttr = &font;
    ng.ng_VisualInfo = visual;
    ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_LeftEdge = screen->WBorLeft + 90;
    int top = screen->WBorTop + screen->Font->ta_YSize + 1;
    ng.ng_Width = 455; ng.ng_Height = 20;
    static char *names[] = {"Group", "Setting", "Value"};
    field_labels[0] = fields[0].name;
    for (unsigned i = 0; i < 3; i++) {
        ng.ng_TopEdge = top + 24 + i * 25;
        ng.ng_GadgetID = i;
        ng.ng_GadgetText = (STRPTR)names[i];
        controls[i] = last = CreateGadget(CYCLE_KIND, last, &ng,
            GTCY_Labels, i == 0 ? groups : i == 1 ? field_labels : menu_keys, GA_Disabled, i == 2, TAG_DONE);
        if (!last) return false;
    }
    static char *labels[] = {"Read", "Defaults", "Use", "Save", "Cancel"};
    ng.ng_Flags = PLACETEXT_IN;
    ng.ng_TopEdge = top + 133; ng.ng_Width = 100;
    for (unsigned i = 0; i < 5; i++) {
        ng.ng_LeftEdge = screen->WBorLeft + 10 + i * 111;
        ng.ng_GadgetText = (STRPTR)labels[i]; ng.ng_GadgetID = i + 3;
        controls[i + 3] = last = CreateGadget(BUTTON_KIND, last, &ng, TAG_DONE);
        if (!last) return false;
    }
    window = OpenWindowTags(NULL, WA_PubScreen, screen, WA_Title, "AmigaHID Preferences",
        WA_InnerWidth, 565, WA_InnerHeight, 164, WA_Gadgets, gadgets,
        WA_Flags, WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_VANILLAKEY | IDCMP_REFRESHWINDOW, TAG_DONE);
    if (!window) return false;
    SetFont(window->RPort, text_font);
    GT_RefreshWindow(window, NULL);
    select_group();
    draw_status();
    return true;
}
int main(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37);
    GadToolsBase = OpenLibrary("gadtools.library", 37);
    bool ready = IntuitionBase && GfxBase && GadToolsBase && open_window();
    if (ready) while (!done) {
        ULONG signals = Wait((1ul << window->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);
        if (signals & SIGBREAKF_CTRL_C) done = true;
        struct IntuiMessage *message;
        while ((message = GT_GetIMsg(window->UserPort))) {
            ULONG cls = message->Class;
            UWORD code = message->Code;
            unsigned id = cls == IDCMP_GADGETUP ? ((struct Gadget *)message->IAddress)->GadgetID : 0;
            GT_ReplyIMsg(message);
            if (cls == IDCMP_CLOSEWINDOW) done = true;
            else if (cls == IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(window); draw_status(); GT_EndRefresh(window, TRUE);
            } else if (cls == IDCMP_VANILLAKEY) key(code);
            else if (cls == IDCMP_GADGETUP) {
                if (id == 0) { group = code; select_group(); }
                else if (id == 1 && code < field_count) { field = code; select_field(); }
                else if (id == 2 && loaded && code < choices(fields[field_indices[field]].labels)) *value() = code;
                else if (id >= 3) action(id);
            }
        }
    }
    if (linked) transport_command(PREFS_END, NULL, 0, NULL, NULL);
    transport_close();
    if (window) CloseWindow(window);
    if (gadgets) FreeGadgets(gadgets);
    if (text_font) CloseFont(text_font);
    if (visual) FreeVisualInfo(visual);
    if (screen) UnlockPubScreen(NULL, screen);
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    return ready ? 0 : 20;
}
