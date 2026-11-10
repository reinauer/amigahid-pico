#!/usr/bin/env python3
"""Compare the compiled REV7 pin map with the hardware source of truth."""
# SPDX-License-Identifier: EPL-2.0
import json
import os
import re
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'kicad_r7/scripts'))
from sexpr import parse, children, get

parts = {c['ref']: c for c in json.loads((root / 'kicad_r7/circuit.json').read_text())['parts']}
symbols = children(parse((root / 'kicad_r7/r7.kicad_sym').read_text()), 'symbol')
chip = next(s for s in symbols if s[1] == parts['U1']['symbol'])
pins = {}
for unit in children(chip, 'symbol'):
    for pin in children(unit, 'pin'):
        match = re.match(r'GPIO(\d+)', get(pin, 'name')[1])
        if match:
            pins[int(match[1])] = parts['U1']['nets'].get(get(pin, 'number')[1])
# Read the actual SDK board header as well as the application's pin map.
text = subprocess.check_output([
    os.environ.get('CC', 'cc'), '-E', '-dM', '-x', 'c', '-DPICO_RP2350=1',
    '-DBOARD_HIDPICO_REV7', '-I' + str(root / 'tests/stubs'),
    '-I' + str(root / 'pico-sdk/src/boards/include'),
    '-include', str(root / 'cmake/boards/amigahid_r7.h'),
    '-include', str(root / 'src/config.h'), '-'], input='', text=True)
macros = dict(re.findall(r'^#define (\w+)\s+(.*)$', text, re.M))
expected = {
    'PICO_DEFAULT_UART_TX_PIN': 'UART_TX', 'PICO_DEFAULT_UART_RX_PIN': 'UART_RX',
    'I2C_PIN_SDA': 'SDA', 'I2C_PIN_SCL': 'SCL',
    'KBD_AMIGA_DAT': 'L_KDAT', 'KBD_AMIGA_CLK': 'L_KCLK',
    'MENU_LEVER_PIN_LEFT': 'MENU_LEFT_N', 'MENU_LEVER_PIN_RIGHT': 'MENU_RIGHT_N',
    'MENU_LEVER_PIN_PUSH': 'MENU_PUSH_N',
    'CYW43_DEFAULT_PIN_WL_REG_ON': 'WL_ON', 'CYW43_DEFAULT_PIN_WL_CLOCK': 'WL_CLK',
    'CYW43_DEFAULT_PIN_WL_CS': 'WL_CS', 'CYW43_DEFAULT_PIN_WL_DATA_IN': 'WL_DATA',
    'CYW43_DEFAULT_PIN_WL_DATA_OUT': 'WL_DATA', 'CYW43_DEFAULT_PIN_WL_HOST_WAKE': 'WL_DATA',
}
for port in (1, 2):
    for suffix, net in [('HQ', 'RIGHT'), ('VQ', 'LEFT'), ('H', 'DOWN'), ('V', 'UP'),
                        ('B1', 'FIRE'), ('B2', 'B2'), ('B3', 'B3')]:
        expected[f'QM{port}_AMIGA_{suffix}'] = f'L_P{port}_{net}'
for macro, net in expected.items():
    gpio = int(macros[macro].rstrip('uU'))
    assert pins[gpio] == net, (macro, gpio, pins[gpio], net)
assert pins[int(macros['KBD_AMIGA_RST'])] is None
for pin, macro in [('1', 'MENU_LEVER_PIN_LEFT'), ('2', 'MENU_LEVER_PIN_RIGHT'),
                   ('T', 'MENU_LEVER_PIN_PUSH')]:
    assert parts['SW2']['nets'][pin] == expected[macro]
assert macros['PICO_DEFAULT_I2C'] == '1'
assert macros['PICO_DEFAULT_I2C_SDA_PIN'] == macros['I2C_PIN_SDA']
assert macros['PICO_DEFAULT_I2C_SCL_PIN'] == macros['I2C_PIN_SCL']
for unused in ['PICO_VSYS_PIN', 'PICO_DEFAULT_SPI', 'CYW43_WL_GPIO_SMPS_PIN', 'CYW43_WL_GPIO_VBUS_PIN']:
    assert unused not in macros, unused
print('r7 firmware pin map matches circuit.json and the MCU symbol: PASS')
