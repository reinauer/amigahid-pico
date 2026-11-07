#!/usr/bin/env python3
"""Attach reviewed project-relative models without changing copper or pads.

Supplier STEP files retain their raw origins. The manifest records placement
corrections independently of the archived supplier files.
"""
import json,hashlib,copy
from pathlib import Path
from sexpr import parse,get,children,dump,Atom
R=Path(__file__).resolve().parents[1]
parts=json.loads((R/'circuit.json').read_text())['parts']
models=json.loads((R/'models/manifest.json').read_text())
def record(m):
    def xyz(k,default):return ' '.join(map(str,m.get(k,default)))
    return parse('(model "${KIPRJMOD}/'+m['file']+'" (offset (xyz '+xyz('offset',[0,0,0])+')) (scale (xyz 1 1 1)) (rotate (xyz '+xyz('rotate',[0,0,0])+')))')
# Shared package libraries use one representative model. Each placed instance
# below gets the model for its own LCSC part number.
seen=set()
for c in parts:
    code=c.get('lcsc');fp=c['footprint']
    if code not in models or fp in seen:continue
    seen.add(fp);lib,name=fp.split(':');f=R/(lib+'.pretty')/(name+'.kicad_mod')
    a=parse(f.read_text());a[:]=[x for x in a if not(isinstance(x,list) and x[0]=='model')]
    a.append(record(models[code]));f.write_text(dump(a)+'\n')
f=R/'amigahid-pico.kicad_pcb';a=parse(f.read_text());byref={c['ref']:c for c in parts};coverage=[]
for fp in children(a,'footprint'):
    ref=next(x[2] for x in children(fp,'property') if x[1]=='Reference');c=byref[ref];code=c.get('lcsc')
    fp[:]=[x for x in fp if not(isinstance(x,list) and x[0]=='model')]
    if code:
        m=models[code];assert (R/m['file']).is_file(),m['file'];fp.append(record(m));coverage.append({'ref':ref,'lcsc':code,'model':m['file'],'source':m['source'],'kind':m['kind']})
    # The OLED is a separate mechanical assembly; show only its connector.
f.write_text(dump(a)+'\n')
(R/'review/model-coverage.json').write_text(json.dumps(coverage,indent=2)+'\n')
print('Attached models to',len(coverage),'purchased parts; OLED assembly hidden.')
