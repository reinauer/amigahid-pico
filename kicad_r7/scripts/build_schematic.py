#!/usr/bin/env python3
"""Build the r7 schematic from circuit.json and local libraries.

Does not modify the routed board. Re-run PCB synchronization after changing
the circuit, then repeat validation. Every assembled part carries LCSC Part #.
"""
from pathlib import Path
import json, copy, uuid, csv
from sexpr import parse, get, children, dump, Atom

R=Path(__file__).resolve().parents[1]
data=json.loads((R/'circuit.json').read_text());parts=data['parts']
orientations=json.loads((R/'assembly-orientation.json').read_text())['parts']
symbols={s[1]:s for s in children(parse((R/'r7.kicad_sym').read_text()),'symbol')}
def uid(s):return str(uuid.uuid5(uuid.NAMESPACE_URL,'amigahid-r7/'+s))
def q(s):return json.dumps(str(s),ensure_ascii=False)
rootid=uid('interfaces')
sheetids={s:uid('sheet/'+s) for s in ['controller','power','usb','control']}
names={'interfaces':'amigahid-pico','controller':'controller','power':'power','usb':'usb','control':'control'}
for sheet in names:
    sid=uid(sheet);own=[c for c in parts if c['sheet']==sheet]
    paper='A2' if sheet=='interfaces' else 'A3'
    out=[f'(kicad_sch (version 20250114) (generator "eeschema") (uuid {q(sid)}) (paper "{paper}") '
         f'(title_block (title "AmigaHID r7 - {sheet}") (date "2026-10-14") (rev "7 prototype")))']
    # Keep the outer expression open while appending its children.
    out[0]=out[0][:-1]
    used=[]
    for name in sorted({c['symbol'] for c in own}):
        s=copy.deepcopy(symbols[name]);s[1]='r7:'+name;used.append(dump(s))
    out.append('(lib_symbols '+'\n'.join(used)+')')
    for c in own:
        x,y=c['sch'];s=symbols[c['symbol']];ref=c['ref'];cid=uid(ref)
        # Keep field labels above the actual symbol graphics, including tall ICs.
        top=max([float(get(p,'at')[2]) for sub in children(s,'symbol') for p in children(sub,'pin')]+[0])
        yy=y-top-7.62
        props=[]
        fields={'Reference':ref,'Value':c['value'],'Footprint':c['footprint'],'Datasheet':c['datasheet'],
                'LCSC Part #':c['lcsc'],'MPN':c['mpn'],'Manufacturer':c['manufacturer'],
                'Source':f'https://www.lcsc.com/product-detail/{c["lcsc"]}.html' if c['lcsc'] else '',
                'Sourcing note':c.get('source_note',''), 'Function':c.get('function','')}
        if c.get('lcsc'):
            fields['FT Rotation Offset']=str(orientations[c['lcsc']]['offset_deg'])
        for i,(name,val) in enumerate(fields.items()):
            hide=name not in ['Reference','Value']
            py=y if hide else yy+2.54*i
            px=x
            if not hide and c['symbol'] in ['R','C','C_Polarized','L']:
                px=x+5.08;py=y-1.27+2.54*i
            props.append(f'(property {q(name)} {q(val)} (at {px} {py} 0) (effects (font (size 1 1))'+(' (hide yes)' if hide else '')+'))')
        path='/'+rootid+('' if sheet=='interfaces' else '/'+sheetids[sheet])
        bom='yes' if c['lcsc'] else 'no'
        onboard='no' if c['ref']=='JP2' else 'yes'
        out.append(f'(symbol (lib_id {q("r7:"+c["symbol"])}) (at {x} {y} 0) (unit 1) '
                   f'(in_bom {bom}) (on_board {onboard}) (dnp no) (uuid {q(cid)}) '+''.join(props)+
                   f'(instances (project "amigahid-pico" (path {q(path)} (reference {q(ref)}) (unit 1)))))')
        seen=set()
        for sub in children(s,'symbol'):
            for pin in children(sub,'pin'):
                num=str(get(pin,'number')[1]);dx,dy,angle=map(float,get(pin,'at')[1:4])
                px,py=round(x+dx,4),round(y-dy,4);net=c['nets'].get(num)
                if (px,py,net) in seen:continue
                seen.add((px,py,net))
                if not net:
                    out.append(f'(no_connect (at {px} {py}) (uuid {q(uid(ref+"nc"+num))}))');continue
                vx,vy={0:(-5.08,0),180:(5.08,0),90:(0,5.08),270:(0,-5.08)}[int(angle)]
                ex,ey=round(px+vx,4),round(py+vy,4)
                out.append(f'(wire (pts (xy {px} {py}) (xy {ex} {ey})) (stroke (width 0) (type default)) (uuid {q(uid(ref+"wire"+num))}))')
                # Global labels link sheet-local functional blocks without hidden nets.
                la=0 if vx<0 else 270 if vy>0 else 90 if vy<0 else 180
                justify='right' if vx<0 or vy>0 else 'left'
                out.append(f'(global_label {q(net)} (shape input) (at {ex} {ey} {la}) '
                           f'(effects (font (size 0.9 0.9)) (justify {justify})) (uuid {q(uid(ref+"label"+num))}))')
    notes=[]
    if sheet=='interfaces':
        for child,y in [('controller',58.42),('power',116.84),('usb',152.4),('control',193.04)]:
            page={'controller':2,'power':3,'usb':4,'control':5}[child]
            out.append(f'(sheet (at 35.56 {y}) (size 76.2 30.48) (stroke (width 0.254) (type default)) (fill (color 0 0 0 0)) '
                       f'(uuid {q(sheetids[child])}) (property "Sheetname" {q(child.title())} (at 35.56 {y-1.27} 0) (effects (font (size 1.27 1.27)) (justify left bottom))) '
                       f'(property "Sheetfile" "{child}.kicad_sch" (at 35.56 {y+31.75} 0) (effects (font (size 1.27 1.27)) (justify left top))) '
                       f'(instances (project "amigahid-pico" (path {q("/"+rootid)} (page "{page}")))))')
        notes=[('r7 PROTOTYPE - RP2350A + RM2; all assembled components on top',20,20),
               ('3V3 bus switches with 5V-tolerant I/O; GPIO must drive LOW or release.',133,40),
               ('Pull-ups: eight isolated 4 x 10k arrays; 16 channels retained.',20,240),
               ('H1-H4: shared OLED / case mounts, 30.50 x 28.60 mm; M3 nylon spacers.',20,318),
               ('Mini-DIN: A4000 keyboard protocol; DE9 pin 7 is NC on both ports.',20,326),
               ('Power: mini-DIN pin 4 remains usable; USB-C automatically takes priority.',20,334),
               ('Recovery: USB-C powers the controller, selects USB service and isolates Amiga signals.',20,342),
               ('J2 is a group of bare top-side test pads, not a fitted pin header.',20,350)]
    elif sheet=='controller':
        notes=[('RP2350A controller, 4MB QSPI flash, 12MHz crystal, RM2 wireless',20,20),
               ('U13 monitors 3V3: 3.07V threshold, CT open = 20ms typical RUN release delay.',20,235),
               ('RECOVERY_N drives MR and latch preset; automatic RUN reset does not select BOOTSEL.',20,243),
               ('RM2: 470R and 10k combine DOUT / IRQ with GPIO24 per datasheet Table 2.',225,230),
               ('Hold RECOVERY while connecting USB-C; release after 0.1s. No working firmware required.',20,248),
               ('USB-C already connected: press/release RECOVERY. Amiga power is optional.',20,256)]
    elif sheet=='power':
        notes=[('Power: automatic Amiga / USB-C mux -> 3V3 buck -> internal 1V1 regulator',20,20),
               ('L1 marked end (pad 1) goes to 1V1. Copy Raspberry Pi regulator placement.',165,104),
               ('100n decoupling: IOVDD, DVDD, ADC_AVDD, USB/QSPI, flash and RM2.',20,124),
               ('Keep C6/C7 and PGND loop compact. R3/C8 filter VREG_AVDD.',165,112),
               ('USB 500mA PTC is overload protection; total draw is limited by the Amiga port.',20,204),
               ('A4000 keyboard port: documented 100mA budget. Measure complete r7/peripheral demand.',20,212)]
    elif sheet=='usb':
        notes=[('USB-C recovery with automatic data selection; CH334P drives two stacked USB-A ports',20,20),
               ('Control sheet: hardware recovery latch selects service data and disables USB-A power.',20,225),
               ('Normal USB-C power requires an adequately rated 5V supply; PC default current is limited.',20,235),
               ('GPIO28 reads USB_HOST_SELECT. U7 S=0 service, S=1 hub.',20,245)]
    else:
        notes=[('Automatic source selection and firmware-independent recovery',20,20),
               ('USB-C priority: 4.0V nominal threshold. Keyboard pin 4 remains the fallback input.',20,250),
               ('Hold RECOVERY while plugging USB-C, release after 0.1s; or press with USB-C present.',20,258),
               ('U9 /PRE=/CLR=0: both outputs high; U13 keeps MCU reset and D2 holds host power OFF.',20,266),
               ('USB absent clears latch. Q1 enables Amiga signals only if keyboard power is present.',20,274)]
    for i,(txt,x,y) in enumerate(notes):
        out.append(f'(text {q(txt)} (at {x} {y} 0) (effects (font (size 1.1 1.1)) (justify left bottom)) (uuid {q(uid(sheet+"note"+str(i)))}))')
    if sheet=='interfaces':out.append(f'(sheet_instances (path "/" (page "1")))')
    out.append(')');(R/(names[sheet]+'.kicad_sch')).write_text('\n'.join(out)+'\n')
(R/'sym-lib-table').write_text('(sym_lib_table (version 7) (lib (name "r7") (type "KiCad") (uri "${KIPRJMOD}/r7.kicad_sym") (options "") (descr "R7 verified pin tables")) (lib (name "lcsc") (type "KiCad") (uri "${KIPRJMOD}/lcsc.kicad_sym") (options "") (descr "LCSC / EasyEDA imports")))\n')
(R/'fp-lib-table').write_text('(fp_lib_table (version 7) (lib (name "lcsc") (type "KiCad") (uri "${KIPRJMOD}/lcsc.pretty") (options "") (descr "LCSC footprints and documented supplier-drawing corrections")) (lib (name "r7") (type "KiCad") (uri "${KIPRJMOD}/r7.pretty") (options "") (descr "Non-BOM mounting holes and test pads")))\n')
fields=['Reference','Value','LCSC Part #','MPN','Manufacturer','Footprint','Datasheet','Source','Notes','Function']
rows=[dict(zip(fields,[c['ref'],c['value'],c['lcsc'],c['mpn'],c['manufacturer'],c['footprint'],c['datasheet'],f'https://www.lcsc.com/product-detail/{c["lcsc"]}.html',c.get('source_note',''),c.get('function','')])) for c in parts if c['lcsc']]
# Keep ordinary KiCad/Fabrication Toolkit grouping consistent with LCSC codes.
groups={}
for row in rows:
    signature=tuple(row[k] for k in ['Value','Footprint','MPN'])
    code=row['LCSC Part #']
    assert groups.setdefault(code,signature)==signature, f'Inconsistent BOM identity: {code}'
for name in ['BOM.csv','SOURCING.csv']:
    with (R/name).open('w') as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
print(f'Built {len(names)} schematic sheets; {len(rows)} assembled parts with LCSC numbers.')
