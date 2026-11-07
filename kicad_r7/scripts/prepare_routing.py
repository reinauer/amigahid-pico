#!/usr/bin/env python3
"""Add reviewed local routes to a freshly generated r7 placement.

Run once after build_board.py. This is not a general PCB synchronization tool.
"""
from pathlib import Path
import pcbnew as p
R=Path(__file__).resolve().parents[1]
b=p.LoadBoard(str(R/'amigahid-pico.kicad_pcb'))
nets={n.GetNetname():n for n in b.GetNetInfo().NetsByName().values()}
def v(x,y):return p.VECTOR2I(p.FromMM(x),p.FromMM(y))
def route(net,pts,width=.2,layer=p.F_Cu):
 for a,z in zip(pts,pts[1:]):
  t=p.PCB_TRACK(b);t.SetStart(v(*a));t.SetEnd(v(*z));t.SetWidth(p.FromMM(width));t.SetLayer(layer);t.SetNet(nets[net]);t.SetLocked(True);b.Add(t)
def via(net,x,y):
 t=p.PCB_VIA(b);t.SetPosition(v(x,y));t.SetWidth(p.FromMM(.45));t.SetDrill(p.FromMM(.2));t.SetViaType(p.VIATYPE_THROUGH);t.SetLayerPair(p.F_Cu,p.B_Cu);t.SetNet(nets[net]);t.SetLocked(True)
 t.SetFrontTentingMode(p.TENTING_MODE_TENTED);t.SetBackTentingMode(p.TENTING_MODE_TENTED);b.Add(t)
# GND vias occupy the paste-free cross on the RP2350 exposed land.
for x,y in [(31,28),(31,27.35),(32.3,28),(31,26.7),(31,29.3)]:via('GND',x,y)
via('GND',43.5,36)
# AP63203 local current loops, with a separate feedback return.
route('SYS_5V',[(26,47.5),(26.05,47.55),(26.05,50.3)],.5)
route('SYS_5V',[(26.05,49.8),(27,49.8),(27,50.3)],.25)
route('BUCK_SW',[(27,52.7),(27,54.1),(25.5,55.6)],.6)
route('BUCK_SW',[(27,52.7),(27,51.88),(29.5,51.88)],.2)
route('BUCK_BST',[(27.95,52.7),(29.48,52.7),(29.5,52.72)],.2)
route('+3V3',[(25.5,58.4),(30.1,58.4),(31,57.5),(34,57.5)],.8)
route('+3V3',[(34,57.5),(32.2,57.5),(32.2,50.6),(31.1,49.5),(28.75,49.5),(27.95,50.3)],.25)
for a,z in [((28,47.5),(28.8,47.5)),((26.05,52.7),(24.8,53.1)),((31,55.5),(31,54.7)),((34,55.5),(34,54.7))]:
 route('GND',[a,z],.4);via('GND',*z)
# Native USB fanout. Pair widths require confirmation against fab stack-up.
for net,x in [('USB_D+',29.9),('USB_D-',30.9)]:
 route(net,[(x,20.17),(x,19.2)],.2);via(net,x,19.2)
route('USB_D+',[(29.9,19.2),(28.5,17.8),(28.5,10.05)],.8,p.B_Cu)
route('USB_D-',[(30.9,19.2),(30.9,18.786),(29.5,17.386),(29.5,10.05)],.8,p.B_Cu)
for net,x in [('USB_D+',28.5),('USB_D-',29.5)]:via(net,x,10.05)
route('USB_D+',[(29.75,11.32),(29.75,11.1),(28.7,10.05),(28.5,10.05)])
route('USB_D-',[(30.25,11.32),(30.25,10.8),(29.5,10.05)])
# Service pair runs on the back, clear of OLED mounting holes and header pins.
route('USB_SERVICE_D+',[(29.25,12.68),(29.25,13),(28.65,13.6),(26.5,13.6)])
route('USB_SERVICE_D-',[(29.75,12.68),(29.75,13),(28.2,14.55),(27.5,14.55)])
via('USB_SERVICE_D+',26.5,13.6);via('USB_SERVICE_D-',27.5,14.55)
route('USB_SERVICE_D+',[(26.5,13.6),(17,13.6),(15.5,15.1),(15.5,48.6),(19,52.1),(56.5,52.1)],.8,p.B_Cu)
route('USB_SERVICE_D-',[(27.5,14.55),(17.3935,14.55),(16.45,15.4935),(16.45,48.2065),(19.3935,51.15),(56.5,51.15)],.8,p.B_Cu)
# Join the reversible Type-C USB2 lands without crossing D+ and D-.
for net,x,y in [('USB_SERVICE_D+',59.75,52.4),('USB_SERVICE_D+',60.75,52.4),('USB_SERVICE_D-',59.25,51.7),('USB_SERVICE_D-',60.25,51.7)]:
 route(net,[(x,53.53),(x,y)],.2);via(net,x,y)
route('USB_SERVICE_D+',[(56.5,52.1),(57.8,52.1),(58.1,52.4),(60.75,52.4)],.2,p.B_Cu)
route('USB_SERVICE_D-',[(56.5,51.15),(58.7,51.15),(59.25,51.7),(60.25,51.7)],.2,p.B_Cu)
# Hub crystal: short local traces; ground pours and local ground vias surround it.
route('HUB_XO',[(42.05,35.25),(41.1,35.25),(40.6,35.75),(40.6,37.65),(39.1,39.15)],.15)
route('HUB_XI',[(42.05,35.75),(41.4,35.75),(41.1,36.05),(41.1,41.7),(37.75,41.7),(36.9,40.85)],.15)
for x,y in [(39.8,40.85),(36.2,39.15)]:via('GND',x,y)
route('GND',[(39.1,40.85),(39.8,40.85)],.2)
route('GND',[(36.9,39.15),(36.2,39.15)],.2)
p.SaveBoard(str(R/'amigahid-pico.kicad_pcb'),b)
print('Added local power, thermal, USB service and hub crystal routes.')
