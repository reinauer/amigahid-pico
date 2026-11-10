#!/usr/bin/env python3
"""Check supervisor/recovery separation in circuit data and the actual PCB.

Run with Python providing pcbnew. Does not substitute for power-ramp testing.
"""
import json
from pathlib import Path
import pcbnew

root = Path(__file__).resolve().parents[1]
parts = {p['ref']: p for p in json.loads((root / 'circuit.json').read_text())['parts']}
expected = {
    'U13': {'1': 'RUN', '2': 'GND', '3': 'RECOVERY_N', '5': '+3V3', '6': '+3V3'},
    'R12': {'1': '+3V3', '2': 'RUN'},
    'R26': {'1': '+3V3', '2': 'RECOVERY_N'},
    'C41': {'1': '+3V3', '2': 'GND'},
}
board = pcbnew.LoadBoard(str(root / 'amigahid-pico.kicad_pcb'))
fps = {f.GetReference(): f for f in board.GetFootprints()}
for ref, pins in expected.items():
    assert parts[ref]['nets'] == pins, ref
    actual = {p.GetNumber(): p.GetNetname() for p in fps[ref].Pads()}
    for pin, net in pins.items():
        assert actual[pin] == net, (ref, pin, actual[pin], net)
for ref, part in parts.items():
    if ref not in fps:
        continue
    pads = {p.GetNumber(): p.GetNetname() for p in fps[ref].Pads()}
    for pin, net in part['nets'].items():
        assert pads.get(pin) == net, (ref, pin, pads.get(pin), net)
assert '4' not in parts['U13']['nets'], 'CT must remain open for the fixed delay'
ct = next(p for p in fps['U13'].Pads() if p.GetNumber() == '4')
assert ct.GetNetname().startswith('unconnected-'), ct.GetNetname()
assert not any(t.GetNetCode() == ct.GetNetCode() for t in board.GetTracks())
for ref, pin in [('SW1', '1'), ('U9', '4')]:
    assert parts[ref]['nets'][pin] == 'RECOVERY_N'
    assert next(p for p in fps[ref].Pads() if p.GetNumber() == pin).GetNetname() == 'RECOVERY_N'
assert parts['U1']['nets']['26'] == 'RUN'
assert parts['D2']['nets']['1'] == 'RUN'
assert not any(p.GetNetname() == 'RUN' for p in fps['U9'].Pads())
print('PASS: supervisor pinout, open CT, RUN pull-up and independent recovery request')
