#!/usr/bin/env python3
"""Connect overlapping ground-fill islands with tented stitching vias.

Run after zone fill. Always inspect the resulting DRC and return paths.
"""
from pathlib import Path
import pcbnew as p
from shapely.geometry import Polygon,Point
from shapely.ops import unary_union
R=Path(__file__).resolve().parents[1];b=p.LoadBoard(str(R/'amigahid-pico.kicad_pcb'))
nodes=[]
def contour(c):return [(c.CPoint(i).x/1e6,c.CPoint(i).y/1e6) for i in range(c.PointCount())]
for z in b.Zones():
 if z.GetIsRuleArea() or z.GetNetname()!='GND':continue
 layer=z.GetLayer();ps=z.GetFilledPolysList(layer)
 for i in range(ps.OutlineCount()):
  poly=Polygon(contour(ps.COutline(i)),[contour(ps.CHole(i,j)) for j in range(ps.HoleCount(i))]).buffer(0)
  if poly.area:nodes.append((layer,poly))
parent=list(range(len(nodes)))
def root(i):
 while parent[i]!=i:i=parent[i]
 return i
def join(a,z):parent[root(z)]=root(a)
obstacles=[];existing=[]
for t in b.GetTracks():
 if isinstance(t,p.PCB_VIA):
  pt=Point(t.GetPosition().x/1e6,t.GetPosition().y/1e6)
  obstacles.append(pt.buffer(t.GetDrillValue()/2e6+.31))
  if t.GetNetname()=='GND':existing.append(pt.buffer(t.GetWidth(p.F_Cu)/2e6))
for f in b.GetFootprints():
 for pad in f.Pads():
  ds=pad.GetDrillSize()
  if ds.x:
   pt=Point(pad.GetPosition().x/1e6,pad.GetPosition().y/1e6)
   obstacles.append(pt.buffer(max(ds.x,ds.y)/2e6+.31))
   if pad.GetNetname()=='GND':existing.append(pt.buffer(min(pad.GetSize().x,pad.GetSize().y)/2e6))
for pt in existing:
 touched=[i for i,(l,poly) in enumerate(nodes) if poly.intersects(pt)]
 for i in touched[1:]:join(touched[0],i)
blocked=unary_union(obstacles);edges=[]
# Every new via crosses every copper layer. A clear overlap of two ground
# fills alone does not establish clearance to tracks on the other layers.
for item in list(b.GetTracks())+[pad for fp in b.GetFootprints() for pad in fp.Pads()]:
 if item.GetNetname()=='GND':continue
 for layer in [p.F_Cu,p.In1_Cu,p.In2_Cu,p.B_Cu]:
  if not item.IsOnLayer(layer):continue
  ps=p.SHAPE_POLY_SET();item.TransformShapeToPolygon(ps,layer,0,1000,p.ERROR_OUTSIDE)
  for i in range(ps.OutlineCount()):
   obstacles.append(Polygon(contour(ps.COutline(i))).buffer(.225+.127+.01))
blocked=unary_union(obstacles)
for i,(la,pa) in enumerate(nodes):
 for j,(lb,pb) in enumerate(nodes[:i]):
  if la==lb:continue
  free=pa.intersection(pb).buffer(-.28).difference(blocked)
  if not free.is_empty and free.area>.005:edges.append((free.area,i,j,free))
added=[];net=b.FindNet('GND')
for _,i,j,free in sorted(edges,reverse=True,key=lambda e:e[0]):
 if root(i)==root(j):continue
 pt=free.representative_point();x,y=pt.x,pt.y
 t=p.PCB_VIA(b);t.SetPosition(p.VECTOR2I(p.FromMM(x),p.FromMM(y)));t.SetWidth(p.FromMM(.45));t.SetDrill(p.FromMM(.2));t.SetViaType(p.VIATYPE_THROUGH);t.SetLayerPair(p.F_Cu,p.B_Cu);t.SetNet(net)
 t.SetFrontTentingMode(p.TENTING_MODE_TENTED);t.SetBackTentingMode(p.TENTING_MODE_TENTED);b.Add(t);join(i,j);added.append((x,y))
p.ZONE_FILLER(b).Fill(b.Zones());p.SaveBoard(str(R/'amigahid-pico.kicad_pcb'),b)
print('Added ground vias:',len(added),'polygon graph groups remaining:',len({root(i) for i in range(len(nodes))}))
