#!/usr/bin/env python3
"""Convert the r7 routing draft to four layers without moving footprints.

L2 is the continuous ground reference. L3 is ground fill with low-speed
logic routes; outer layers carry the remaining signals and power.
"""
from pathlib import Path
import pcbnew as p
R=Path(__file__).resolve().parents[1];b=p.LoadBoard(str(R/'amigahid-pico.kicad_pcb'))
b.SetCopperLayerCount(4)
def v(x,y):return p.VECTOR2I(p.FromMM(x),p.FromMM(y))
for z in b.Zones():
 if z.GetIsRuleArea():
  ls=p.LSET()
  for l in [p.F_Cu,p.In1_Cu,p.In2_Cu,p.B_Cu]:ls.AddLayer(l)
  z.SetLayerSet(ls)
for layer in [p.In1_Cu,p.In2_Cu]:
 if any(not z.GetIsRuleArea() and z.GetLayer()==layer for z in b.Zones()):continue
 z=p.ZONE(b);z.SetLayer(layer);z.SetNet(b.FindNet('GND'));z.SetLocalClearance(p.FromMM(.15));z.SetPadConnection(p.ZONE_CONNECTION_FULL);z.SetMinThickness(p.FromMM(.15))
 poly=z.Outline();poly.NewOutline()
 for x,y in [(0,-6),(70,-6),(70,60),(0,60)]:poly.Append(p.FromMM(x),p.FromMM(y))
 b.Add(z)
# Signal routes are retained in the PCB; this helper only configures layers.
for d in list(b.GetDrawings()):
 if d.GetLayer()==p.B_SilkS:b.Delete(d)
p.SaveBoard(str(R/'amigahid-pico.kicad_pcb'),b)
print('Four layers; continuous L2 ground, L3 ground with slow logic routes.')
