#!/usr/bin/env python3
"""Restore reviewed pin-1/polarity silk on libraries and placed footprints.

Marker locations are local footprint coordinates, checked against the routed
board's solder-mask openings. No copper, placement or net data is changed.
"""
from pathlib import Path
import json
import uuid
from sexpr import parse, children, get, dump

R = Path(__file__).resolve().parents[1]
markers = json.loads((R/'assembly-markers.json').read_text())


def apply(fp, key):
    m = markers[key]
    identity = 'r7/orientation/' + key
    ids = {str(uuid.uuid5(uuid.NAMESPACE_URL, identity + '/' + str(i)))
           for i in range(2)}
    fp[:] = [a for a in fp if not (isinstance(a, list) and
             get(a, 'uuid') and get(a, 'uuid')[1] in ids)]
    x, y = m['at']
    shapes = []
    if m['kind'] == 'positive':
        shapes = [f'(fp_line (start {x-.3} {y}) (end {x+.3} {y})',
                  f'(fp_line (start {x} {y-.3}) (end {x} {y+.3})']
    elif m['kind'] == 'cathode':
        shapes = [f'(fp_line (start {x} {y-.25}) (end {x} {y+.25})']
    else:
        shapes = [f'(fp_circle (center {x} {y}) (end {x+.15} {y}) (fill solid)']
    for i, shape in enumerate(shapes):
        uid = str(uuid.uuid5(uuid.NAMESPACE_URL, identity + '/' + str(i)))
        fp.append(parse(shape + ' (stroke (width 0.12) (type default)) '
                        f'(layer "F.SilkS") (uuid "{uid}"))'))


for key in markers:
    lib, name = key.split(':')
    path = R/(lib+'.pretty')/(name+'.kicad_mod')
    fp = parse(path.read_text())
    apply(fp, key)
    path.write_text(dump(fp)+'\n')
path = R/'amigahid-pico.kicad_pcb'
board = parse(path.read_text())
count = 0
for fp in children(board, 'footprint'):
    if fp[1] in markers:
        apply(fp, fp[1])
        count += 1
path.write_text(dump(board)+'\n')
print('Restored orientation markers on', count, 'footprints.')
