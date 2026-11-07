#!/usr/bin/env python3
"""Apply documented supplier-drawing corrections to LCSC imports once.

Uses KiCad's bundled pcbnew. Vendor originals are retained in vendor-originals.
"""
from pathlib import Path
import json, shutil
from sexpr import parse,get,children,dump,Atom
R=Path(__file__).resolve().parents[1]
original=R/'vendor-originals';original.mkdir(exist_ok=True)
for f in (R/'lcsc.pretty').glob('*.kicad_mod'):
    if not (original/f.name).exists():shutil.copy2(f,original/f.name)
for src,dst,code in [('USB_A_R6','USB_A_C7279912','C7279912'),('DE9_Female_R6','DE9_Female_C50380980','C50380980')]:
    a=parse((R/'amigahid-pico.pretty'/f'{src}.kicad_mod').read_text());a[1]=dst
    a.append(parse(f'(descr "Reconstructed from LCSC {code} supplier drawing; no EasyEDA model available")'))
    (R/'lcsc.pretty'/f'{dst}.kicad_mod').write_text(dump(a)+'\n')
# TI DBQ is 0.635 mm pitch. The LCSC import incorrectly uses 0.65 mm.
path=R/'lcsc.pretty/SSOP-24_L8.6-W3.9-P0.65-LS6.0-BL.kicad_mod'
a=parse((original/path.name).read_text())
for pad in children(a,'pad'):
    n=int(pad[1]);i=n-1 if n<=12 else 24-n
    get(pad,'at')[1]=Atom(str(round((i-5.5)*0.635,4)))
path.write_text(dump(a)+'\n')
# RM2 DS-6 Fig. 6: 14.5 x 16.5 body, 21 castellations; antenna faces -Y.
path=R/'lcsc.pretty/WIRELM-SMD_RMC20452T.kicad_mod'
a=parse((original/path.name).read_text());get(a,'attr')[1]=Atom('smd')
for pad in children(a,'pad'):
    n=int(pad[1]);at=get(pad,'at');sz=get(pad,'size')
    if n<=7:at[1:]=[Atom('-6.75'),Atom(str(round(-5.05+(n-1)*1.5,3))),Atom('0')];sz[1:]=[Atom('2'),Atom('1')]
    elif n>=15:at[1:]=[Atom('6.75'),Atom(str(round(3.95-(n-15)*1.5,3))),Atom('0')];sz[1:]=[Atom('2'),Atom('1')]
    else:at[1:]=[Atom(str(-4.5+(n-8)*1.5)),Atom('5.35'),Atom('0')];sz[1:]=[Atom('1'),Atom('1.8')]
a.append(parse('(fp_rect (start -7.25 -10.75) (end 7.25 5.75) (layer "F.Fab") (width 0.1) (fill none))'))
a.append(parse('(fp_rect (start -17.75 -24.25) (end 17.75 -5.75) (layer "Dwgs.User") (width 0.1) (fill none))'))
path.write_text(dump(a)+'\n')
# Electrical shield is represented as one schematic pin and three lands.
path=R/'lcsc.pretty/CONN-TH_DIN-603.kicad_mod';a=parse((original/path.name).read_text())
for pad in children(a,'pad'):
    if pad[1] in ['7','8','9']:pad[1]='0'
path.write_text(dump(a)+'\n')
path=R/'lcsc.pretty/USB-C_SMD-TYPE-C-31-M-12_1.kicad_mod'
a=parse((original/path.name).read_text())
for pad in children(a,'pad'):
    if pad[1]=='':pad[2]=Atom('np_thru_hole')
    if pad[1] in ['A1B12','B1A12','A4B9','B4A9']:
        pad[3]=Atom('rect');get(pad,'size')[1:]=[Atom('.6'),Atom('1.3')]
        pad[:]=[x for x in pad if not isinstance(x,list) or x[0]!='primitives']
path.write_text(dump(a)+'\n')
# RP2350 exposed pad: four 1.2 mm paste windows leave a cross for thermal vias.
# Copper land and peripheral lands remain those of the LCSC footprint.
path=R/'lcsc.pretty/QFN-60_L7.0-W7.0-P0.40-TL-EP3.4.kicad_mod'
a=parse((original/path.name).read_text())
for pad in children(a,'pad'):
    if pad[1]=='61':
        layers=get(pad,'layers');layers[:]=[v for v in layers if str(v)!='F.Paste']
for x in [-.85,.85]:
    for y in [-.85,.85]:
        a.append(parse(f'(pad "" smd rect (at {x} {y}) (size 1.2 1.2) (layers "F.Paste"))'))
path.write_text(dump(a)+'\n')
# TS24CA: supplier drawing shows two plastic locating pegs, not plated holes.
path=R/'lcsc.pretty/SW-SMD_2P-TS24CA.kicad_mod'
a=parse((original/path.name).read_text())
for pad in children(a,'pad'):
    if pad[1]=='':pad[2]=Atom('np_thru_hole')
path.write_text(dump(a)+'\n')
# Correct the two front shield legs to the 5.80 mm row pitch on C2341's drawing.
path=R/'lcsc.pretty/USB-A-TH_USB-M-51.kicad_mod'
a=parse((original/path.name).read_text())
for pad in children(a,'pad'):
    if pad[1]=='9' and float(get(pad,'at')[2])>0:get(pad,'at')[2]=Atom('2.88')
    if pad[1] in [str(i) for i in range(1,9)]:
        get(pad,'drill')[1]=Atom('0.92')
path.write_text(dump(a)+'\n')
mechanical=R/'r7.pretty';mechanical.mkdir(exist_ok=True)
for name,drill,diameter in [('MountingHole_3.2mm_M3',3.2,6.4),('MountingHole_2.2mm_M2',2.2,4.4)]:
    text=f'(footprint "{name}" (version 20241229) (generator "pcbnew") (layer "F.Cu") (attr exclude_from_pos_files exclude_from_bom) (pad "" np_thru_hole circle (at 0 0) (size {drill} {drill}) (drill {drill}) (layers "*.Cu" "*.Mask")) (fp_circle (center 0 0) (end {diameter/2} 0) (stroke (width 0.1) (type default)) (fill none) (layer "F.Fab")))'
    (mechanical/(name+'.kicad_mod')).write_text(text+'\n')
test='(footprint "UART_SWD_TestPads" (version 20241229) (generator "pcbnew") (layer "F.Cu") (attr exclude_from_pos_files exclude_from_bom) '
for i in range(6):test+=f'(pad "{i+1}" smd circle (at {i*1.8} 0) (size 1 1) (layers "F.Cu" "F.Mask")) '
test+=')';(mechanical/'UART_SWD_TestPads.kicad_mod').write_text(test+'\n')
data=json.loads((R/'circuit.json').read_text())
for c in data['parts']:
    if c['ref'].startswith('H'):c['footprint']='r7:'+c['footprint'].split(':')[-1]
(R/'circuit.json').write_text(json.dumps(data,indent=2)+'\n')
import math
for folder in [R/'lcsc.pretty',mechanical]:
    for file in folder.glob('*.kicad_mod'):
        a=parse(file.read_text());a[1]=file.stem
        for item in list(a[2:]):
            if not isinstance(item,list):continue
            if item[0]=='property' and item[1]=='LCSC Part' or get(item,'layer')==[Atom('layer'),'F.CrtYd']:a.remove(item)
        # Supplier silk can exceed the body; use a compact IPC density-C
        # courtyard for 0402 and the regulator inductor. Copper unchanged.
        compact=file.stem.startswith('QFN-60') or file.stem in ['C0402','R0402','IND-SMD_L2.0-W1.6_AOTA-B201610S3R3-101-T']
        points=[]
        for pad in children(a,'pad'):
            at=get(pad,'at');x,y=map(float,at[1:3]);w,h=map(float,get(pad,'size')[1:3]);angle=float(at[3]) if len(at)>3 else 0
            if abs(angle%180-90)<.01:w,h=h,w
            points.extend([(x-w/2,y-h/2),(x+w/2,y+h/2)])
        for item in a[2:]:
            if compact:continue
            if not isinstance(item,list) or not str(item[0]).startswith('fp_') or item[0]=='fp_text':continue
            layer=get(item,'layer')
            if layer is None or layer[1] not in ['F.Fab','F.SilkS']:continue
            for tag in ['start','end','center']:
                pt=get(item,tag)
                if pt:points.append(tuple(map(float,pt[1:3])))
            if item[0]=='fp_circle':
                c=get(item,'center');e=get(item,'end');x,y=map(float,c[1:3]);r=math.hypot(float(e[1])-x,float(e[2])-y);points.extend([(x-r,y-r),(x+r,y+r)])
        if not points:continue
        margin=.1 if compact else .25
        x1=min(v[0] for v in points)-margin;y1=min(v[1] for v in points)-margin;x2=max(v[0] for v in points)+margin;y2=max(v[1] for v in points)+margin
        for v,w in [((x1,y1),(x2,y1)),((x2,y1),(x2,y2)),((x2,y2),(x1,y2)),((x1,y2),(x1,y1))]:
            a.append(parse(f'(fp_line (start {v[0]} {v[1]}) (end {w[0]} {w[1]}) (stroke (width 0.05) (type default)) (layer "F.CrtYd"))'))
        # Put imported detailed outlines on fabrication layer; board legends
        # are placed separately to keep solder lands clear.
        for item in a[2:]:
            if isinstance(item,list) and str(item[0]).startswith('fp_'):
                layer=get(item,'layer')
                if layer and layer[1]=='F.SilkS':layer[1]='F.Fab'
        file.write_text(dump(a)+'\n')
print('Prepared local LCSC libraries and non-BOM mechanical/test-pad library.')

# Restore reviewed models after regenerating corrected footprint geometry.
if (R/"models/manifest.json").exists():
    import subprocess,sys
    subprocess.run([sys.executable,str(R/"scripts/attach_models.py")],check=True)
if (R/'assembly-markers.json').exists():
    import subprocess, sys
    subprocess.run([sys.executable,str(R/'scripts/apply_orientation_marks.py')],check=True)
