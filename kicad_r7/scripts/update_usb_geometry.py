#!/usr/bin/env python3
"""Replace the draft wide USB routes with the JLC04101H-7628 geometry.

JLC's calculator (2026-10-14): 90 ohm, mask, .15 mm spacing gives
.2474 mm width and .1501 mm spacing. Fanout uses the existing vias.
"""
from pathlib import Path
import pcbnew as p
from shapely.geometry import LineString
R=Path(__file__).resolve().parents[1];f=R/'amigahid-pico.kicad_pcb';b=p.LoadBoard(str(f))
W=.2474;G=.1501
nets=['USB_D+','USB_D-','USB_SERVICE_D+','USB_SERVICE_D-']
# Only the former .8 mm main routes are replaced; connector breakout retained.
old=[t for t in b.GetTracks() if isinstance(t,p.PCB_TRACK) and not isinstance(t,p.PCB_VIA) and t.GetNetname() in nets and t.GetLayer()==p.B_Cu and t.GetWidth()>p.FromMM(.7)]
if not old:raise SystemExit('No old wide routes: conversion already applied.')
for t in old:b.Delete(t)
def route(net,pts):
 for a,z in zip(pts,pts[1:]):
  if a==z:continue
  t=p.PCB_TRACK(b);t.SetStart(p.VECTOR2I(p.FromMM(a[0]),p.FromMM(a[1])));t.SetEnd(p.VECTOR2I(p.FromMM(z[0]),p.FromMM(z[1])));t.SetLayer(p.B_Cu);t.SetWidth(p.FromMM(W));t.SetNet(b.FindNet(net));b.Add(t)
base=LineString([(25.8,13.6),(17,13.6),(15.5,15.1),(15.5,48.6),(19,52.1),(55.8,52.1)])
for net,d,start,end in [('USB_SERVICE_D+',-(.95-W-G)/2,(26.5,13.6),(56.5,52.1)),('USB_SERVICE_D-',-(.95+W+G)/2,(27.5,14.55),(56.5,51.15))]:
 pts=list(base.offset_curve(d,join_style=2).coords);route(net,[start]+pts+[end])
base=LineString([(30.4,18.8),(29,17.4),(29,10.55)])
for net,d,start,end in [('USB_D+',-(W+G)/2,(29.9,19.2),(28.5,10.05)),('USB_D-',(W+G)/2,(30.9,19.2),(29.5,10.05))]:
 pts=list(base.offset_curve(d,join_style=2).coords);route(net,[start]+pts+[end])
p.SaveBoard(str(f),b)
print('Updated native and recovery USB pair geometry.')
