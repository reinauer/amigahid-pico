#!/usr/bin/env python3
"""Extend the DB9 edge by 6 mm, retaining every component and route.

The existing coordinates stay fixed: the 70 x 66 mm outline spans Y=-6..60.
Run once on the 70 x 60 mm draft, then refill zones with pcbnew.
"""
from pathlib import Path
import shutil
from sexpr import parse, get, children, dump, Atom

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / 'amigahid-pico.kicad_pcb'
board = parse(path.read_text())
edges = [x for x in children(board, 'gr_line')
         if get(x, 'layer')[1] == 'Edge.Cuts']
ys = [float(get(x, end)[2]) for x in edges for end in ('start', 'end')]
if min(ys) == -6 and max(ys) == 60:
    raise SystemExit('Outline is already 70 x 66 mm.')
assert min(ys) == 0 and max(ys) == 60, 'Unexpected outline; review first'
backup = ROOT / 'review/r7-before-66mm-extension.kicad_pcb'
assert not backup.exists(), 'Preserve the previous backup before another conversion'
shutil.copy2(path, backup)
for edge in edges:
    for end in ('start', 'end'):
        point = get(edge, end)
        if float(point[2]) == 0:
            point[2] = Atom('-6')
for zone in children(board, 'zone'):
    if get(zone, 'keepout') is not None:
        continue
    assert get(zone, 'net')[1] == 'GND'
    for polygon in children(zone, 'polygon'):
        for point in children(get(polygon, 'pts'), 'xy'):
            if float(point[2]) == 0:
                point[2] = Atom('-6')
path.write_text(dump(board) + '\n')
print('Extended DB9 edge to Y=-6 mm: 70 x 66 mm, placement and routes retained.')
