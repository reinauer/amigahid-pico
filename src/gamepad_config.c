/* SPDX-License-Identifier: EPL-2.0 */
#include "gamepad_config.h"
#include "settings.h"

uint8_t gamepad_axis(int64_t value, int32_t minimum, int32_t maximum, uint8_t low, uint8_t high)
{
    static unsigned const zones[] = {25, 50, 75};
    int64_t range = (int64_t)maximum - minimum;
    if (range < 2 || value < minimum || value > maximum) return 0;
    int64_t position = 200 * (value - minimum);
    unsigned zone = zones[settings_get()->gamepad_deadzone];
    if (position < (100 - zone) * range) return low;
    if (position > (100 + zone) * range) return high;
    return 0;
}

uint16_t gamepad_map(uint8_t dpad, uint8_t stick, uint16_t buttons, uint8_t const defaults[7])
{
    settings_t const *settings = settings_get();
    uint16_t state = (settings->gamepad_directions != 2 ? dpad : 0) |
        (settings->gamepad_directions != 1 ? stick : 0);
    for (unsigned i = 0; i < 7; i++) {
        unsigned source = settings->gamepad_buttons[i];
        if (!source) source = defaults[i];
        if (source >= 1 && source <= 16 && (buttons & (1u << (source - 1))))
            state |= 1u << (i + 4);
    }
    return state;
}
