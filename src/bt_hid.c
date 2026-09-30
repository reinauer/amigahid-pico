/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * bluetooth hid host integration for pico w builds.
 */

#include "bt_hid.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "btstack_config.h"
#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/util/queue.h"
#include "hardware/sync.h"

#include "input_bridge_bt.h"
#include "hid_gamepad.h"
#include "bt_ds4.h"
#include "settings.h"
#include "ble/le_device_db.h"
#include "util/debug_cons.h"
#include "util/output.h"

#define BT_HID_QUEUE_DEPTH 64
#define BT_CLASSIC_DESCRIPTOR_STORAGE_SIZE 2048
#define BT_LE_DESCRIPTOR_STORAGE_SIZE 1024
#define BT_HID_LOCAL_NAME "AmigaHID Pico"

#define HID_USAGE_DESKTOP_X     0x30
#define HID_USAGE_DESKTOP_Y     0x31
#define HID_USAGE_DESKTOP_WHEEL 0x38

typedef enum
{
    BT_HID_QUEUE_DISCONNECT = 0,
    BT_HID_QUEUE_KEYBOARD,
    BT_HID_QUEUE_MOUSE,
    BT_HID_QUEUE_GAMEPAD,
} bt_hid_queue_type_t;

typedef struct
{
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keycode[6];
} bt_hid_keyboard_report_t;

typedef struct
{
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
    int8_t pan;
} bt_hid_mouse_report_t;

typedef struct
{
    uint8_t slot;
    bt_hid_queue_type_t type;
    union
    {
        bt_hid_keyboard_report_t keyboard;
        bt_hid_mouse_report_t mouse;
        uint16_t gamepad;
    };
} bt_hid_queue_entry_t;

typedef struct
{
    bool in_use;
    uint16_t hid_cid;
    hid_protocol_mode_t protocol_mode;
    bd_addr_t address;
    bool descriptor_ready, gamepad_device, has_gamepad, is_ds4;
    hid_gamepad_t gamepad;
    bt_ds4_t ds4;
    bt_hid_gamepad_status_t status;
    btstack_timer_source_t setup_timer;
    uint8_t setup_attempts;
} bt_classic_connection_t;

typedef enum {
    BT_CLASSIC_IDLE,
    BT_CLASSIC_SCANNING,
    BT_CLASSIC_CONNECTING,
} bt_classic_state_t;

typedef enum
{
    BT_LE_STATE_OFF = 0,
    BT_LE_STATE_SCANNING,
    BT_LE_STATE_CONNECTING,
    BT_LE_STATE_ENCRYPTING,
    BT_LE_STATE_SERVICE_QUERY,
    BT_LE_STATE_CHARACTERISTIC_QUERY,
    BT_LE_STATE_ENABLE_KEYBOARD,
    BT_LE_STATE_ENABLE_MOUSE,
    BT_LE_STATE_ENABLE_GAMEPAD,
    BT_LE_STATE_READY,
    BT_LE_STATE_DISCONNECTING,
} bt_le_state_t;

static queue_t bt_hid_queue;
static bool bt_hid_ready;
static bool bt_radio_on, bt_controller_off = true;
static bool bt_pair_request, bt_forget_request, bt_forgetting;
static bool bt_pair_window;
static uint32_t bt_pair_deadline;
static bool bt_resolving;
static bd_addr_t bt_resolve_addr;
static uint8_t bt_resolve_type;

static bool bt_pairing_allowed(void)
{
    return bt_radio_on && (!settings_get()->bluetooth_pairing || bt_pair_window);
}

static bool bt_classic_known(bd_addr_t address)
{
    link_key_t key;
    link_key_type_t type;
    return gap_get_link_key_for_bd_addr(address, key, &type);
}


typedef struct
{
    bool disconnect;
    bool keyboard_pending;
    bool mouse_pending;
    bool gamepad_pending;
    bt_hid_keyboard_report_t keyboard;
    bt_hid_mouse_report_t mouse;
    uint16_t gamepad;
} bt_hid_overflow_t;

static bool bt_hid_queue_resync;
static bt_hid_overflow_t bt_hid_overflow[INPUT_BRIDGE_MAX_SLOTS];

static bt_classic_connection_t bt_classic_connections[INPUT_BRIDGE_BT_CLASSIC_SLOTS];
static uint8_t bt_classic_descriptor_storage[BT_CLASSIC_DESCRIPTOR_STORAGE_SIZE];
static const hid_protocol_mode_t bt_classic_report_mode = HID_PROTOCOL_MODE_REPORT_WITH_FALLBACK_TO_BOOT;
static bt_classic_state_t bt_classic_state;
static bool bt_classic_working, bt_classic_candidate;
static bd_addr_t bt_classic_candidate_addr;
static uint16_t bt_classic_outgoing_cid;
static btstack_timer_source_t bt_classic_scan_timer;

static btstack_packet_callback_registration_t bt_classic_hci_event_callback;
static btstack_packet_callback_registration_t bt_le_hci_event_callback;
static btstack_packet_callback_registration_t bt_le_sm_event_callback;

static bt_le_state_t bt_le_state = BT_LE_STATE_OFF;
static bd_addr_t bt_le_addr;
static bd_addr_type_t bt_le_addr_type;
static hci_con_handle_t bt_le_connection_handle = HCI_CON_HANDLE_INVALID;
static gatt_client_service_t bt_le_hid_service;
static gatt_client_characteristic_t bt_le_protocol_mode_characteristic;
static gatt_client_characteristic_t bt_le_boot_keyboard_characteristic;
static gatt_client_characteristic_t bt_le_boot_mouse_characteristic;
static gatt_client_notification_t bt_le_keyboard_notifications;
static gatt_client_notification_t bt_le_mouse_notifications;
static bool bt_le_has_protocol_mode;
static bool bt_le_has_boot_keyboard;
static bool bt_le_has_boot_mouse;
static uint16_t bt_le_hids_cid;
static uint8_t bt_le_descriptor_storage[BT_LE_DESCRIPTOR_STORAGE_SIZE];
static hid_gamepad_t bt_le_gamepads[MAX_NUM_HID_SERVICES];
static uint16_t bt_le_gamepad_states[MAX_NUM_HID_SERVICES];
static bool bt_le_has_gamepad;
static bt_hid_gamepad_status_t bt_le_gamepad_status;

static uint8_t bt_classic_connected_count(void)
{
    uint8_t count = 0;

    for (uint8_t slot = 0; slot < INPUT_BRIDGE_BT_CLASSIC_SLOTS; slot++) {
        if (bt_classic_connections[slot].in_use)
            count++;
    }

    return count;
}

static char const *bt_le_state_name(bt_le_state_t state)
{
    switch (state) {
        case BT_LE_STATE_OFF:
            return "off";

        case BT_LE_STATE_SCANNING:
            return "scan";

        case BT_LE_STATE_CONNECTING:
            return "conn";

        case BT_LE_STATE_ENCRYPTING:
            return "pair";

        case BT_LE_STATE_SERVICE_QUERY:
            return "svc";

        case BT_LE_STATE_CHARACTERISTIC_QUERY:
            return "char";

        case BT_LE_STATE_ENABLE_KEYBOARD:
            return "kbd";

        case BT_LE_STATE_ENABLE_MOUSE:
            return "mouse";

        case BT_LE_STATE_ENABLE_GAMEPAD:
            return "pad";

        case BT_LE_STATE_READY:
            return "ready";

        case BT_LE_STATE_DISCONNECTING:
            return "disc";
    }

    return "?";
}

static void bt_hid_update_status(void)
{
    if (!bt_radio_on) {
        dbgcons_bt_status("bt off");
        return;
    }
    char linebuf[32] = "";
    unsigned gamepads = bt_le_has_gamepad ? 1 : 0;
    for (unsigned slot = 0; slot < INPUT_BRIDGE_BT_CLASSIC_SLOTS; slot++)
        gamepads += bt_classic_connections[slot].in_use && bt_classic_connections[slot].has_gamepad;
    char const *phase = bt_classic_state == BT_CLASSIC_SCANNING ? ":scan" :
        bt_classic_state == BT_CLASSIC_CONNECTING ? ":conn" : "";
    // Keep the whole line within the OLED's 21 character width.
    if (gamepads)
        snprintf(linebuf, sizeof(linebuf), "bt c%u le:%s j%u",
            bt_classic_connected_count(), bt_le_state_name(bt_le_state), gamepads);
    else
        snprintf(linebuf, sizeof(linebuf), "bt c%u%s le:%s",
            bt_classic_connected_count(), phase, bt_le_state_name(bt_le_state));
    dbgcons_bt_status(linebuf);
}

static void bt_le_set_state(bt_le_state_t state)
{
    bt_le_state = state;
    bt_hid_update_status();
}

static void bt_hid_show_passkey(char const *label, uint32_t passkey)
{
    char linebuf[32] = "";

    snprintf(linebuf, sizeof(linebuf), "bt %s %06lu", label, (unsigned long)passkey);
    dbgcons_bt_passkey(linebuf);
}

static inline uint8_t bt_classic_input_slot(uint8_t classic_slot)
{
    return (uint8_t)(INPUT_BRIDGE_BT_CLASSIC_SLOT_BASE + classic_slot);
}

static inline uint8_t bt_le_input_slot(void)
{
    return INPUT_BRIDGE_BT_LE_SLOT_BASE;
}

bool bt_hid_gamepad_status(bt_hid_gamepad_status_t *status)
{
    if (status == NULL)
        return false;
    uint32_t interrupts = save_and_disable_interrupts();
    bool available = bt_le_has_gamepad && bt_le_state == BT_LE_STATE_READY;
    if (available)
        *status = bt_le_gamepad_status;
    for (unsigned slot = 0; slot < INPUT_BRIDGE_BT_CLASSIC_SLOTS; slot++) {
        bt_classic_connection_t const *connection = &bt_classic_connections[slot];
        if (connection->in_use && connection->has_gamepad) {
            *status = connection->status;
            available = true;
            break;
        }
    }
    restore_interrupts(interrupts);
    return available;
}

static int8_t bt_hid_clamp_i8(int32_t value)
{
    if (value > INT8_MAX)
        return INT8_MAX;
    if (value < INT8_MIN)
        return INT8_MIN;

    return (int8_t)value;
}

static void bt_hid_enqueue(bt_hid_queue_entry_t const *entry)
{
    if (entry->slot >= INPUT_BRIDGE_MAX_SLOTS)
        return;

    // CYW43's background callbacks and bt_hid_task() both run on core0.
    // Protect the FIFO and overflow snapshots together against IRQ callbacks.
    uint32_t interrupts = save_and_disable_interrupts();

    if (!bt_hid_queue_resync && queue_try_add(&bt_hid_queue, entry)) {
        restore_interrupts(interrupts);
        return;
    }

    bt_hid_queue_resync = true;
    bt_hid_overflow_t *pending = &bt_hid_overflow[entry->slot];

    // During overload, intermediate motion and brief taps may be coalesced,
    // but retain the final key/button state and every pending disconnect.
    switch (entry->type) {
        case BT_HID_QUEUE_DISCONNECT:
            memset(pending, 0, sizeof(*pending));
            pending->disconnect = true;
            break;
        case BT_HID_QUEUE_KEYBOARD:
            pending->keyboard = entry->keyboard;
            pending->keyboard_pending = true;
            break;
        case BT_HID_QUEUE_MOUSE:
            pending->mouse = entry->mouse;
            pending->mouse_pending = true;
            break;
        case BT_HID_QUEUE_GAMEPAD:
            pending->gamepad = entry->gamepad;
            pending->gamepad_pending = true;
            break;
    }

    restore_interrupts(interrupts);
}

static bool bt_hid_dequeue(bt_hid_queue_entry_t *entry)
{
    uint32_t interrupts = save_and_disable_interrupts();

    if (queue_try_remove(&bt_hid_queue, entry)) {
        restore_interrupts(interrupts);
        return true;
    }

    // Drain older reports first. A disconnect precedes any reports from a
    // subsequent connection reusing that slot.
    for (uint8_t slot = 0; bt_hid_queue_resync && slot < INPUT_BRIDGE_MAX_SLOTS; slot++) {
        bt_hid_overflow_t *pending = &bt_hid_overflow[slot];
        entry->slot = slot;
        if (pending->disconnect) {
            pending->disconnect = false;
            entry->type = BT_HID_QUEUE_DISCONNECT;
        } else if (pending->keyboard_pending) {
            pending->keyboard_pending = false;
            entry->type = BT_HID_QUEUE_KEYBOARD;
            entry->keyboard = pending->keyboard;
        } else if (pending->mouse_pending) {
            pending->mouse_pending = false;
            entry->type = BT_HID_QUEUE_MOUSE;
            entry->mouse = pending->mouse;
        } else if (pending->gamepad_pending) {
            pending->gamepad_pending = false;
            entry->type = BT_HID_QUEUE_GAMEPAD;
            entry->gamepad = pending->gamepad;
        } else {
            continue;
        }
        restore_interrupts(interrupts);
        return true;
    }

    bt_hid_queue_resync = false;
    restore_interrupts(interrupts);
    return false;
}

static void bt_hid_enqueue_disconnect(uint8_t slot)
{
    bt_hid_queue_entry_t entry = {
        .slot = slot,
        .type = BT_HID_QUEUE_DISCONNECT,
    };

    bt_hid_enqueue(&entry);
}

static void bt_hid_enqueue_keyboard(uint8_t slot, bt_hid_keyboard_report_t const *report)
{
    bt_hid_queue_entry_t entry = {
        .slot = slot,
        .type = BT_HID_QUEUE_KEYBOARD,
    };

    entry.keyboard = *report;
    bt_hid_enqueue(&entry);
}

static void bt_hid_enqueue_mouse(uint8_t slot, bt_hid_mouse_report_t const *report)
{
    bt_hid_queue_entry_t entry = {
        .slot = slot,
        .type = BT_HID_QUEUE_MOUSE,
    };

    entry.mouse = *report;
    bt_hid_enqueue(&entry);
}

static void bt_hid_append_keycode(bt_hid_keyboard_report_t *report, uint8_t keycode)
{
    for (uint8_t pos = 0; pos < 6; pos++) {
        if (report->keycode[pos] == keycode)
            return;
        if (report->keycode[pos] == 0) {
            report->keycode[pos] = keycode;
            return;
        }
    }
}

static void bt_hid_enqueue_gamepad(uint8_t slot, uint16_t state)
{
    bt_hid_queue_entry_t entry = {
        .slot = slot,
        .type = BT_HID_QUEUE_GAMEPAD,
        .gamepad = state,
    };
    bt_hid_enqueue(&entry);
}

static int8_t bt_classic_find_slot(uint16_t hid_cid)
{
    for (uint8_t slot = 0; slot < INPUT_BRIDGE_BT_CLASSIC_SLOTS; slot++) {
        if (bt_classic_connections[slot].in_use && (bt_classic_connections[slot].hid_cid == hid_cid))
            return (int8_t)slot;
    }

    return -1;
}

static int8_t bt_classic_find_free_slot(void)
{
    for (uint8_t slot = 0; slot < INPUT_BRIDGE_BT_CLASSIC_SLOTS; slot++)
        if (!bt_classic_connections[slot].in_use)
            return (int8_t)slot;

    return -1;
}

static bool bt_classic_wants_inquiry(void)
{
    if (!bt_radio_on || !bt_classic_working || bt_classic_find_free_slot() < 0)
        return false;
    for (unsigned slot = 0; slot < INPUT_BRIDGE_BT_CLASSIC_SLOTS; slot++)
        if (bt_classic_connections[slot].in_use && bt_classic_connections[slot].has_gamepad)
            return false;
    return true;
}

static void bt_classic_schedule_inquiry(uint32_t delay_ms);

static void bt_classic_inquiry_timer(btstack_timer_source_t *timer)
{
    UNUSED(timer);
    if (!bt_classic_wants_inquiry() || bt_classic_state != BT_CLASSIC_IDLE)
        return;
    // Short searches with a quiet interval; stop once a Classic pad is ready.
    if (gap_inquiry_start(3) == ERROR_CODE_SUCCESS) {
        bt_classic_state = BT_CLASSIC_SCANNING;
        bt_hid_update_status();
    } else {
        bt_classic_schedule_inquiry(5000);
    }
}

static void bt_classic_schedule_inquiry(uint32_t delay_ms)
{
    btstack_run_loop_remove_timer(&bt_classic_scan_timer);
    if (!bt_classic_wants_inquiry() || bt_classic_state != BT_CLASSIC_IDLE)
        return;
    bt_classic_scan_timer.process = &bt_classic_inquiry_timer;
    btstack_run_loop_set_timer(&bt_classic_scan_timer, delay_ms);
    btstack_run_loop_add_timer(&bt_classic_scan_timer);
}

static void bt_classic_stop_inquiry(void)
{
    btstack_run_loop_remove_timer(&bt_classic_scan_timer);
    bt_classic_candidate = false;
    if (bt_classic_state == BT_CLASSIC_SCANNING) {
        bt_classic_state = BT_CLASSIC_IDLE;
        gap_inquiry_stop();
    }
}

static void bt_classic_inquiry_result(uint8_t const *packet, uint16_t size)
{
    if (size < 27 || bt_classic_state != BT_CLASSIC_SCANNING ||
        bt_classic_candidate || !bt_classic_wants_inquiry())
        return;
    uint32_t cod = gap_event_inquiry_result_get_class_of_device(packet);
    // Peripheral / joystick or gamepad only. Do not claim nearby keyboards,
    // mice, phones or consoles simply because they are discoverable.
    unsigned kind = (cod >> 2) & 0x0f;
    if ((cod & 0x1f00) != 0x0500 || (kind != 1 && kind != 2))
        return;
    bd_addr_t address;
    gap_event_inquiry_result_get_bd_addr(packet, address);
    if (!bt_pairing_allowed() && !bt_classic_known(address)) return;
    for (unsigned slot = 0; slot < INPUT_BRIDGE_BT_CLASSIC_SLOTS; slot++)
        if (bt_classic_connections[slot].in_use &&
            !memcmp(address, bt_classic_connections[slot].address, sizeof(bd_addr_t)))
            return;
    memcpy(bt_classic_candidate_addr, address, sizeof(bd_addr_t));
    bt_classic_candidate = true;
    // Connect only after inquiry cancellation completes.
    gap_inquiry_stop();
}

static void bt_classic_inquiry_complete(void)
{
    bt_classic_state = BT_CLASSIC_IDLE;
    if (bt_classic_candidate && bt_classic_wants_inquiry()) {
        bt_classic_candidate = false;
        bt_classic_state = BT_CLASSIC_CONNECTING;
        uint8_t result = hid_host_connect(bt_classic_candidate_addr,
            HID_PROTOCOL_MODE_REPORT, &bt_classic_outgoing_cid);
        if (result != ERROR_CODE_SUCCESS) {
            ahprintf("[bt] classic gamepad connect failed: 0x%02x\n", result);
            bt_classic_outgoing_cid = 0;
            bt_classic_state = BT_CLASSIC_IDLE;
        }
    } else {
        bt_classic_candidate = false;
    }
    bt_classic_schedule_inquiry(5000);
    bt_hid_update_status();
}

static void bt_classic_ds4_setup(btstack_timer_source_t *timer)
{
    bt_classic_connection_t *connection = timer->context;
    if (!connection->in_use || !connection->is_ds4)
        return;
    // Feature 2 enables extended input, including the touchpad. Defer until
    // incoming SDP/SET_PROTOCOL has completed inside BTstack.
    uint8_t result = hid_host_send_get_report(connection->hid_cid, HID_REPORT_TYPE_FEATURE, 2);
    if (result != ERROR_CODE_SUCCESS && ++connection->setup_attempts < 5) {
        btstack_run_loop_set_timer(timer, 200);
        btstack_run_loop_add_timer(timer);
    } else if (result != ERROR_CODE_SUCCESS) {
        ahprintf("[bt] DS4 extended input request failed: 0x%02x\n", result);
    }
}

static void bt_classic_descriptor_ready(uint8_t slot)
{
    bt_classic_connection_t *connection = &bt_classic_connections[slot];
    uint8_t const *descriptor = hid_descriptor_storage_get_descriptor_data(connection->hid_cid);
    uint16_t length = hid_descriptor_storage_get_descriptor_len(connection->hid_cid);
    if (!descriptor || !length || connection->descriptor_ready)
        return;
    connection->descriptor_ready = true;
    connection->is_ds4 = bt_ds4_matches_descriptor(descriptor, length);
    connection->has_gamepad = connection->is_ds4 || hid_gamepad_parse(&connection->gamepad, descriptor, length);
    // Also suppress the old mouse decoder for unsupported controller layouts.
    if (length >= 6 && descriptor[0] == 5 && descriptor[1] == 1 &&
        descriptor[2] == 9 && (descriptor[3] == 4 || descriptor[3] == 5) &&
        descriptor[4] == 0xa1 && descriptor[5] == 1)
        connection->gamepad_device = true;
    connection->gamepad_device |= connection->has_gamepad;
    if (connection->has_gamepad) {
        connection->status.slot = bt_classic_input_slot(slot);
        bt_classic_stop_inquiry();
        if (connection->is_ds4) {
            connection->status.expected_length = 78;
            connection->setup_timer.process = &bt_classic_ds4_setup;
            connection->setup_timer.context = connection;
            btstack_run_loop_set_timer(&connection->setup_timer, 100);
            btstack_run_loop_add_timer(&connection->setup_timer);
        }
    }
    ahprintf("[bt] classic descriptor %u bytes, %s\n", length,
        connection->is_ds4 ? "DS4" : connection->has_gamepad ? "gamepad" : "other HID");
    bt_hid_update_status();
}

static void bt_classic_disconnect_slot(uint8_t slot)
{
    if (!bt_classic_connections[slot].in_use)
        return;

    bt_hid_enqueue_disconnect(bt_classic_input_slot(slot));
    btstack_run_loop_remove_timer(&bt_classic_connections[slot].setup_timer);
    memset(&bt_classic_connections[slot], 0, sizeof(bt_classic_connections[slot]));
    bt_classic_schedule_inquiry(1000);
    bt_hid_update_status();
}

static void bt_classic_parse_report(uint8_t slot, uint8_t const *report, uint16_t report_len)
{
    if (!bt_radio_on) return;
    bt_classic_connection_t *connection = &bt_classic_connections[slot];
    bt_hid_keyboard_report_t keyboard = { 0, 0, {0} };
    bt_hid_mouse_report_t mouse = { 0 };
    btstack_hid_parser_t parser;
    const uint8_t *descriptor;
    uint16_t descriptor_len;
    bool saw_keyboard = false;
    bool saw_mouse = false;

    if ((report == NULL) || (report_len < 2) || (report[0] != 0xa1))
        return;

    report++;
    report_len--;

    if (connection->protocol_mode != HID_PROTOCOL_MODE_BOOT && !connection->descriptor_ready)
        return;
    if (connection->has_gamepad) {
        bt_hid_gamepad_status_t *status = &connection->status;
        status->reports++;
        status->length = report_len;
        if (connection->is_ds4) {
            bt_ds4_report_t decoded;
            status->expected_length = report[0] == 1 ? 10 : 78;
            status->decoded = bt_ds4_decode(&connection->ds4, report, report_len, &decoded);
            if (status->decoded) {
                status->state = decoded.gamepad;
                bt_hid_mouse_report_t touch = {
                    .buttons = decoded.mouse_buttons, .x = decoded.mouse_x, .y = decoded.mouse_y,
                };
                bt_hid_enqueue_mouse(bt_classic_input_slot(slot), &touch);
            }
        } else {
            hid_gamepad_t *pad = &connection->gamepad;
            status->expected_length = 0;
            for (unsigned i = 0; i < pad->report_count; i++)
                if (!pad->report_ids || pad->reports[i].id == report[0])
                    status->expected_length = (pad->reports[i].bits + 7u) / 8u + (pad->report_ids ? 1u : 0u);
            status->decoded = hid_gamepad_decode(pad, report, report_len, &status->state);
        }
        if (status->decoded)
            bt_hid_enqueue_gamepad(bt_classic_input_slot(slot), status->state);
        return;
    }
    if (connection->gamepad_device)
        return;

    if (connection->protocol_mode == HID_PROTOCOL_MODE_BOOT) {
        descriptor = btstack_hid_get_boot_descriptor_data();
        descriptor_len = btstack_hid_get_boot_descriptor_len();
    } else {
        descriptor = hid_descriptor_storage_get_descriptor_data(connection->hid_cid);
        descriptor_len = hid_descriptor_storage_get_descriptor_len(connection->hid_cid);

        if ((descriptor == NULL) || (descriptor_len == 0))
            return;
    }

    btstack_hid_parser_init(&parser, descriptor, descriptor_len, HID_REPORT_TYPE_INPUT, report, report_len);

    while (btstack_hid_parser_has_more(&parser)) {
        uint16_t usage_page;
        uint16_t usage;
        int32_t value;

        btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);

        switch (usage_page) {
            case HID_USAGE_PAGE_KEYBOARD:
                saw_keyboard = true;

                if ((usage >= HID_USAGE_KEY_KEYBOARD_LEFTCONTROL) && (usage <= HID_USAGE_KEY_KEYBOARD_RIGHT_GUI)) {
                    if (value)
                        keyboard.modifier |= (uint8_t)(1u << (usage - HID_USAGE_KEY_KEYBOARD_LEFTCONTROL));
                    break;
                }

                if ((usage != HID_USAGE_KEY_RESERVED) && (usage < 0x100))
                    bt_hid_append_keycode(&keyboard, (uint8_t)usage);
                break;

            case HID_USAGE_PAGE_BUTTON:
                if ((usage >= 1) && (usage <= 8)) {
                    saw_mouse = true;
                    if (value)
                        mouse.buttons |= (uint8_t)(1u << (usage - 1u));
                }
                break;

            case HID_USAGE_PAGE_DESKTOP:
                switch (usage) {
                    case HID_USAGE_DESKTOP_X:
                        saw_mouse = true;
                        mouse.x = bt_hid_clamp_i8(value);
                        break;

                    case HID_USAGE_DESKTOP_Y:
                        saw_mouse = true;
                        mouse.y = bt_hid_clamp_i8(value);
                        break;

                    case HID_USAGE_DESKTOP_WHEEL:
                        saw_mouse = true;
                        mouse.wheel = bt_hid_clamp_i8(value);
                        break;

                    default:
                        break;
                }
                break;

            default:
                break;
        }
    }

    if (saw_keyboard)
        bt_hid_enqueue_keyboard(bt_classic_input_slot(slot), &keyboard);

    if (saw_mouse)
        bt_hid_enqueue_mouse(bt_classic_input_slot(slot), &mouse);
}

static void bt_classic_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);

    if (packet_type != HCI_EVENT_PACKET || size < 2)
        return;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (size >= 3 && btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                bt_controller_off = false;
                bt_classic_working = true;
                gap_discoverable_control(bt_pairing_allowed());
                bt_classic_schedule_inquiry(1000);
            } else if (size >= 3 && btstack_event_state_get_state(packet) == HCI_STATE_OFF) {
                bt_controller_off = true;
                bt_classic_working = false;
                bt_classic_stop_inquiry();
                bt_classic_state = BT_CLASSIC_IDLE;
                bt_classic_outgoing_cid = 0;
                for (unsigned slot = 0; slot < INPUT_BRIDGE_BT_CLASSIC_SLOTS; slot++)
                    bt_classic_disconnect_slot(slot);
            }
            break;

        case GAP_EVENT_INQUIRY_RESULT:
            bt_classic_inquiry_result(packet, size);
            break;

        case GAP_EVENT_INQUIRY_COMPLETE:
            if (bt_classic_state == BT_CLASSIC_SCANNING)
                bt_classic_inquiry_complete();
            break;

        case HCI_EVENT_PIN_CODE_REQUEST: {
            if (size < 8) break;
            bd_addr_t event_addr;

            hci_event_pin_code_request_get_bd_addr(packet, event_addr);
            dbgcons_bt_passkey("bt pin 0000");
            if (bt_pairing_allowed()) gap_pin_code_response(event_addr, "0000");
            else gap_pin_code_negative(event_addr);
            break;
        }

        case HCI_EVENT_USER_CONFIRMATION_REQUEST: {
            if (size < 12) break;
            bd_addr_t event_addr;

            hci_event_user_confirmation_request_get_bd_addr(packet, event_addr);
            bt_hid_show_passkey("conf",
                hci_event_user_confirmation_request_get_numeric_value(packet));
            if (bt_pairing_allowed() || bt_classic_known(event_addr))
                gap_ssp_confirmation_response(event_addr);
            else gap_ssp_confirmation_negative(event_addr);
            break;
        }

        case HCI_EVENT_HID_META:
            if (size < 3) break;
            switch (hci_event_hid_meta_get_subevent_code(packet)) {
                case HID_SUBEVENT_INCOMING_CONNECTION:
                    if (size < 14) break;
                    if (hid_subevent_incoming_connection_get_status(packet) != ERROR_CODE_SUCCESS)
                        break;
                    bd_addr_t incoming;
                    hid_subevent_incoming_connection_get_address(packet, incoming);
                    if (bt_radio_on && (bt_pairing_allowed() || bt_classic_known(incoming)) &&
                        bt_classic_find_free_slot() >= 0) {
                        bt_classic_stop_inquiry();
                        hid_host_accept_connection(hid_subevent_incoming_connection_get_hid_cid(packet), bt_classic_report_mode);
                    } else {
                        hid_host_decline_connection(hid_subevent_incoming_connection_get_hid_cid(packet));
                    }
                    break;

                case HID_SUBEVENT_CONNECTION_OPENED: {
                    if (size < 15) break;
                    uint8_t status = hid_subevent_connection_opened_get_status(packet);
                    uint16_t hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                    int8_t slot;
                    bool outgoing = hid_cid == bt_classic_outgoing_cid;
                    if (outgoing) {
                        bt_classic_outgoing_cid = 0;
                        bt_classic_state = BT_CLASSIC_IDLE;
                    }

                    if (status != ERROR_CODE_SUCCESS) {
                        ahprintf("[bt] classic hid connect failed: 0x%02x\n", status);
                        dbgcons_bt_passkey_clear();
                        slot = bt_classic_find_slot(hid_cid);
                        if (slot >= 0) bt_classic_disconnect_slot((uint8_t)slot);
                        bt_classic_schedule_inquiry(5000);
                        bt_hid_update_status();
                        break;
                    }

                    if (bt_classic_find_slot(hid_cid) >= 0) break;
                    slot = bt_classic_find_free_slot();
                    if (slot < 0) {
                        hid_host_disconnect(hid_cid);
                        break;
                    }

                    bt_classic_connections[slot].in_use = true;
                    bt_classic_connections[slot].hid_cid = hid_cid;
                    bt_classic_connections[slot].protocol_mode = HID_PROTOCOL_MODE_REPORT;
                    bt_classic_connections[slot].gamepad_device = outgoing;
                    hid_subevent_connection_opened_get_bd_addr(packet, bt_classic_connections[slot].address);
                    bt_classic_schedule_inquiry(5000);
                    dbgcons_bt_passkey_clear();
                    bt_hid_update_status();
                    break;
                }

                case HID_SUBEVENT_DESCRIPTOR_AVAILABLE: {
                    if (size < 6) break;
                    int8_t slot = bt_classic_find_slot(hid_subevent_descriptor_available_get_hid_cid(packet));
                    if (slot >= 0 && hid_subevent_descriptor_available_get_status(packet) == ERROR_CODE_SUCCESS)
                        bt_classic_descriptor_ready((uint8_t)slot);
                    break;
                }

                case HID_SUBEVENT_SET_PROTOCOL_RESPONSE: {
                    if (size < 7) break;
                    int8_t slot = bt_classic_find_slot(hid_subevent_set_protocol_response_get_hid_cid(packet));

                    if ((slot >= 0)
                        && (hid_subevent_set_protocol_response_get_handshake_status(packet) == HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL)) {
                        bt_classic_connections[slot].protocol_mode =
                            (hid_protocol_mode_t)hid_subevent_set_protocol_response_get_protocol_mode(packet);
                    }
                    break;
                }

                case HID_SUBEVENT_REPORT: {
                    if (size < 7 || hid_subevent_report_get_report_len(packet) > size - 7) break;
                    int8_t slot = bt_classic_find_slot(hid_subevent_report_get_hid_cid(packet));

                    if (slot >= 0)
                        bt_classic_parse_report((uint8_t)slot, hid_subevent_report_get_report(packet),
                            hid_subevent_report_get_report_len(packet));
                    break;
                }

                case HID_SUBEVENT_CONNECTION_CLOSED: {
                    if (size < 5) break;
                    uint16_t cid = hid_subevent_connection_closed_get_hid_cid(packet);
                    if (cid == bt_classic_outgoing_cid) {
                        bt_classic_outgoing_cid = 0;
                        bt_classic_state = BT_CLASSIC_IDLE;
                        bt_classic_schedule_inquiry(5000);
                    }
                    int8_t slot = bt_classic_find_slot(cid);

                    dbgcons_bt_passkey_clear();

                    if (slot >= 0)
                        bt_classic_disconnect_slot((uint8_t)slot);
                    break;
                }

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

static void bt_le_clear_characteristics(void)
{
    memset(&bt_le_hid_service, 0, sizeof(bt_le_hid_service));
    memset(&bt_le_protocol_mode_characteristic, 0, sizeof(bt_le_protocol_mode_characteristic));
    memset(&bt_le_boot_keyboard_characteristic, 0, sizeof(bt_le_boot_keyboard_characteristic));
    memset(&bt_le_boot_mouse_characteristic, 0, sizeof(bt_le_boot_mouse_characteristic));
    memset(&bt_le_keyboard_notifications, 0, sizeof(bt_le_keyboard_notifications));
    memset(&bt_le_mouse_notifications, 0, sizeof(bt_le_mouse_notifications));
    bt_le_has_protocol_mode = false;
    bt_le_has_boot_keyboard = false;
    bt_le_has_boot_mouse = false;
    bt_le_hids_cid = 0;
    memset(bt_le_gamepads, 0, sizeof(bt_le_gamepads));
    memset(bt_le_gamepad_states, 0, sizeof(bt_le_gamepad_states));
    memset(&bt_le_gamepad_status, 0, sizeof(bt_le_gamepad_status));
    bt_le_has_gamepad = false;
}

static bool bt_le_adv_event_contains_hid_service(uint8_t const *packet)
{
    uint8_t ad_len = gap_event_advertising_report_get_data_length(packet);
    uint8_t const *ad_data = gap_event_advertising_report_get_data(packet);

    return ad_data_contains_uuid16(ad_len, ad_data, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE);
}

static void bt_le_start_scan(void)
{
    if (!bt_hid_ready || !bt_radio_on || !bt_classic_working || (bt_le_connection_handle != HCI_CON_HANDLE_INVALID))
        return;

    bt_le_clear_characteristics();
    dbgcons_bt_passkey_clear();
    bt_le_set_state(BT_LE_STATE_SCANNING);
    // Some LE controllers put their HID service UUID in the scan response.
    gap_set_scan_parameters(1, 48, 48);
    gap_start_scan();
}

static void bt_le_restart_scan(void)
{
    bt_le_connection_handle = HCI_CON_HANDLE_INVALID;
    dbgcons_bt_passkey_clear();
    bt_le_set_state(BT_LE_STATE_OFF);
    bt_le_start_scan();
}

static void bt_le_disconnect_and_restart(void)
{
    if (bt_le_connection_handle != HCI_CON_HANDLE_INVALID) {
        bt_le_set_state(BT_LE_STATE_DISCONNECTING);
        gap_disconnect(bt_le_connection_handle);
    } else {
        bt_le_restart_scan();
    }
}

static void bt_le_gamepad_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    // HIDS uses HCI_EVENT_PACKET for connection events and
    // HCI_EVENT_GATTSERVICE_META for notifications. Inspect the event itself.
    UNUSED(packet_type);
    UNUSED(channel);
    if (size < 5 || hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META || !bt_le_hids_cid)
        return;
    if (little_endian_read_16(packet, 3) != bt_le_hids_cid)
        return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
        case GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED: {
            if (size < 8 || bt_le_state != BT_LE_STATE_ENABLE_GAMEPAD)
                break;
            uint8_t status = gattservice_subevent_hid_service_connected_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
                ahprintf("[btle] gamepad discovery failed: 0x%02x\n", status);
                bt_le_disconnect_and_restart();
                break;
            }
            uint8_t services = gattservice_subevent_hid_service_connected_get_num_instances(packet);
            for (unsigned i = 0; i < services && i < MAX_NUM_HID_SERVICES; i++) {
                uint8_t const *descriptor = hids_host_descriptor_storage_get_descriptor_data(bt_le_hids_cid, i);
                uint16_t length = hids_host_descriptor_storage_get_descriptor_len(bt_le_hids_cid, i);
                if (hid_gamepad_parse(&bt_le_gamepads[i], descriptor, length)) {
                    if (!bt_le_has_gamepad) {
                        hid_gamepad_t const *pad = &bt_le_gamepads[i];
                        bt_le_gamepad_status.expected_length =
                            (pad->reports[0].bits + 7u) / 8u + (pad->report_ids ? 1u : 0u);
                    }
                    bt_le_has_gamepad = true;
                }
            }
            if (!bt_le_has_gamepad) {
                ahprintf("[btle] no supported gamepad report map\n");
                bt_le_disconnect_and_restart();
                break;
            }
            bt_le_gamepad_status.slot = bt_le_input_slot();
            dbgcons_bt_passkey_clear();
            bt_le_set_state(BT_LE_STATE_READY);
            break;
        }

        case GATTSERVICE_SUBEVENT_HID_REPORT: {
            if (size < 10 || bt_le_state != BT_LE_STATE_READY || !bt_le_has_gamepad)
                break;
            uint8_t service = gattservice_subevent_hid_report_get_service_index(packet);
            if (service >= MAX_NUM_HID_SERVICES || !bt_le_gamepads[service].field_count)
                break;
            uint8_t const *report = gattservice_subevent_hid_report_get_report(packet);
            uint16_t length = gattservice_subevent_hid_report_get_report_len(packet);
            if (length == 0 || length > size - 9u)
                break;
            hid_gamepad_t *pad = &bt_le_gamepads[service];
            // HIDS always inserts the Report Reference ID, even for ID zero.
            if (!pad->report_ids) {
                if (*report != 0)
                    break;
                report++;
                length--;
            }
            bt_le_gamepad_status.reports++;
            bt_le_gamepad_status.length = length;
            bt_le_gamepad_status.expected_length = 0;
            for (unsigned i = 0; i < pad->report_count; i++) {
                if (pad->reports[i].id == (pad->report_ids && length ? report[0] : 0))
                    bt_le_gamepad_status.expected_length =
                        (pad->reports[i].bits + 7u) / 8u + (pad->report_ids ? 1u : 0u);
            }
            bt_le_gamepad_status.decoded = hid_gamepad_decode(pad, report, length, &bt_le_gamepad_states[service]);
            if (!bt_le_gamepad_status.decoded)
                break;
            uint16_t combined = 0;
            for (unsigned i = 0; i < MAX_NUM_HID_SERVICES; i++)
                combined |= bt_le_gamepad_states[i];
            bt_le_gamepad_status.state = combined;
            bt_hid_enqueue_gamepad(bt_le_input_slot(), combined);
            break;
        }

        case GATTSERVICE_SUBEVENT_HID_SERVICE_DISCONNECTED:
            // The HCI disconnection handler releases input and restarts scanning.
            bt_le_has_gamepad = false;
            break;

        default:
            break;
    }
}

static void bt_le_ready(void)
{
    uint8_t boot_protocol_mode = 0;

    if (bt_le_has_protocol_mode) {
        gatt_client_write_value_of_characteristic_without_response(bt_le_connection_handle,
            bt_le_protocol_mode_characteristic.value_handle, 1, &boot_protocol_mode);
    }

    dbgcons_bt_passkey_clear();
    bt_le_set_state(BT_LE_STATE_READY);
}

static void bt_le_keyboard_notification_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    bt_hid_keyboard_report_t report = { 0, 0, {0} };

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION)
        return;

    if (gatt_event_notification_get_value_length(packet) > sizeof(report))
        memcpy(&report, gatt_event_notification_get_value(packet), sizeof(report));
    else
        memcpy(&report, gatt_event_notification_get_value(packet), gatt_event_notification_get_value_length(packet));

    bt_hid_enqueue_keyboard(bt_le_input_slot(), &report);
}

static void bt_le_mouse_notification_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    bt_hid_mouse_report_t report = { 0 };
    uint8_t const *value;
    uint16_t value_len;

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    if (hci_event_packet_get_type(packet) != GATT_EVENT_NOTIFICATION)
        return;

    value = gatt_event_notification_get_value(packet);
    value_len = gatt_event_notification_get_value_length(packet);

    if (value_len > 0)
        report.buttons = value[0];
    if (value_len > 1)
        report.x = (int8_t)value[1];
    if (value_len > 2)
        report.y = (int8_t)value[2];
    if (value_len > 3)
        report.wheel = (int8_t)value[3];

    bt_hid_enqueue_mouse(bt_le_input_slot(), &report);
}

static void bt_le_gatt_client_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    gatt_client_characteristic_t characteristic;

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (bt_le_state) {
        case BT_LE_STATE_SERVICE_QUERY:
            switch (hci_event_packet_get_type(packet)) {
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    gatt_event_service_query_result_get_service(packet, &bt_le_hid_service);
                    break;

                case GATT_EVENT_QUERY_COMPLETE:
                    if (gatt_event_query_complete_get_att_status(packet) != ATT_ERROR_SUCCESS) {
                        bt_le_disconnect_and_restart();
                        break;
                    }

                    bt_le_set_state(BT_LE_STATE_CHARACTERISTIC_QUERY);
                    gatt_client_discover_characteristics_for_service(&bt_le_gatt_client_handler,
                        bt_le_connection_handle, &bt_le_hid_service);
                    break;

                default:
                    break;
            }
            break;

        case BT_LE_STATE_CHARACTERISTIC_QUERY:
            switch (hci_event_packet_get_type(packet)) {
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);

                    switch (characteristic.uuid16) {
                        case ORG_BLUETOOTH_CHARACTERISTIC_PROTOCOL_MODE:
                            bt_le_protocol_mode_characteristic = characteristic;
                            bt_le_has_protocol_mode = true;
                            break;

                        case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT:
                            bt_le_boot_keyboard_characteristic = characteristic;
                            bt_le_has_boot_keyboard = true;
                            break;

                        case ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT:
                            bt_le_boot_mouse_characteristic = characteristic;
                            bt_le_has_boot_mouse = true;
                            break;

                        default:
                            break;
                    }
                    break;

                case GATT_EVENT_QUERY_COMPLETE:
                    if (gatt_event_query_complete_get_att_status(packet) != ATT_ERROR_SUCCESS) {
                        bt_le_disconnect_and_restart();
                        break;
                    }

                    if (bt_le_has_boot_keyboard) {
                        bt_le_set_state(BT_LE_STATE_ENABLE_KEYBOARD);
                        gatt_client_write_client_characteristic_configuration(&bt_le_gatt_client_handler,
                            bt_le_connection_handle, &bt_le_boot_keyboard_characteristic,
                            GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    } else if (bt_le_has_boot_mouse) {
                        bt_le_set_state(BT_LE_STATE_ENABLE_MOUSE);
                        gatt_client_write_client_characteristic_configuration(&bt_le_gatt_client_handler,
                            bt_le_connection_handle, &bt_le_boot_mouse_characteristic,
                            GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    } else {
                        // Gamepads such as Stadia have no boot reports. Let
                        // HIDS discover Report Maps, Report References and CCCs.
                        bt_le_set_state(BT_LE_STATE_ENABLE_GAMEPAD);
                        uint8_t status = hids_host_connect(bt_le_connection_handle,
                            &bt_le_gamepad_handler, HID_PROTOCOL_MODE_REPORT, &bt_le_hids_cid);
                        if (status != ERROR_CODE_SUCCESS) {
                            ahprintf("[btle] gamepad discovery could not start: 0x%02x\n", status);
                            bt_le_disconnect_and_restart();
                        }
                    }
                    break;

                default:
                    break;
            }
            break;

        case BT_LE_STATE_ENABLE_KEYBOARD:
            if (hci_event_packet_get_type(packet) != GATT_EVENT_QUERY_COMPLETE)
                break;

            if (gatt_event_query_complete_get_att_status(packet) != ATT_ERROR_SUCCESS) {
                bt_le_disconnect_and_restart();
                break;
            }

            gatt_client_listen_for_characteristic_value_updates(&bt_le_keyboard_notifications,
                &bt_le_keyboard_notification_handler, bt_le_connection_handle, &bt_le_boot_keyboard_characteristic);

            if (bt_le_has_boot_mouse) {
                bt_le_set_state(BT_LE_STATE_ENABLE_MOUSE);
                gatt_client_write_client_characteristic_configuration(&bt_le_gatt_client_handler,
                    bt_le_connection_handle, &bt_le_boot_mouse_characteristic,
                    GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
            } else {
                bt_le_ready();
            }
            break;

        case BT_LE_STATE_ENABLE_MOUSE:
            if (hci_event_packet_get_type(packet) != GATT_EVENT_QUERY_COMPLETE)
                break;

            if (gatt_event_query_complete_get_att_status(packet) != ATT_ERROR_SUCCESS) {
                bt_le_disconnect_and_restart();
                break;
            }

            gatt_client_listen_for_characteristic_value_updates(&bt_le_mouse_notifications,
                &bt_le_mouse_notification_handler, bt_le_connection_handle, &bt_le_boot_mouse_characteristic);
            bt_le_ready();
            break;

        default:
            break;
    }
}

static void bt_le_connect_candidate(uint8_t type, bd_addr_t address)
{
    if (!bt_radio_on || bt_le_state != BT_LE_STATE_SCANNING) return;
    gap_stop_scan();
    memcpy(bt_le_addr, address, sizeof(bd_addr_t));
    bt_le_addr_type = type;
    bt_le_set_state(BT_LE_STATE_CONNECTING);
    if (gap_connect(bt_le_addr, bt_le_addr_type) != ERROR_CODE_SUCCESS)
        bt_le_restart_scan();
}

static void bt_le_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING)
                bt_le_start_scan();
            else if (btstack_event_state_get_state(packet) == HCI_STATE_OFF) {
                bt_resolving = false;
                bt_hid_enqueue_disconnect(bt_le_input_slot());
                bt_le_clear_characteristics();
                bt_le_connection_handle = HCI_CON_HANDLE_INVALID;
                bt_le_set_state(BT_LE_STATE_OFF);
            }
            break;

        case GAP_EVENT_ADVERTISING_REPORT:
            if (bt_le_state != BT_LE_STATE_SCANNING)
                break;
            if (!bt_le_adv_event_contains_hid_service(packet))
                break;

            if (bt_resolving) break;
            gap_event_advertising_report_get_address(packet, bt_resolve_addr);
            bt_resolve_type = gap_event_advertising_report_get_address_type(packet);
            if (bt_pairing_allowed()) {
                bt_le_connect_candidate(bt_resolve_type, bt_resolve_addr);
            } else {
                // Resolve private addresses against stored IRKs as well as public identities.
                bt_resolving = true;
                if (sm_address_resolution_lookup(bt_resolve_type, bt_resolve_addr))
                    bt_resolving = false;
            }
            break;

        case HCI_EVENT_META_GAP:
            if (hci_event_gap_meta_get_subevent_code(packet) != GAP_SUBEVENT_LE_CONNECTION_COMPLETE)
                break;

            if (bt_le_state != BT_LE_STATE_CONNECTING)
                break;

            if (gap_subevent_le_connection_complete_get_status(packet) != ERROR_CODE_SUCCESS) {
                bt_le_restart_scan();
                break;
            }

            bt_le_connection_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
            bt_le_set_state(BT_LE_STATE_ENCRYPTING);
            sm_request_pairing(bt_le_connection_handle);
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (hci_event_disconnection_complete_get_connection_handle(packet) != bt_le_connection_handle)
                break;

            bt_hid_enqueue_disconnect(bt_le_input_slot());
            bt_le_clear_characteristics();
            bt_le_restart_scan();
            break;

        default:
            break;
    }
}

static void bt_le_sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    bool connect_to_service = false;

    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
        case SM_EVENT_IDENTITY_RESOLVING_FAILED: {
            if (size < 11 || !bt_resolving) break;
            bd_addr_t address;
            sm_event_identity_resolving_failed_get_address(packet, address);
            if (sm_event_identity_resolving_failed_get_addr_type(packet) != bt_resolve_type ||
                memcmp(address, bt_resolve_addr, sizeof(address))) break;
            bt_resolving = false;
            if (hci_event_packet_get_type(packet) == SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED)
                bt_le_connect_candidate(bt_resolve_type, bt_resolve_addr);
            break;
        }
        case SM_EVENT_JUST_WORKS_REQUEST:
            if (bt_pairing_allowed()) sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            else sm_bonding_decline(sm_event_just_works_request_get_handle(packet));
            break;

        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            ahprintf("[btle] confirm %lu\n", (unsigned long)sm_event_numeric_comparison_request_get_passkey(packet));
            bt_hid_show_passkey("conf",
                sm_event_numeric_comparison_request_get_passkey(packet));
            if (bt_pairing_allowed()) sm_numeric_comparison_confirm(sm_event_numeric_comparison_request_get_handle(packet));
            else sm_bonding_decline(sm_event_numeric_comparison_request_get_handle(packet));
            break;

        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            ahprintf("[btle] passkey %lu\n", (unsigned long)sm_event_passkey_display_number_get_passkey(packet));
            bt_hid_show_passkey("key",
                sm_event_passkey_display_number_get_passkey(packet));
            break;

        case SM_EVENT_PAIRING_COMPLETE:
            if (sm_event_pairing_complete_get_handle(packet) != bt_le_connection_handle ||
                bt_le_state != BT_LE_STATE_ENCRYPTING)
                break;
            if (sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS)
                connect_to_service = true;
            else
                bt_le_disconnect_and_restart();
            break;

        case SM_EVENT_REENCRYPTION_COMPLETE:
            if (sm_event_reencryption_complete_get_handle(packet) != bt_le_connection_handle ||
                bt_le_state != BT_LE_STATE_ENCRYPTING)
                break;
            if (sm_event_reencryption_complete_get_status(packet) == ERROR_CODE_SUCCESS)
                connect_to_service = true;
            else
                bt_le_disconnect_and_restart();
            break;

        default:
            break;
    }

    if (!connect_to_service || (bt_le_connection_handle == HCI_CON_HANDLE_INVALID))
        return;

    bt_le_set_state(BT_LE_STATE_SERVICE_QUERY);
    gatt_client_discover_primary_services_by_uuid16(&bt_le_gatt_client_handler, bt_le_connection_handle,
        ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE);
}

void bt_hid_init(void)
{
    if (bt_hid_ready) return;
    if (!settings_get()->bluetooth_enabled) {
        dbgcons_bt_status("bt off");
        return;
    }

    dbgcons_bt_status("bt init");

    for (uint8_t slot = INPUT_BRIDGE_BT_CLASSIC_SLOT_BASE; slot < INPUT_BRIDGE_MAX_SLOTS; slot++)
        input_bridge_reset(slot);

    queue_init(&bt_hid_queue, sizeof(bt_hid_queue_entry_t), BT_HID_QUEUE_DEPTH);

    dbgcons_bt_status("bt cyw43");
    ahprintf("[bt] initializing CYW43 and pairing storage\n");
    if (cyw43_arch_init()) {
        ahprintf("[bt] cyw43 init failed\n");
        dbgcons_bt_status("bt init fail");
        return;
    }

    dbgcons_bt_status("bt stack");
    ahprintf("[bt] initializing HID stack\n");
    l2cap_init();
    sm_init();
    // Preserve pairing with older HID devices after the BTstack default changes.
    sm_set_secure_connections_only_mode(false);
    sm_set_encryption_key_size_range(7, 16);
    gap_set_required_encryption_key_size(7);
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    gatt_client_init();
    hids_host_init(bt_le_descriptor_storage, sizeof(bt_le_descriptor_storage));

    hid_host_init(bt_classic_descriptor_storage, sizeof(bt_classic_descriptor_storage));
    hid_host_register_packet_handler(&bt_classic_packet_handler);

    gap_set_local_name(BT_HID_LOCAL_NAME);
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);
    gap_discoverable_control(!settings_get()->bluetooth_pairing);
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);
    hci_set_master_slave_policy(HCI_ROLE_MASTER);
    gap_ssp_set_auto_accept(0);

    bt_classic_hci_event_callback.callback = &bt_classic_packet_handler;
    hci_add_event_handler(&bt_classic_hci_event_callback);

    bt_le_hci_event_callback.callback = &bt_le_packet_handler;
    hci_add_event_handler(&bt_le_hci_event_callback);

    bt_le_sm_event_callback.callback = &bt_le_sm_packet_handler;
    sm_add_event_handler(&bt_le_sm_event_callback);

    bt_le_clear_characteristics();
    bt_hid_ready = true;

    dbgcons_bt_status("bt power");
    ahprintf("[bt] powering Bluetooth controller\n");
    bt_radio_on = true;
    bt_controller_off = false;
    hci_power_control(HCI_POWER_ON);
    ahprintf("[bt] controller startup requested\n");
}

void bt_hid_pair(void) { bt_pair_request = true; }
void bt_hid_forget(void) { bt_forget_request = true; }

static void bt_hid_settings_task(void)
{
    bool enabled = settings_get()->bluetooth_enabled;
    if (!bt_hid_ready) {
        if (enabled) bt_hid_init();
        return;
    }
    async_context_t *context = cyw43_arch_async_context();
    async_context_acquire_lock_blocking(context);
    if (bt_forget_request) {
        bt_forget_request = false;
        bt_forgetting = true;
    }
    if (bt_radio_on && (!enabled || bt_forgetting)) {
        bt_radio_on = false;
        bt_pair_window = false;
        bt_classic_stop_inquiry();
        gap_stop_scan();
        hci_power_control(HCI_POWER_OFF);
        // Release controls immediately, before asynchronous disconnection finishes.
        for (unsigned slot = INPUT_BRIDGE_BT_CLASSIC_SLOT_BASE; slot < INPUT_BRIDGE_MAX_SLOTS; slot++)
            bt_hid_enqueue_disconnect(slot);
        bt_hid_update_status();
    }
    if (bt_controller_off && bt_forgetting) {
        gap_delete_all_link_keys();
        for (int index = 0; index < le_device_db_max_count(); index++) {
            int type;
            bd_addr_t address;
            le_device_db_info(index, &type, address, NULL);
            if (type != BD_ADDR_TYPE_UNKNOWN) le_device_db_remove(index);
        }
        bt_forgetting = false;
    }
    if (enabled && !bt_radio_on && bt_controller_off && !bt_forgetting) {
        bt_radio_on = true;
        bt_controller_off = false;
        hci_power_control(HCI_POWER_ON);
    }
    if (bt_pair_request) {
        bt_pair_request = false;
        if (bt_radio_on) {
            bt_pair_window = true;
            bt_pair_deadline = btstack_run_loop_get_time_ms() + 120000u;
            bt_classic_schedule_inquiry(1);
        }
    }
    if (bt_pair_window && (int32_t)(btstack_run_loop_get_time_ms() - bt_pair_deadline) >= 0)
        bt_pair_window = false;
    if (bt_classic_working) gap_discoverable_control(bt_pairing_allowed());
    async_context_release_lock(context);
}

void bt_hid_task(void)
{
    bt_hid_queue_entry_t entry;
    bt_hid_settings_task();

    if (!bt_hid_ready)
        return;

    while (bt_hid_dequeue(&entry)) {
        if (!bt_radio_on && entry.type != BT_HID_QUEUE_DISCONNECT) continue;
        switch (entry.type) {
            case BT_HID_QUEUE_DISCONNECT:
                input_bridge_disconnect(entry.slot);
                break;

            case BT_HID_QUEUE_KEYBOARD:
                input_bridge_handle_keyboard_boot(entry.slot, entry.keyboard.modifier, entry.keyboard.keycode);
                break;

            case BT_HID_QUEUE_MOUSE:
                input_bridge_handle_mouse_boot(entry.slot, entry.mouse.buttons, entry.mouse.x, entry.mouse.y,
                    entry.mouse.wheel);
                break;

            case BT_HID_QUEUE_GAMEPAD:
                input_bridge_handle_gamepad(entry.slot, entry.gamepad);
                break;
        }
    }
}
