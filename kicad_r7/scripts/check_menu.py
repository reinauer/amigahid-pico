#!/usr/bin/env python3
"""Check menu contact wiring, pull-ups and locator holes on the actual PCB."""
import json
from pathlib import Path
import pcbnew as p
R=Path(__file__).resolve().parents[1]
parts={c['ref']:c for c in json.loads((R/'circuit.json').read_text())['parts']}
b=p.LoadBoard(str(R/'amigahid-pico.kicad_pcb'))
fps={f.GetReference():f for f in b.GetFootprints()}
def net(ref,pin):
    assert parts[ref]['nets'][pin] == next(q.GetNetname() for q in fps[ref].Pads() if q.GetNumber()==pin)
    return parts[ref]['nets'][pin]
for contact,gpio_pin,resistor,name in [('1','18','R27','MENU_LEFT_N'),('2','19','R28','MENU_RIGHT_N'),('T','27','R29','MENU_PUSH_N')]:
    assert net('SW2',contact)==net('U1',gpio_pin)==net(resistor,'2')==name
    assert net(resistor,'1')=='+3V3' and parts[resistor]['value']=='10k'
for pin in ['C','4','5','6','7']:assert net('SW2',pin)=='GND'
holes=[q for q in fps['SW2'].Pads() if not q.GetNumber()]
assert len(holes)==2
for q in holes:
    assert q.GetAttribute()==p.PAD_ATTRIB_NPTH
    assert q.GetDrillSize().x==q.GetDrillSize().y==p.FromMM(1.1)
for q in fps['SW2'].Pads():
    if q.GetNumber() in ['1','2','C','T']:
        assert q.GetSize().x==p.FromMM(1.0) and q.GetSize().y==p.FromMM(1.6)
assert abs(abs(fps['SW2'].GetOrientationDegrees())-180)<0.001
print('PASS: menu GPIO14/15/16, 10k pull-ups, grounded common/anchors and unplated locator holes')
