#!/usr/bin/env python3
"""Export grouped BOM, explicit JLC rotations and a numbered-pad assembly map.

Run using KiCad's pcbnew Python. SMD anchors and THT pad centroids match
Fabrication Toolkit's default origins. This does not save a board.
"""
from pathlib import Path
import csv
import hashlib
import html
import json
import math
import re
import pcbnew as p

R = Path(__file__).resolve().parents[1]
board = R/'amigahid-pico.kicad_pcb'
b = p.LoadBoard(str(board))
parts = {c['ref']: c for c in json.loads((R/'circuit.json').read_text())['parts'] if c.get('lcsc')}
orient = json.loads((R/'assembly-orientation.json').read_text())['parts']
markers = json.loads((R/'assembly-markers.json').read_text())
options = json.loads((R/'fabrication-toolkit-options.json').read_text())
assert options['AUTO TRANSLATE'] is False, 'Generic package translations conflict with LCSC orientations'
out = R/'production'
out.mkdir(exist_ok=True)
fps = {f.GetReference(): f for f in b.GetFootprints()}
groups = {}
positions = []
detail = []
origin = b.GetDesignSettings().GetAuxOrigin()


def natural(s):
    return [int(t) if t.isdigit() else t for t in re.split('(\d+)', s)]


def padbox(pad):
    bb = pad.GetBoundingBox()
    return [bb.GetLeft()/1e6, bb.GetTop()/1e6,
            bb.GetRight()/1e6, bb.GetBottom()/1e6]


for ref in sorted(parts, key=natural):
    c, f = parts[ref], fps[ref]
    assert f.GetLayer() == p.F_Cu
    assert f.GetValue() == c['value']
    offset = orient[c['lcsc']]['offset_deg']
    assert float(f.GetField('FT Rotation Offset').GetText()) == offset
    assert f.GetField('LCSC Part #').GetText() == c['lcsc']
    assert str(f.GetFPID().GetLibNickname())+':'+str(f.GetFPID().GetLibItemName()) == c['footprint']
    boxes = [padbox(d) for d in f.Pads()]
    center = [(min(v[i] for v in boxes)+max(v[i+2] for v in boxes))/2 for i in range(2)]
    if f.GetAttributes() & p.FP_SMD:
        center = [v/1e6 for v in f.GetPosition()]
    angle = (f.GetOrientationDegrees()+offset) % 360
    positions.append([ref, round(center[0]-origin.x/1e6,6),
                      round(-center[1]+origin.y/1e6,6), angle, 'top'])
    groups.setdefault(c['lcsc'], []).append(c)
    pin1 = next((d for d in f.Pads() if d.GetNumber() == '1'), None)
    if c['footprint'] in markers:
        detail.append({'ref': ref, 'lcsc': c['lcsc'], 'native_deg': f.GetOrientationDegrees(),
                       'offset_deg': offset, 'jlc_deg': angle,
                       'pin1_mm': [round(v/1e6,4) for v in pin1.GetPosition()],
                       'pin1_net': pin1.GetNetname()})
with (out/'positions.csv').open('w', newline='', encoding='utf-8-sig') as file:
    w = csv.writer(file)
    w.writerow(['Designator','Mid X','Mid Y','Rotation','Layer'])
    w.writerows(positions)
with (out/'bom.csv').open('w', newline='', encoding='utf-8-sig') as file:
    w = csv.writer(file)
    w.writerow(['Designator','Footprint','Quantity','Value','LCSC Part #'])
    for code, group in sorted(groups.items()):
        assert len({(c['value'],c['footprint'],c['mpn']) for c in group}) == 1
        c = group[0]
        w.writerow([','.join(c['ref'] for c in group),c['footprint'].split(':')[1],
                    len(group),c['value'],code])

# Each card shows the actual board orientation, not a generic library view.
cols = 5
rows = math.ceil(len(detail)/cols)
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1500" height="{170+rows*240}" '
       f'viewBox="0 0 1500 {170+rows*240}">',
       '<rect width="100%" height="100%" fill="white"/>',
       '<g font-family="sans-serif" fill="#17202a">',
       '<text x="25" y="35" font-size="26">AmigaHID r7 - assembly orientation</text>',
       '<text x="25" y="65" font-size="18">Top view as placed: DB9 edge above, USB-C edge below. Purple = pad 1.</text>',
       '<text x="25" y="90" font-size="17">D2/D3: pad 1 is cathode. C4: pad 1 is positive. L1: dotted end is pad 1 (+1V1).</text>',
       '<text x="25" y="115" font-size="17">Black marks show the new physical silkscreen. Numbers identify the copper pads.</text>']
for i, d in enumerate(detail):
    f = fps[d['ref']]
    cx, cy = (i%cols)*300, 140+(i//cols)*240
    svg.append(f'<rect x="{cx+5}" y="{cy}" width="290" height="230" rx="8" fill="#f5f7fa" stroke="#ccd1d9"/>')
    svg.append(f'<text x="{cx+18}" y="{cy+25}" font-size="20" font-weight="bold">{d["ref"]} · {d["lcsc"]}</text>')
    pads = [pad for pad in f.Pads() if pad.GetNumber()]
    bounds = [padbox(pad) for pad in pads]
    x1,y1,x2,y2 = min(v[0] for v in bounds),min(v[1] for v in bounds),max(v[2] for v in bounds),max(v[3] for v in bounds)
    sc = min(235/(x2-x1+2),145/(y2-y1+2))
    tx,ty = cx+150-sc*(x1+x2)/2,cy+118-sc*(y1+y2)/2
    for pad, (x,y,xx,yy) in zip(pads,bounds):
        color = '#a328c6' if pad.GetNumber()=='1' else '#b4beca'
        svg.append(f'<rect x="{tx+x*sc}" y="{ty+y*sc}" width="{(xx-x)*sc}" height="{(yy-y)*sc}" fill="{color}" stroke="#59636e" stroke-width="0.5"/>')
        font = max(4,min(11,(xx-x)*sc*.7,(yy-y)*sc*.7))
        svg.append(f'<text x="{tx+(x+xx)*sc/2}" y="{ty+(y+yy)*sc/2+font*.3}" text-anchor="middle" font-size="{font}">{html.escape(pad.GetNumber())}</text>')
    for shape in f.GraphicalItems():
        if shape.GetLayer()!=p.F_SilkS:continue
        if shape.GetShape()==p.SHAPE_T_CIRCLE:
            v=shape.GetCenter()
            svg.append(f'<circle cx="{tx+v.x/1e6*sc}" cy="{ty+v.y/1e6*sc}" r="{.21*sc}" fill="#111"/>')
        elif shape.GetShape()==p.SHAPE_T_SEGMENT:
            a,z=shape.GetStart(),shape.GetEnd()
            svg.append(f'<line x1="{tx+a.x/1e6*sc}" y1="{ty+a.y/1e6*sc}" x2="{tx+z.x/1e6*sc}" y2="{ty+z.y/1e6*sc}" stroke="#111" stroke-width="{.12*sc}"/>')
    label_net = 'NC' if d['pin1_net'].startswith('unconnected-') else d['pin1_net']
    svg.append(f'<text x="{cx+15}" y="{cy+208}" font-size="13">Pad 1: {html.escape(label_net)} · CPL {d["jlc_deg"]:g}°</text>')
svg += ['</g></svg>']
(out/'assembly-orientation.svg').write_text('\n'.join(svg)+'\n')
report = {'board_sha256':hashlib.sha256(board.read_bytes()).hexdigest(),
          'bom_groups':len(groups),'assembled_parts':len(parts),'orientation':detail,
          'preview_limit':'RM2 and DE9 lack useful JLC placement models; use numbered pads and supplier drawings for factory CAM review.'}
(R/'review/assembly-audit.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'Exported {len(groups)} unique BOM rows, {len(positions)} placements and {len(detail)} orientation cards.')
