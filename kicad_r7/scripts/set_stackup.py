#!/usr/bin/env python3
"""Record the selected JLC04101H-7628 nominal 1 mm four-layer construction.

2026-10-14 calculator result: outer 90-ohm differential pair with solder mask,
0.2474 mm trace width and 0.1501 mm edge gap. Fabricator must confirm this
construction and impedance at quotation; this is not a measured coupon.
"""
from pathlib import Path
from sexpr import parse, get, dump
R=Path(__file__).resolve().parents[1]
f=R/'amigahid-pico.kicad_pcb';a=parse(f.read_text());setup=get(a,'setup')
setup[:]=[x for x in setup if not(isinstance(x,list) and x[0]=='stackup')]
setup.append(parse('''(stackup
 (layer "F.SilkS" (type "Top Silk Screen"))
 (layer "F.Paste" (type "Top Solder Paste"))
 (layer "F.Mask" (type "Top Solder Mask") (thickness 0.01524) (epsilon_r 3.8) (loss_tangent 0.02))
 (layer "F.Cu" (type "copper") (thickness 0.035))
 (layer "dielectric 1" (type "prepreg") (thickness 0.2104) (material "7628") (epsilon_r 4.4) (loss_tangent 0.02))
 (layer "In1.Cu" (type "copper") (thickness 0.0152))
 (layer "dielectric 2" (type "core") (thickness 0.5) (material "FR4") (epsilon_r 4.6) (loss_tangent 0.02))
 (layer "In2.Cu" (type "copper") (thickness 0.0152))
 (layer "dielectric 3" (type "prepreg") (thickness 0.2104) (material "7628") (epsilon_r 4.4) (loss_tangent 0.02))
 (layer "B.Cu" (type "copper") (thickness 0.035))
 (layer "B.Mask" (type "Bottom Solder Mask") (thickness 0.01524) (epsilon_r 3.8) (loss_tangent 0.02))
 (layer "B.Paste" (type "Bottom Solder Paste"))
 (layer "B.SilkS" (type "Bottom Silk Screen"))
 (copper_finish "ENIG") (dielectric_constraints yes))'''))
f.write_text(dump(a)+'\n')
print('Recorded JLC04101H-7628 stackup and ENIG finish.')
