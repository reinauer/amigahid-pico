#!/usr/bin/env python3
"""Record reproducible layout checks; complements KiCad ERC/DRC, not SI tests.

Run with KiCad's pcbnew Python and Shapely after the final zone fill.
"""
from pathlib import Path
import collections
import hashlib
import json
import pcbnew as p
from shapely.geometry import LineString, Point
from shapely.ops import unary_union

R = Path(__file__).resolve().parents[1]
board_file = R / 'amigahid-pico.kicad_pcb'
b = p.LoadBoard(str(board_file))
assert b
parts = {c['ref']: c for c in json.loads((R / 'circuit.json').read_text())['parts']}
fps = {f.GetReference(): f for f in b.GetFootprints()}
assert set(fps) == {ref for ref in parts if not ref.startswith('#')}
tracks = list(b.GetTracks())


def xy(v):
    return tuple(x / 1e6 for x in v)


def is_usb(t):
    return 'USB' in t.GetNetname() and '_D' in t.GetNetname()


def line(t):
    return LineString([xy(t.GetStart()), xy(t.GetEnd())])


purchased = [f for ref, f in fps.items() if parts[ref].get('lcsc')]
models = []
for f in purchased:
    attached = list(f.Models())
    assert len(attached) == 1, f.GetReference()
    path = Path(attached[0].m_Filename.replace('${KIPRJMOD}', str(R)))
    assert path.is_file(), str(path)
    models.append({'ref': f.GetReference(), 'file': str(path.relative_to(R))})
bottom_parts = [f.GetReference() for f in purchased if f.GetLayer() != p.F_Cu]
bottom_silk = [x.m_Uuid.AsString() for x in b.GetDrawings() if x.GetLayer() == p.B_SilkS]
for f in fps.values():
    bottom_silk += [x.m_Uuid.AsString() for x in f.GraphicalItems()
                    if x.GetLayer() == p.B_SilkS]
    bottom_silk += [x.m_Uuid.AsString() for x in f.GetFields()
                    if x.GetLayer() == p.B_SilkS and x.IsVisible()]
assert not bottom_parts and not bottom_silk
assert b.GetCopperLayerCount() == 4
assert not [t for t in tracks if not isinstance(t, p.PCB_VIA) and t.GetLayer() == p.In1_Cu]
assert not [t for t in tracks if not isinstance(t, p.PCB_VIA)
            and t.GetLayer() == p.In2_Cu and is_usb(t)]

usb_back = unary_union([line(t).buffer(t.GetWidth() / 2e6 + .6)
                       for t in tracks if not isinstance(t, p.PCB_VIA)
                       and t.GetLayer() == p.B_Cu and is_usb(t)])
crossings = [t.m_Uuid.AsString() for t in tracks
             if not isinstance(t, p.PCB_VIA) and t.GetLayer() == p.In2_Cu
             and usb_back.intersects(line(t))]
assert not crossings, crossings
ground = [Point(xy(t.GetPosition())) for t in tracks
          if isinstance(t, p.PCB_VIA) and t.GetNetname() == 'GND']
ground += [Point(xy(q.GetPosition())) for f in fps.values() for q in f.Pads()
           if q.GetDrillSize().x and q.GetNetname() == 'GND']
ground = unary_union(ground)
returns = [{'net': t.GetNetname(), 'position_mm': xy(t.GetPosition()),
            'nearest_ground_hole_mm': round(Point(xy(t.GetPosition())).distance(ground), 3)}
           for t in tracks if isinstance(t, p.PCB_VIA) and is_usb(t)]

power = {}
for net in ['AMIGA_5V', 'USB_C_VBUS', 'SYS_5V', 'HOST_POWER_RAW', 'USB_5V', '+3V3']:
    routes = [t for t in tracks if not isinstance(t, p.PCB_VIA) and t.GetNetname() == net]
    # Sum all branches: an upper bound for one path's track resistance at
    # nominal copper thickness, excluding vias, pads, temperature and tolerances.
    resistance = sum(.01724 * t.GetLength() / 1e6 /
                     (t.GetWidth() / 1e6 * (35 if t.GetLayer() in [p.F_Cu, p.B_Cu] else 15.2))
                     for t in routes)
    power[net] = {'all_branches_length_mm': round(sum(t.GetLength() for t in routes) / 1e6, 3),
                  'all_branches_track_resistance_ohm_at_20C_nominal_copper': round(resistance, 4)}

drills = {}
for ref in ['J4', 'J7']:
    drills[ref] = [{'pin': q.GetNumber(), 'position_mm': xy(q.GetPosition()),
                    'drill_mm': xy(q.GetDrillSize())} for q in fps[ref].Pads()]
for q in fps['J4'].Pads():
    assert abs(q.GetDrillSize().x / 1e6 - (2.3 if q.GetNumber() == '9' else .92)) < .0001
for q in fps['J7'].Pads():
    assert abs(q.GetDrillSize().x / 1e6 - (2.3 if q.GetNumber() == '0' else 1.1)) < .0001

edge_points = [xy(point) for d in b.GetDrawings() if d.GetLayer() == p.Edge_Cuts
               for point in [d.GetStart(), d.GetEnd()]]
minimum = [min(point[i] for point in edge_points) for i in range(2)]
size = [max(point[i] for point in edge_points) - minimum[i] for i in range(2)]
outline = {'minimum_mm': minimum, 'size_mm': size}
assert size == [70, 66]
report = {
    'board_sha256': hashlib.sha256(board_file.read_bytes()).hexdigest(),
    'outline': outline, 'copper_layers': 4, 'purchased_parts': len(purchased),
    'unique_lcsc_codes': len({c['lcsc'] for c in parts.values() if c.get('lcsc')}),
    'bottom_assembled_parts': bottom_parts, 'bottom_silkscreen_objects': bottom_silk,
    'models': models, 'usb_reference_corridor_crossings': crossings,
    'usb_transition_ground_distances': returns, 'power_track_estimates': power,
    'connector_drills': drills,
    'vias_to_fill_and_cap': [
        {'uuid': t.m_Uuid.AsString(), 'position_mm': xy(t.GetPosition()),
         'drill_mm': t.GetDrillValue() / 1e6, 'diameter_mm': t.GetWidth(p.F_Cu) / 1e6,
         'net': t.GetNetname()}
        for t in tracks if isinstance(t, p.PCB_VIA)],
    'limits': ['Geometric checks do not establish impedance, EMI, thermal or physical performance.',
               'USB corridor check excludes unavoidable pad/via antipads; inspect filled planes too.',
               'Power sums include all branches and exclude components, vias and connectors.']}
(R / 'review/board-audit.json').write_text(json.dumps(report, indent=2) + '\n')
print('Checked', len(purchased), 'top-side parts; models, outline, drills and USB corridors pass.')
print('USB ground transitions:', returns)
