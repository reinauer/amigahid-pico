#!/usr/bin/env python3
"""Extract r7 mechanics with a Python interpreter providing pcbnew.

Connector envelopes still require supplier drawing review when parts change.
"""
from pathlib import Path
import hashlib
import json
import pcbnew as p

ROOT = Path(__file__).resolve().parents[2]
case = ROOT / 'case_r7'
src = ROOT / 'kicad_r7/amigahid-pico.kicad_pcb'
b = p.LoadBoard(str(src))
fs = {f.GetReference(): f for f in b.GetFootprints()}
assert 'H5' not in fs and 'H6' not in fs, 'Shared mounts replace H5/H6'
refs = ['H1', 'H2', 'H3', 'H4', 'J3', 'J4', 'J5', 'J6', 'J7', 'J10', 'SW1', 'SW2', 'U1', 'U4', 'C4']
assert fs['SW2'].GetValue() == 'SLLB510100 / menu'
assert abs(abs(fs['SW2'].GetOrientationDegrees()) - 180) < 0.001
def xy(f):
    return [round(f.GetPosition().x / 1e6, 4), round(f.GetPosition().y / 1e6, 4)]
points = []
for item in b.GetDrawings():
    if item.GetLayer() == p.Edge_Cuts:
        assert item.GetShape() == p.SHAPE_T_SEGMENT
        points += [(item.GetStart().x / 1e6, item.GetStart().y / 1e6),
                   (item.GetEnd().x / 1e6, item.GetEnd().y / 1e6)]
assert min(x for x, y in points) == 0 and min(y for x, y in points) == -6
width = max(x for x, y in points)
ymax = max(y for x, y in points)
depth = ymax - min(y for x, y in points)
assert (width, depth) == (70, 66), 'Review outline and antenna notch after size changes'
data = {
    'source': str(src.relative_to(ROOT)),
    'pcb_sha256': hashlib.sha256(src.read_bytes()).hexdigest(),
    'board_mm': [width, depth, b.GetDesignSettings().GetBoardThickness() / 1e6],
    'coordinates': f'KiCad X right/Y down, board Y=-6..60; case X=x, Y={ymax:g}-y',
    'outline_bounds_mm': [0, -6, width, ymax],
    'positions': {ref: xy(fs[ref]) for ref in refs},
    'rotations_degrees': {ref: fs[ref].GetOrientationDegrees() for ref in refs},
    'shared_mounts': ['H1', 'H2', 'H3', 'H4'],
    'notch_mm': [0, 36, 3.6, 16],
}
for ref in ['J5', 'J6']:
    pads = [a for a in fs[ref].Pads() if a.GetNumber() == 'SH']
    assert len(pads) == 2
    data[ref + '_centre_x'] = sum(a.GetPosition().x for a in pads) / 2e6
pads = {a.GetNumber(): a for a in fs['J4'].Pads() if a.GetNumber()}
data['usb_centre_y'] = (pads['1'].GetPosition().y + pads['4'].GetPosition().y) / 2e6
data['usb_shield_tabs'] = [
    [round(a.GetPosition().x / 1e6, 4),
     round(ymax - a.GetPosition().y / 1e6, 4)]
    for a in fs['J4'].Pads() if a.GetNumber() == '9']
assert len(data['usb_shield_tabs']) == 4
data['mount_pitch_mm'] = [
    data['positions']['H2'][0] - data['positions']['H1'][0],
    data['positions']['H3'][1] - data['positions']['H1'][1],
]
data['menu_lever'] = {
    'reference': 'SW2', 'part': 'SLLB510100',
    'tip_offset_toward_edge_mm': 6.5024,
    'tip_kicad_y_mm': round(data['positions']['SW2'][1] + 6.5024, 4),
    'sweep_envelope_mm': [14.2, 9.5, 2.2],
    'envelope_is_approximate': True,
}
assert all(abs(a - z) < 0.001 for a, z in zip(data['mount_pitch_mm'], [30.5, 28.6]))
(case / 'pcb-mechanics.json').write_text(json.dumps(data, indent=2) + '\n')
lines = ['// Generated from kicad_r7 by tools/extract_pcb.py; units mm.',
         f'board_x = {width:g};', f'board_y = {depth:g};',
         f'board_t = {data["board_mm"][2]:g};']
for ref, coords in data['positions'].items():
    lines.append(f'{ref.lower()} = {json.dumps([coords[0], round(ymax - coords[1], 4)])};')
lines += [f'de9_centres = [{data["J5_centre_x"]}, {data["J6_centre_x"]}];',
          f'usb_y = {ymax - data["usb_centre_y"]:g};',
          f'usb_shield_tabs = {json.dumps(data["usb_shield_tabs"])};',
          'oled_mounts = [h1, h2, h3, h4];']
(case / 'pcb-mechanics.scad').write_text('\n'.join(lines) + '\n')
print('Extracted current r7 outline size, shared mounts and connector positions.')
