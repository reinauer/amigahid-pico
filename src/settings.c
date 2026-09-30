/* Runtime settings for AmigaHID-Pico. SPDX-License-Identifier: EPL-2.0 */
#include "settings.h"
#include "config.h"

#include <stddef.h>
#include <string.h>

#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include "class/hid/hid.h"
#ifdef ENABLE_BLUETOOTH_HID
#include "pico/btstack_flash_bank.h"
#endif

/* The final two sectors belong to BTstack, also in USB-only builds so
 * switching firmware variants preserves both settings and pairing data.
 * Two preceding sectors alternate: an interrupted write leaves the previous
 * valid record intact. The linker also reserves this 16 KiB region. */
#define SETTINGS_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - 4u * FLASH_SECTOR_SIZE)
#define SETTINGS_MAGIC 0x41484346u
#define SETTINGS_SCHEMA 4u
#define SETTINGS_V1_SIZE 8u

typedef struct {
    uint32_t magic;
    uint32_t schema;
    uint32_t length;
    uint32_t sequence;
    settings_t values;
    uint32_t crc;
} settings_record_t;

_Static_assert(FLASH_SECTOR_SIZE == 4096, "Update settings_flash.ld reservation");
_Static_assert(offsetof(settings_t, port_mode) == SETTINGS_V1_SIZE, "Preserve v1 migration layout");
_Static_assert(offsetof(settings_t, joystick_port2) == 9, "Preserve v2 migration layout");
_Static_assert(sizeof(settings_t) == 16, "Update settings schema");
_Static_assert(sizeof(settings_record_t) == 36, "Unexpected record padding");
_Static_assert(sizeof(settings_record_t) <= FLASH_PAGE_SIZE, "Settings exceed one flash page");
#ifdef ENABLE_BLUETOOTH_HID
_Static_assert(SETTINGS_FLASH_OFFSET + 2u * FLASH_SECTOR_SIZE <= PICO_FLASH_BANK_STORAGE_OFFSET,
    "Settings overlap Bluetooth pairing storage");
#endif

static settings_t active;
static int current_bank = -1;
static uint32_t current_sequence;

void settings_defaults(settings_t *settings)
{
    *settings = (settings_t) {
        .menu_key = SETTINGS_MENU_F12,
        .menu_entry = SETTINGS_MENU_HOLD,
        .right_gui = SETTINGS_GUI_AMIGA,
        .wheel_enabled = 1,
        .wheel_reverse = 0,
        .mouse_speed = 0,
        .display = SETTINGS_DISPLAY_STATUS,
        .watchdog = 0,
        .port_mode = SETTINGS_PORT_MOUSE,
        .bluetooth_enabled = 1,
#ifdef HAS_JOYSTICK_PORT2
        .joystick_port2 = 1,
#endif
    };
}

bool settings_valid(settings_t const *settings)
{
    return settings != NULL && settings->menu_key < SETTINGS_MENU_KEY_COUNT &&
        settings->menu_entry < SETTINGS_MENU_ENTRY_COUNT && settings->right_gui < SETTINGS_GUI_COUNT &&
        settings->wheel_enabled <= 1 && settings->wheel_reverse <= 1 && settings->mouse_speed < 4 &&
        settings->display < SETTINGS_DISPLAY_COUNT && settings->watchdog < 3 &&
        settings->port_mode < SETTINGS_PORT_COUNT && settings->joystick_port2 <= 1 &&
        !settings->reserved[0] && !settings->reserved[1] &&
        settings->bluetooth_enabled <= 1 && settings->bluetooth_pairing <= 1 &&
        !settings->reserved4[0] && !settings->reserved4[1];
}

static uint32_t record_crc(settings_record_t const *record)
{
    uint8_t const *bytes = (uint8_t const *)record;
    uint32_t crc = UINT32_MAX;
    size_t length = offsetof(settings_record_t, values) + record->length;
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0);
    }
    return ~crc;
}

static settings_record_t const *bank_record(unsigned bank)
{
    return (settings_record_t const *)(XIP_BASE + SETTINGS_FLASH_OFFSET + bank * FLASH_SECTOR_SIZE);
}

static bool record_decode(settings_record_t const *record, settings_t *settings)
{
    if (record->magic != SETTINGS_MAGIC ||
        !((record->schema == 1 && record->length == SETTINGS_V1_SIZE) ||
          ((record->schema == 2 || record->schema == 3) && record->length == 12) ||
          (record->schema == SETTINGS_SCHEMA && record->length == sizeof(settings_t))))
        return false;
    uint32_t crc;
    memcpy(&crc, (uint8_t const *)&record->values + record->length, sizeof(crc));
    if (crc != record_crc(record))
        return false;
    // Version 1 stored the same first eight fields. New options use defaults;
    // upgrading firmware never silently drops an existing watchdog setting.
    settings_defaults(settings);
    uint8_t port2_default = settings->joystick_port2;
    memcpy(settings, &record->values, record->length);
    if (record->schema == 2) {
        // This byte was reserved (and required to be zero) in schema 2.
        if (settings->joystick_port2 != 0)
            return false;
        settings->joystick_port2 = port2_default;
    }
    return settings_valid(settings);
}

void settings_init(void)
{
    settings_defaults(&active);
    current_bank = -1;
    current_sequence = 0;
    for (unsigned bank = 0; bank < 2; bank++) {
        settings_record_t const *record = bank_record(bank);
        settings_t decoded;
        if (record_decode(record, &decoded) && (current_bank < 0 ||
            (int32_t)(record->sequence - current_sequence) > 0)) {
            current_bank = (int)bank;
            current_sequence = record->sequence;
            active = decoded;
        }
    }
}

settings_t const *settings_get(void)
{
    return &active;
}

bool settings_set(settings_t const *settings)
{
    if (!settings_valid(settings))
        return false;
    active = *settings;
    return true;
}

typedef struct {
    uint32_t offset;
    uint8_t page[FLASH_PAGE_SIZE];
} settings_write_t;

static void write_record(void *context)
{
    settings_write_t const *write = context;
    flash_range_erase(write->offset, FLASH_SECTOR_SIZE);
    flash_range_program(write->offset, write->page, FLASH_PAGE_SIZE);
}

bool settings_save(settings_t const *settings)
{
    if (!settings_valid(settings))
        return false;
    settings_t previous;
    if (current_bank >= 0 && bank_record(current_bank)->schema == SETTINGS_SCHEMA &&
        record_decode(bank_record(current_bank), &previous) &&
        memcmp(settings, &previous, sizeof(*settings)) == 0)
        return true;

    unsigned bank = current_bank == 0 ? 1 : 0;
    settings_record_t record = {
        .magic = SETTINGS_MAGIC,
        .schema = SETTINGS_SCHEMA,
        .length = sizeof(settings_t),
        .sequence = current_sequence + 1u,
        .values = *settings,
    };
    record.crc = record_crc(&record);
    settings_write_t write = { .offset = SETTINGS_FLASH_OFFSET + bank * FLASH_SECTOR_SIZE };
    memset(write.page, 0xff, sizeof(write.page));
    memcpy(write.page, &record, sizeof(record));

    /* Core1 is registered for flash lockout in all firmware variants. This
     * function is called only on core0, from the menu task, never an IRQ. */
    int result = flash_safe_execute(write_record, &write, 500);
    settings_record_t const *stored = bank_record(bank);
    if (result != PICO_OK || !record_decode(stored, &previous) || memcmp(stored, &record, sizeof(record)) != 0)
        return false;
    current_bank = (int)bank;
    current_sequence = record.sequence;
    return true;
}

uint16_t settings_mouse_interval_us(void)
{
    static uint16_t const intervals[] = {300, 200, 150, 100};
    return intervals[active.mouse_speed];
}

uint32_t settings_watchdog_ms(void)
{
    static uint32_t const timeouts[] = {0, 2000, 5000};
    return timeouts[active.watchdog];
}

uint8_t settings_menu_hid_key(void)
{
    static uint8_t const keys[] = {HID_KEY_F12, HID_KEY_F11, HID_KEY_APPLICATION};
    return keys[active.menu_key];
}
