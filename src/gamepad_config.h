/* Gamepad policy shared by USB and Bluetooth. SPDX-License-Identifier: EPL-2.0 */
#ifndef AMIGAHID_GAMEPAD_CONFIG_H
#define AMIGAHID_GAMEPAD_CONFIG_H
#include <stdint.h>
uint8_t gamepad_axis(int64_t value, int32_t minimum, int32_t maximum, uint8_t low, uint8_t high);
uint16_t gamepad_map(uint8_t dpad, uint8_t stick, uint16_t buttons, uint8_t const defaults[7]);
#endif
