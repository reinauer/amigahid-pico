#!/usr/bin/env python3
"""Export a locally reviewable factory quotation package; never uploads it.

Requires fresh KiCad ERC/DRC and audit_board.py results. Run using pcbnew
Python. Assembly rotations are explicit, reviewed JLC part conventions.
"""
from pathlib import Path
import csv
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile

R = Path(__file__).resolve().parents[1]
cli = os.environ.get('KICAD_CLI', '/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli')
board = R / 'amigahid-pico.kicad_pcb'
digest = hashlib.sha256(board.read_bytes()).hexdigest()
audit = json.loads((R / 'review/board-audit.json').read_text())
assert audit['board_sha256'] == digest, 'Run audit_board.py on the current board'
drc_file = R / 'review/routed-drc.json'
erc_file = R / 'review/erc.json'
drc = json.loads(drc_file.read_text())
erc = json.loads(erc_file.read_text())
assert not (drc['violations'] or drc['unconnected_items'] or drc['schematic_parity'])
assert not any(s['violations'] for s in erc['sheets'])
assert drc_file.stat().st_mtime >= board.stat().st_mtime
assert erc_file.stat().st_mtime >= max(f.stat().st_mtime for f in R.glob('*.kicad_sch'))
subprocess.run([sys.executable, str(R/'scripts/export_assembly.py')], check=True)
parts = [c for c in json.loads((R / 'circuit.json').read_text())['parts'] if c.get('lcsc')]
byref = {c['ref']: c for c in parts}
out = R / 'production' / ('factory-review-' + digest[:12])
out.mkdir(parents=True, exist_ok=True)
fab = out / 'fabrication'
fab.mkdir(exist_ok=True)


def run(*args):
    subprocess.run([cli, *map(str, args)], check=True)


run('pcb', 'export', 'gerbers', '--layers',
    'F.Cu,In1.Cu,In2.Cu,B.Cu,F.Mask,B.Mask,F.Paste,F.SilkS,Edge.Cuts',
    '--subtract-soldermask', '--output', str(fab) + '/', board)
run('pcb', 'export', 'drill', '--excellon-separate-th', '--generate-map',
    '--map-format', 'pdf', '--generate-report', '--report-path', fab / 'drill-report.txt',
    '--output', str(fab) + '/', board)
run('pcb', 'export', 'ipcd356', '--output', fab / 'amigahid-pico.d356', board)
run('pcb', 'export', 'pos', '--format', 'csv', '--units', 'mm', '--side', 'front',
    '--exclude-dnp', '--output', out / 'positions-kicad.csv', board)
positions = list(csv.DictReader((out / 'positions-kicad.csv').open()))
positions = [r for r in positions if r['Ref'] in byref]
assert {r['Ref'] for r in positions} == set(byref)
shutil.copy2(R/'production/positions.csv', out/'CPL.csv')
shutil.copy2(R/'production/bom.csv', out/'BOM.csv')
shutil.copy2(R/'production/assembly-orientation.svg', out/'assembly-orientation.svg')
groups = {}
for c in parts:
    groups.setdefault(c['lcsc'], []).append(c)
with (out / 'BOM-assembly.csv').open('w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['Comment', 'Designator', 'Footprint', 'LCSC Part #'])
    for code, group in sorted(groups.items()):
        assert len({c['footprint'] for c in group}) == 1
        w.writerow([group[0]['mpn'], ','.join(c['ref'] for c in group), group[0]['footprint'], code])
with (out / 'via-fill-locations.csv').open('w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['UUID', 'KiCad X mm', 'KiCad Y mm', 'Finished drill mm', 'Copper diameter mm', 'Net'])
    for v in audit['vias_to_fill_and_cap']:
        w.writerow([v['uuid'], *v['position_mm'], v['drill_mm'], v['diameter_mm'], v['net']])

run('sch', 'export', 'pdf', '--output', out / 'schematic.pdf', R / 'amigahid-pico.kicad_sch')
run('pcb', 'export', 'step', '--force', '--subst-models', '--output', out / 'board.step', board)
run('pcb', 'export', 'pdf', '--layers', 'F.Cu,In1.Cu,In2.Cu,B.Cu,F.Fab',
    '--common-layers', 'Edge.Cuts', '--mode-multipage', '--scale', '0',
    '--output', out / 'board-layers.pdf', board)

# Provide native source for via identification and component orientation.
native = out / 'kicad_r7'
native.mkdir(exist_ok=True)
sources = list(R.glob('*.kicad_sch')) + list(R.glob('*.kicad_sym'))
sources += [board, R / 'amigahid-pico.kicad_pro', R / 'fp-lib-table', R / 'sym-lib-table']
sources += [R / n for n in ['BOM.csv', 'SOURCING.csv', 'circuit.json', 'README.md',
                           'assembly-accessories.json', 'manufacturing-requirements.json',
                           'electrical-review.json', 'assembly-stock-check.json', 'stock-check.json']]
sources += list((R / 'lcsc.pretty').glob('*.kicad_mod')) + list((R / 'r7.pretty').glob('*.kicad_mod'))
sources += [R / v['file'] for v in audit['models']]
sources += [R / 'models/manifest.json', R / 'review/board-audit.json', drc_file, erc_file]
sources += [R/n for n in ['assembly-orientation.json', 'assembly-markers.json',
                          'fabrication-toolkit-options.json', 'review/assembly-audit.json']]
sources += list((R / 'models').glob('*LICENSE*'))
sources += list((R / 'reference').glob('*.pdf')) + [R / 'reference/LICENSE.txt']
for src in set(sources):
    dst = native / src.relative_to(R)
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
case = R.parent / 'case_r7'
for src in list((case / 'build').glob('*.stl')) + [case / 'build/mesh-validation.json'] + list((case / 'reference').glob('oled-*-r6.png')):
    dst = out / 'case_r7' / src.relative_to(case)
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
stock = json.loads((R / 'assembly-stock-check.json').read_text())
cost = round(sum(len(s['refs']) * s['initialPrice'] for s in stock), 4)
(out / 'README.txt').write_text(
    'AmigaHID r7 - FACTORY QUOTATION / CAM REVIEW ONLY\n\n'
    'No purchase or manufacturing release is authorized by this package.\n'
    'Read kicad_r7/manufacturing-requirements.json and electrical-review.json.\n'
    'Four layers, nominal 1mm, ENIG, all electrical vias epoxy filled and copper capped.\n'
    f'All {len(parts)} PCB parts are assembled on top; factory must solder THT connectors.\n'
    'Use CPL.csv: explicit JLC rotations, with generic package translations disabled.\n'
    'Use BOM.csv or BOM-assembly.csv: both contain exactly one row per LCSC part.\n'
    'See assembly-orientation.svg for numbered pads and pin-1/polarity silk.\n'
    'Return a final CAM placement preview; RM2/DE9 models are missing in the web viewer.\n'
    'Retain the r6 SH1106 display specified by the included drawings and pinout.\n'
    'DE9 STEP is a substitute; supplier drawing controls purchased pin length.\n'
    'Verify stackup, impedance, 1mm connector retention, inventory, attrition and assembly process.\n'
    'Prototype testing and r7 firmware work remain; no complete-product proof is claimed.\n')
manifest = {'board_sha256': digest, 'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'status': 'quote_review_only', 'assembled_parts': len(parts), 'unique_lcsc_codes': len(groups),
            'initial_tier_parts_usd_per_board': cost,
            'cost_excludes': ['OLED', 'mounting hardware', 'PCB', 'assembly', 'setup', 'case', 'shipping', 'tax'],
            'files': {str(f.relative_to(out)): hashlib.sha256(f.read_bytes()).hexdigest()
                      for f in sorted(out.rglob('*')) if f.is_file() and f != out / 'manifest.json'}}
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
archive = out.with_suffix('.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for f in sorted(out.rglob('*')):
        if f.is_file(): z.write(f, f.relative_to(out))
# A dedicated Gerber archive is the PCB upload; BOM/CPL are uploaded separately.
with zipfile.ZipFile(R/'production/gerbers.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for f in sorted(fab.iterdir()):
        if f.suffix.startswith('.g') or f.suffix == '.drl':
            z.write(f, f.name)
# Keep the existing Fabrication Toolkit upload filename current as well.
shutil.copy2(R/'production/gerbers.zip',
             R/'production/AmigaHID_r7_-_RP2350A__RM2_-_PROTOTYPE_7.zip')
print('Factory review package:', archive)
print('Parts estimate USD:', cost)
