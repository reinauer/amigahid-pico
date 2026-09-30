/* CD32 controller output. SPDX-License-Identifier: EPL-2.0
 * Protocol reference: Amiga Test Kit, testkit/joymouse.c read_gamepad().
 */
#include "cd32.h"
#include "cd32.pio.h"
#include "config.h"
#include "hid_gamepad.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"

// Reserve PIO1 before CYW43 starts. Four SMs for two ports, with both programs
// shared between ports. PIO0 remains available to the wireless driver.
static PIO const output_pio = pio1;
static unsigned data_offset, fire_offset;
static struct {
    unsigned clock, data, load, data_sm, fire_sm;
    uint16_t data_state, fire_state;
    bool enabled;
} ports[] = {
    {.clock = QM1_AMIGA_B1, .data = QM1_AMIGA_B2, .load = QM1_AMIGA_B3, .data_sm = 0, .fire_sm = 1},
#ifdef HAS_JOYSTICK_PORT2
    {.clock = QM2_AMIGA_B1, .data = QM2_AMIGA_B2, .load = QM2_AMIGA_B3, .data_sm = 2, .fire_sm = 3},
#endif
};
#define PORT_COUNT (sizeof(ports) / sizeof(ports[0]))

void cd32_init(void)
{
    data_offset = pio_add_program(output_pio, &cd32_data_program);
    fire_offset = pio_add_program(output_pio, &cd32_fire_program);
    for (unsigned port = 0; port < PORT_COUNT; port++) {
        pio_sm_claim(output_pio, ports[port].data_sm);
        pio_sm_claim(output_pio, ports[port].fire_sm);
        ports[port].data_state = ports[port].fire_state = UINT16_MAX;
        // The select input is never driven by this adapter.
        gpio_set_dir(ports[port].load, GPIO_IN);
        gpio_pull_up(ports[port].load);
        gpio_pull_up(ports[port].clock);
        gpio_pull_up(ports[port].data);
    }
}

static void start_sm(unsigned sm, unsigned offset, unsigned pin, unsigned load, unsigned clock, bool serial)
{
    pio_sm_set_enabled(output_pio, sm, false);
    pio_sm_config config = serial ? cd32_data_program_get_default_config(offset) : cd32_fire_program_get_default_config(offset);
    sm_config_set_out_pins(&config, pin, 1);
    sm_config_set_set_pins(&config, pin, 1);
    sm_config_set_jmp_pin(&config, load);
    sm_config_set_in_pins(&config, clock);
    sm_config_set_in_shift(&config, false, false, 32);
    sm_config_set_out_shift(&config, true, false, serial ? 8 : 32);
    pio_sm_init(output_pio, sm, offset, &config);
    pio_sm_set_pins_with_mask(output_pio, sm, 0, 1u << pin);
    pio_sm_set_pindirs_with_mask(output_pio, sm, 0, 1u << pin);
    pio_sm_exec(output_pio, sm, pio_encode_set(pio_x, 0));
    pio_sm_put(output_pio, sm, 0);
    pio_gpio_init(output_pio, pin);
    pio_sm_set_enabled(output_pio, sm, true);
}

void cd32_enable(unsigned port, bool enabled)
{
    if (port >= PORT_COUNT || ports[port].enabled == enabled) return;
    unsigned clock = ports[port].clock, data = ports[port].data;
    gpio_set_dir(ports[port].load, GPIO_IN);
    if (enabled) {
        // Hand over released pins before enabling PIO's open-drain outputs.
        gpio_set_dir(clock, GPIO_IN);
        gpio_set_dir(data, GPIO_IN);
        start_sm(ports[port].data_sm, data_offset, data, ports[port].load, clock, true);
        start_sm(ports[port].fire_sm, fire_offset, clock, ports[port].load, clock, false);
    } else {
        gpio_set_dir(clock, GPIO_IN);
        gpio_set_dir(data, GPIO_IN);
        gpio_set_function(clock, GPIO_FUNC_SIO);
        gpio_set_function(data, GPIO_FUNC_SIO);
        pio_sm_set_enabled(output_pio, ports[port].data_sm, false);
        pio_sm_set_enabled(output_pio, ports[port].fire_sm, false);
    }
    ports[port].enabled = enabled;
    ports[port].data_state = ports[port].fire_state = UINT16_MAX;
}

void cd32_update(unsigned port, uint16_t state)
{
    if (port >= PORT_COUNT || !ports[port].enabled) return;
    // Wire order: Blue, Red, Yellow, Green, Forward, Rewind, Play, released ID.
    uint32_t serial = ((state & GAMEPAD_FIRE2) ? 1u : 0u) |
        ((state & GAMEPAD_FIRE) ? 2u : 0u) | ((state & GAMEPAD_FIRE4) ? 4u : 0u) |
        ((state & GAMEPAD_FIRE3) ? 8u : 0u) | ((state & GAMEPAD_R) ? 16u : 0u) |
        ((state & GAMEPAD_L) ? 32u : 0u) | ((state & GAMEPAD_PLAY) ? 64u : 0u);
    if (state != ports[port].data_state && !pio_sm_is_tx_fifo_full(output_pio, ports[port].data_sm)) {
        pio_sm_put(output_pio, ports[port].data_sm, serial);
        ports[port].data_state = state;
    }
    if (state != ports[port].fire_state && !pio_sm_is_tx_fifo_full(output_pio, ports[port].fire_sm)) {
        pio_sm_put(output_pio, ports[port].fire_sm, (state & GAMEPAD_FIRE) ? 1u : 0u);
        ports[port].fire_state = state;
    }
}
