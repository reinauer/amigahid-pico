#!/usr/bin/env python3
"""Copy reviewed SOURCING.csv fields to schematic, PCB, and both BOMs.
Does not select/substitute parts or change footprints/routing. Stock is a dated
snapshot; refresh and verify SOURCING.csv against LCSC before reusing it.
"""
from pathlib import Path
import csv, json, re
from sexpr import parse, children
R = Path(__file__).resolve().parents[1]
rows = {r['Reference']: r for r in csv.DictReader((R/'SOURCING.csv').open())}
def spans(s):
    depth = 0
    for m in re.finditer(r'"(?:\\.|[^"\\])*"|[()]', s):
        if m.group() == '(':
            if depth == 1: start = m.start()
            depth += 1
        elif m.group() == ')':
            depth -= 1
            if depth == 1: yield start, m.end()
for filename, kind in [('amigahid-pico.kicad_sch', 'symbol'), ('amigahid-pico.kicad_pcb', 'footprint')]:
    path = R/filename
    s = path.read_text()
    edits = []
    for a, z in spans(s):
        block = s[a:z]
        if not re.match(r'\('+kind+r'\s', block): continue
        props = {v[1]: v[2] for v in children(parse(block), 'property')}
        row = rows.get(props.get('Reference'))
        if row is None: continue
        fields = {'LCSC Part #': row['LCSC Part #'],
                  'Manufacturer': row['Manufacturer'],
                  'MPN': row['MPN'], 'Datasheet': row['Datasheet'],
                  'LCSC Stock': row['Stock at check'], 'Stock checked UTC': row['Checked UTC'],
                  'Sourcing note': row['Fit notes']}
        if props['Reference'] == 'U1' or re.fullmatch(r'J[0-9]+|JP[0-9]+', props['Reference']):
            fields['FT Rotation Offset'] = '90'
        rotation_overrides = {'U1': '270', 'C4': '180', 'J7': '0'}
        if props['Reference'] in rotation_overrides:
            fields['FT Rotation Offset'] = rotation_overrides[props['Reference']]
        for name, value in fields.items():
            pattern = r'(\(property\s+'+re.escape(json.dumps(name))+r'\s+)"(?:\\.|[^"\\])*"'
            if re.search(pattern, block):
                block = re.sub(pattern, lambda m: m[1]+json.dumps(value), block, count=1)
            else:
                extra = f'\n(property {json.dumps(name)} {json.dumps(value)} (at 0 0 0) '
                extra += '(layer "F.Fab") (hide yes) ' if kind == 'footprint' else ''
                extra += '(effects (font (size 1 1))'+(' (hide yes)' if kind == 'symbol' else '')+'))\n'
                block = block[:-1]+extra+')'
        edits.append((a, z, block))
    for a, z, block in reversed(edits): s = s[:a]+block+s[z:]
    path.write_text(s)
bom = []
for sym in children(parse((R/'amigahid-pico.kicad_sch').read_text()), 'symbol'):
    p = {v[1]:v[2] for v in children(sym,'property')}
    if p.get('Reference') not in rows: continue
    bom.append({key:p.get(key,'') for key in ['Reference','Value','Footprint','Datasheet','LCSC Part #','Manufacturer','MPN','LCSC Stock','Stock checked UTC','Sourcing note']})
with (R/'BOM.csv').open('w') as f:
    w=csv.DictWriter(f,fieldnames=list(bom[0]));w.writeheader();w.writerows(bom)
path=R/'production/bom.csv'
if path.exists():
    jlc=list(csv.DictReader(path.open(encoding='utf-8-sig')))
    byref={r['Reference']:r for r in bom}
    for row in jlc:
        refs=[r.strip() for r in row['Designator'].split(',')]
        parts=[byref[r] for r in refs]
        assert len({p['LCSC Part #'] for p in parts})==1, refs
        row['LCSC Part #']=parts[0]['LCSC Part #']
        row['Value']=parts[0]['Value']
        row['Footprint']=parts[0]['Footprint'].split(':')[-1]
    with path.open('w',encoding='utf-8-sig') as f:
        w=csv.DictWriter(f,fieldnames=list(jlc[0]));w.writeheader();w.writerows(jlc)
print(f'Synced sourcing for {len(rows)} components ({sum(bool(r["LCSC Part #"]) for r in rows.values())} LCSC selections).')
