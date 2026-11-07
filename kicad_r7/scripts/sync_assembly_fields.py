#!/usr/bin/env python3
"""Sync BOM identity and explicit assembly rotations without moving copper."""
import json
from pathlib import Path
from sexpr import parse, children, get, dump

R = Path(__file__).resolve().parents[1]
parts = {c['ref']: c for c in json.loads((R/'circuit.json').read_text())['parts']}
orient = json.loads((R/'assembly-orientation.json').read_text())['parts']
path = R/'amigahid-pico.kicad_pcb'
board = parse(path.read_text())
for fp in children(board, 'footprint'):
    props = {p[1]: p for p in children(fp, 'property')}
    c = parts[props['Reference'][2]]
    props['Value'][2] = c['value']
    fields = {'Function': c.get('function', '')}
    if c.get('lcsc'):
        fields['FT Rotation Offset'] = str(orient[c['lcsc']]['offset_deg'])
    for name, value in fields.items():
        if name in props:
            props[name][2] = value
        else:
            fp.append(parse(f'(property {json.dumps(name)} {json.dumps(value)} '
                            '(at 0 0 0) (layer "F.Fab") (hide yes) '
                            '(effects (font (size 1 1) (thickness 0.15))))'))
path.write_text(dump(board)+'\n')
print('Synchronized BOM values, function fields and explicit assembly rotations.')
