#!/usr/bin/env python3
"""Check ASCII STL topology, volume and solid count without third-party packages."""
from pathlib import Path
from collections import Counter
import json,re,math
ROOT=Path(__file__).resolve().parents[1]
def inspect(path):
 points=[tuple(map(float,m)) for m in re.findall(r'vertex\s+(\S+)\s+(\S+)\s+(\S+)',path.read_text())]
 triangles=[points[i:i+3] for i in range(0,len(points),3)]
 assert triangles, f'Empty mesh: {path}'
 edges=Counter();adj={};volume=0
 for a,b,c in triangles:
  volume+=(a[0]*(b[1]*c[2]-b[2]*c[1])+a[1]*(b[2]*c[0]-b[0]*c[2])+a[2]*(b[0]*c[1]-b[1]*c[0]))/6
  for u,v in [(a,b),(b,c),(c,a)]:
   edges[tuple(sorted([u,v]))]+=1;adj.setdefault(u,set()).add(v);adj.setdefault(v,set()).add(u)
 assert all(n==2 for n in edges.values()),f'Open/nonmanifold edges in {path}'
 seen=set();count=0
 for v in adj:
  if v in seen:continue
  count+=1;stack=[v];seen.add(v)
  while stack:
   for n in adj[stack.pop()]:
    if n not in seen:seen.add(n);stack.append(n)
 assert count==1,f'{path}: expected one connected solid, got {count}'
 assert volume>0,f'{path}: inverted or zero-volume solid'
 bounds=[[min(v[k] for v in points) for k in range(3)],[max(v[k] for v in points) for k in range(3)]]
 assert abs(bounds[0][2])<1e-5,'Print base must sit on Z=0'
 return {'triangles':len(triangles),'closed_two_manifold':True,'connected_solids':count,'volume_mm3':round(volume,3),'bounds_mm':bounds}
if __name__=='__main__':
 report={name:inspect(ROOT/'build'/f'{name}.stl') for name in ['base','lid']}
 (ROOT/'build/mesh-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
