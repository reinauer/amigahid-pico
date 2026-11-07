#!/usr/bin/env python3
"""Adopt stocked C77848 using its supplier drawing, preserving other routing.

The supplier import has oversized shield holes and a misplaced central tab.
Coordinates below are derived from C-DS1093-01-XXXX-B, revision A, 2019-05-07.
The front face remains 8.08 mm forward of the original r6 footprint origin.
Signal numbering agrees with the drawing and the supplier's six-pin import.
"""
import hashlib
import json
import uuid
from pathlib import Path
from sexpr import Atom, children, dump, get, parse

R = Path(__file__).resolve().parents[1]
name = 'CONN-TH_DS1093-01-PN60'
fp = parse((R / 'vendor-originals' / (name + '.kicad_mod')).read_text())
fp[:] = [x for x in fp if not (isinstance(x, list) and
          (str(x[0]).startswith('fp_') or x[0] in ['model', 'pad']))]
fp[1] = name
positions = {'1': (1.3, -.66), '2': (-1.3, -.66),
             '3': (3.4, -.66), '4': (-3.4, -.66),
             '5': (3.4, -3.16), '6': (-3.4, -3.16)}
for n, (x, y) in positions.items():
    fp.append(parse(f'(pad "{n}" thru_hole circle (at {x} {y}) '
                    '(size 1.7 1.7) (drill 1.1) (layers "*.Cu" "*.Mask"))'))
for x, y in [(-6.85, 2.58), (6.85, 2.58), (0, 3.30)]:
    fp.append(parse(f'(pad "0" thru_hole circle (at {x} {y}) '
                    '(size 3.3 3.3) (drill 2.3) (layers "*.Cu" "*.Mask"))'))
fp.append(parse('(fp_rect (start -7 -4.92) (end 7 8.08) '
                '(stroke (width 0.1) (type default)) (fill none) (layer "F.Fab"))'))
fp.append(parse('(fp_rect (start -8.75 -5.17) (end 8.75 8.33) '
                '(stroke (width 0.05) (type default)) (fill none) (layer "F.CrtYd"))'))
fp.append(parse('(fp_text user "1" (at 1.3 1.1) (layer "F.Fab") '
                '(effects (font (size 0.7 0.7) (thickness 0.1))))'))
(R / 'lcsc.pretty' / (name + '.kicad_mod')).write_text(dump(fp) + '\n')

data = json.loads((R / 'circuit.json').read_text())
c = next(x for x in data['parts'] if x['ref'] == 'J7')
assert c['lcsc'] in ['C7428703', 'C77848']
c.update(lcsc='C77848', mpn='DS1093-01-PN60', manufacturer='CONNFLY',
         footprint='lcsc:' + name,
         datasheet='https://datasheet.lcsc.com/datasheet/pdf/91b259a35bf42e0e40c79aca63cab966.pdf?productCode=C77848',
         source_note='C77848 replaces unavailable r6 HOOYA C7428703. Supplier drawing sets 13.70mm shield pitch, 2.30mm shield holes and 4.78mm central-tab setback. Standard mini-DIN numbering retained; front face unchanged.')
(R / 'circuit.json').write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n')

board = parse((R / 'amigahid-pico.kicad_pcb').read_text())
bfp = next(x for x in children(board, 'footprint') if
           any(p[1:3] == ['Reference', 'J7'] for p in children(x, 'property')))
nets = {p[1]: get(p, 'net') for p in children(bfp, 'pad')}
bfp[1] = c['footprint']
bfp[:] = [x for x in bfp if not (isinstance(x, list) and
           (str(x[0]).startswith('fp_') or x[0] in ['pad', 'model']))]
fields = {'Value': c['value'], 'Datasheet': c['datasheet'],
          'LCSC Part #': c['lcsc'], 'MPN': c['mpn'],
          'Manufacturer': c['manufacturer'],
          'Source': 'https://www.lcsc.com/product-detail/C77848.html',
          'Sourcing note': c['source_note']}
for p in children(bfp, 'property'):
    if p[1] in fields:
        p[2] = fields[p[1]]
for item in fp:
    if not isinstance(item, list) or not (str(item[0]).startswith('fp_') or item[0] == 'pad'):
        continue
    if item[0] == 'pad' and nets.get(item[1]):
        item.append(nets[item[1]])
    if item[0] in ['pad', 'fp_text']:
        at = get(item, 'at')
        at[3:] = [Atom(str(float(get(bfp, 'at')[3])))]
    item.append(parse('(uuid "' + str(uuid.uuid5(
        uuid.NAMESPACE_URL, 'r7/connfly/' + dump(item))) + '")'))
    bfp.append(item)
(R / 'amigahid-pico.kicad_pcb').write_text(dump(board) + '\n')

models = json.loads((R / 'models/manifest.json').read_text())
step = R / 'vendor-models/C77848/model.3dshapes' / (name + '.step')
models['C77848'] = {
    'file': str(step.relative_to(R)), 'offset': [0, -1.58, 0],
    'rotate': [0, 0, 0], 'source': 'https://www.lcsc.com/product-detail/C77848.html',
    'kind': 'LCSC/EasyEDA supplier model',
    'sha256': hashlib.sha256(step.read_bytes()).hexdigest(),
    'note': 'Aligned front face to drawing-based footprint at +8.08mm; STEP housing height is 13.0mm versus drawing 12.8mm. Drawing controls holes and seating.'}
(R / 'models/manifest.json').write_text(json.dumps(models, indent=2) + '\n')
print('Adopted C77848; original front face and standard signal numbers retained.')
