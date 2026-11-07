#!/usr/bin/env python3
"""Create r7 placement from the KiCad-exported schematic netlist.

This deliberately overwrites PCB routing. Run only for a fresh placement.
The normal editable deliverable is amigahid-pico.kicad_pcb.
"""
from pathlib import Path
import json, uuid, subprocess, xml.etree.ElementTree as ET
import pcbnew as p
R=Path(__file__).resolve().parents[1]
CLI='/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'
subprocess.run([CLI,'sch','export','netlist','--format','kicadxml','-o',str(R/'review/netlist.xml'),str(R/'amigahid-pico.kicad_sch')],check=True,stderr=subprocess.DEVNULL)
xml=ET.parse(R/'review/netlist.xml').getroot()
data=json.loads((R/'circuit.json').read_text());parts={c['ref']:c for c in data['parts']}
b=p.BOARD();b.SetCopperLayerCount(4);b.GetDesignSettings().SetBoardThickness(p.FromMM(1))
b.GetTitleBlock().SetTitle('AmigaHID r7 - RP2350A + RM2 - PROTOTYPE');b.GetTitleBlock().SetRevision('7')
def v(x,y):return p.VECTOR2I(p.FromMM(x),p.FromMM(y))
netmap={};pinmap={}
for n in xml.find('nets'):
    name=n.get('name');net=p.NETINFO_ITEM(b,name);b.Add(net);netmap[name]=net
    for node in n:pinmap[(node.get('ref'),node.get('pin'))]=name
positions={
 'U1':(20,29,0),'U2':(38,18.5,-90),'U3':(38,41.5,-90),
 'U4':(14.5,49.25,180),'U5':(13.05,16.275,0),'U6':(30.5,41.5,180),
 'J10':(5.5,16,-90),'U7':(21,16.5,0),'Q1':(27,15,0),
 'R14':(9.5,10.5,0),'R15':(9.5,19.5,0),'R16':(27,18,0),'R17':(27,11,0),'R18':(24,13,0),
 'C30':(18,16,90),'C31':(7.5,22,0),'J2':(2.5,34.5,0),'J3':(50.84,27.74,0),'J4':(58,45,90),
 'J5':(12,4.4,180),'J6':(46,4.4,180),'J7':(62,16.35,90),
 'H1':(35.75,28.75,0),'H2':(66.25,28.75,0),'H3':(35.75,57.35,0),'H4':(66.25,57.35,0),
 'RN1':(45,15,0),'RN2':(45,21,0),'RN3':(45,35,0),'RN4':(45,45,0),
 'RN6':(50,15,0),'RN7':(50,21,0),'RN8':(50,35,0),'RN9':(50,45,0),
 'C1':(38,12,0),'C2':(38,48,0),'C3':(55,32,0),'C4':(58,55,0),'C5':(40,52,0),
 'R1':(44,52,0),'R2':(48,52,0),'F1':(51,39,0),
 'L1':(22,21.8,0),'R3':(24.2,22.1,-90),'C6':(22,24.39,0),'C7':(22,23.45,0),'C8':(24.2,23.95,-90),
 'Y1':(20,39.9,0),'R4':(19.6,35.9,-90),'C28':(18.6,37.5,180),'C29':(21.58,37.5,0),
 'R5':(12.5,22,0),'R6':(9,13,90),'R7':(19.9,21.6,90),'R8':(18.9,21.6,90),
 'R9':(24,47.1,0),'R10':(15,41.5,0),'R11':(3,29.5,90),'R12':(25,35.5,0),'R13':(12.5,41.3,0),
 'D3':(3,26,90),'SW1':(7,23,0),'SW2':(7,30,0),
 'L2':(29,47,-90),'C9':(31,37.5,0),'C10':(26.5,51.5,90),'C11':(30,51.5,90),'C12':(33,42.3,90),
 'C13':(13.515,25.6,180),'C14':(13.52,28,180),'C15':(13.515,30.4,180),
 'C16':(18.2,35.08,-90),'C17':(20.8,35.88,-90),'C18':(25.685,33,0),
 'C19':(27.085,29.4,0),'C20':(27.08,28.4,0),'C21':(26.485,26,0),
 'C22':(26.5,24.4,0),'C23':(17.9,21.6,90),'C24':(17.5,13.5,180),
 'C25':(4.5,47,90),'C26':(3,43.5,90),'C27':(10,41.5,0),
}
# Preserve the Raspberry Pi regulator/decoupling placement as one group.
core_shift=(11,-1)
core=['U1','L1','R3','C6','C7','C8','Y1','R4','C28','C29','R7','R8',
      'C13','C14','C15','C16','C17','C18','C19','C20','C21','C22','C23']
for ref in core:
    x,y,a=positions[ref];positions[ref]=(x+core_shift[0],y+core_shift[1],a)
positions.pop('SW2',None)
positions.update({
 'U2':(43,25.5,-90),'U3':(43,40.5,-90),'U4':(10.25,44,90),
 'U5':(20,25,0),'U6':(27,51.5,180),'U7':(30,12,0),'U8':(43,31.5,0),
 'J10':(60,56,0),'SW1':(49,57.7,180),'J3':(34.84,16.29,0),
 'J4':(57.8,41.5,90),'J2':(6,20,90),
 'H1':(19.75,17.3,0),'H2':(50.25,17.3,0),
 'H3':(19.75,45.9,0),'H4':(50.25,45.9,0),
 'RN1':(49,24,0),'RN2':(49,30,0),'RN3':(49,36,0),'RN4':(49,41,0),
 'RN6':(53,26.5,0),'RN7':(53,30,0),'RN8':(53,35,0),'RN9':(45,48,0),
 'C1':(43,20.5,0),'C2':(43,45.5,0),'C3':(44,18,0),'C4':(65,29,0),'C5':(40,49,0),
 'R1':(39,18,0),'R2':(42,18,0),'F1':(60,29,0),
 'R5':(24,20,0),'R6':(20,21,90),'R9':(18,36,0),'R10':(17,32,0),
 'R11':(9,26,90),'R12':(47,54,0),'R13':(18,39,0),'D3':(6,26,90),
 'L2':(25.5,57,-90),'C9':(27,47.5,0),'C10':(31,56.5,90),'C11':(34,56.5,90),'C12':(29.5,52.3,90),
 'C24':(20,28.5,0),'C25':(7,33,90),'C26':(10,32,90),'C27':(14,34,0),
 'Q1':(53,54,0),'Q2':(18,30,0),'R14':(55,51.5,0),'R15':(65,51.5,0),
 'R16':(30,9.5,0),'R17':(49,51,0),'R18':(53,51,0),'C30':(27.5,12,90),'C31':(65.5,54,0),
 'D1':(38,53,0),'D2':(43,53,0),'Y2':(47.5,32.5,0),'C32':(40,31.5,90),'C33':(42.5,35,0),
})
positions.update({
 'U3':(42.5,41.5,-90),'U4':(9.5,44,90),'U5':(18,25,0),
 'U8':(40.2,31.3,0),'Y2':(44.8,34.8,0),'C32':(36.7,30.1,90),'C33':(38.8,35,0),
 'J4':(58.6,41.5,90),'J2':(8,20,90),
 'RN1':(49.5,24,90),'RN2':(49.5,29,90),'RN3':(49.5,34,90),'RN4':(49.5,39.5,90),
 'RN6':(38.5,11.5,0),'RN7':(53.8,26.2,90),'RN8':(53.8,30.7,90),'RN9':(44,48.5,90),
 'C2':(40,47.2,0),'C3':(25.5,16,0),'C5':(40,49.5,0),'F1':(58,29,90),
 'R1':(42,15.5,90),'R2':(45,15.5,90),
 'R6':(14,22,90),'R5':(14,24,90),'C24':(14,26,0),'Q2':(16,31.4,0),'R10':(20.5,32.5,0),
 'D2':(43.5,53,0),'C31':(66.8,52,0),
})
# Route-oriented placement: each bus bank faces its connector group.
positions.update({
 'U2':(12,16,0),'C1':(7,14,90),'U3':(43,25.5,-90),'C2':(43,20.5,0),
 'J2':(4.5,22,90),'U8':(43.5,36,-90),'Y2':(38,40,0),
 'C32':(42,33.1,0),'C33':(38,44,0),
 'RN1':(7,10,0),'RN2':(12,10,0),'RN3':(49.5,24,90),'RN4':(49.5,29,90),
 'RN6':(9,23,0),'RN7':(22.5,36,90),'RN8':(43,30.8,0),'RN9':(39,19.8,0),
 'R1':(24.5,18.5,90),'R2':(24.5,22,90),'C3':(24.6,12.5,0),'D3':(6,27,90),
 'C4':(65,29,0),'F1':(58,29,90),'C5':(53,51,0),'R18':(46,50.5,0),'R14':(57,51,0),'R16':(32,10,0),
})
fps={}
for comp in xml.find('components'):
    ref=comp.get('ref');c=parts[ref]
    if not c['footprint']:continue
    lib,name=c['footprint'].split(':');fp=p.FootprintLoad(str(R/(lib+'.pretty')),name)
    if not fp:raise RuntimeError(c['footprint'])
    fp.SetFPID(p.LIB_ID(lib,name));fp.SetReference(ref);fp.SetValue(c['value'])
    fp.SetPath(p.KIID_PATH(comp.find('sheetpath').get('tstamps')+comp.findtext('tstamps')))
    x,y,a=positions[ref];fp.SetPosition(v(x,y));fp.SetOrientationDegrees(a)
    b.Add(fp);fps[ref]=fp
    for pad in fp.Pads():
        netname=pinmap.get((ref,pad.GetNumber()))
        if netname:pad.SetNet(netmap[netname])
    for name,value in [('LCSC Part #',c['lcsc']),('MPN',c['mpn']),('Manufacturer',c['manufacturer']),('Source',f'https://www.lcsc.com/product-detail/{c["lcsc"]}.html' if c['lcsc'] else ''),('Datasheet',c['datasheet']),('Sourcing note',c.get('source_note',''))]:
        field=fp.GetField(name)
        if field is None:
            field=p.PCB_FIELD(fp,p.FIELD_T_USER,name);fp.Add(field)
        field.SetText(value);field.SetVisible(False)
    fp.Value().SetVisible(False);fp.Reference().SetVisible(False)
    # Keep detailed component outlines and labels on fabrication layer.
    # Separate board legends are added after final placement.
    for g in fp.GraphicalItems():
        if g.GetLayer()==p.F_SilkS:g.SetLayer(p.F_Fab)
    if ref.startswith('H') or ref=='J2':fp.SetAttributes(fp.GetAttributes()|p.FP_EXCLUDE_FROM_BOM|p.FP_EXCLUDE_FROM_POS_FILES)
outline=[(0,-6),(70,-6),(70,60),(0,60),(0,52),(3.6,52),(3.6,36),(0,36),(0,-6)]
for a,z in zip(outline,outline[1:]):
    g=p.PCB_SHAPE();g.SetShape(p.SHAPE_T_SEGMENT);g.SetStart(v(*a));g.SetEnd(v(*z));g.SetLayer(p.Edge_Cuts);g.SetWidth(p.FromMM(.05));b.Add(g)
def area(points,layers,rule=False,net=None):
    z=p.ZONE(b);ls=p.LSET()
    for layer in layers:ls.AddLayer(layer)
    z.SetLayerSet(ls);z.SetIsRuleArea(rule)
    if rule:
        z.SetDoNotAllowZoneFills(True);z.SetDoNotAllowTracks(True);z.SetDoNotAllowVias(True);z.SetDoNotAllowPads(True);z.SetDoNotAllowFootprints(False)
    elif net:
        z.SetNet(netmap[net]);z.SetLocalClearance(p.FromMM(.15));z.SetPadConnection(p.ZONE_CONNECTION_FULL);z.SetMinThickness(p.FromMM(.15))
    poly=z.Outline();poly.NewOutline()
    for x,y in points:poly.Append(p.FromMM(x),p.FromMM(y))
    b.Add(z);return z
area([(-14.75,26.25),(3.75,26.25),(3.75,61.75),(-14.75,61.75)],[p.F_Cu,p.In1_Cu,p.In2_Cu,p.B_Cu],True)
def text(label,x,y,size=1,layer=p.F_SilkS):
    t=p.PCB_TEXT(b);t.SetText(label);t.SetPosition(v(x,y));t.SetTextSize(v(size,size));t.SetTextThickness(p.FromMM(.15));t.SetLayer(layer);b.Add(t)
text('AmigaHID r7',38,59,1)
text('P1',17,11.5,.8);text('P2',55,10.8,.8);text('KBD',61,7,.8)
text('RECOVERY',49,55,.7)
text('USB-C',60,51,.7)
# Retain the OLED mechanical envelope as an assembly drawing, above the PCB.
for a,z in zip([(17.25,14.75),(52.75,14.75),(52.75,48.45),(17.25,48.45),(17.25,14.75)],[(52.75,14.75),(52.75,48.45),(17.25,48.45),(17.25,14.75)]):
    g=p.PCB_SHAPE();g.SetShape(p.SHAPE_T_SEGMENT);g.SetStart(v(*a));g.SetEnd(v(*z));g.SetLayer(p.Dwgs_User);g.SetWidth(p.FromMM(.1));b.Add(g)
text('CENTERED OLED 35.5 x 33.7',35,23,.8,p.Dwgs_User)
# Source-verified regulator/clock routing from Raspberry Pi R4-S1 reference.
reference=R/'reference/RP2350A-minimal.kicad_pcb'
if reference.exists():
    source=p.LoadBoard(str(reference))
    mapping={'+3V3':'+3V3','+1V1':'+1V1','GND':'GND','/VREG_LX':'VREG_LX','/VREG_AVDD':'VREG_AVDD','/XIN':'XIN','/XOUT':'XOUT',
             'Net-(X1-3)':'XTAL_OUT','Net-(U1-USB_DM)':'USB_DM_MCU','Net-(U1-USB_DP)':'USB_DP_MCU'}
    for t in source.GetTracks():
        target=mapping.get(t.GetNetname())
        if not target or target in ['XIN','XOUT','XTAL_OUT']:continue
        if t.GetNetname()=='GND' and t.GetStart().x/1e6>107 and t.GetStart().y/1e6>109:continue
        ends=[t.GetStart(),t.GetEnd()]
        if not all(92<=pt.x/1e6<=108 and 90<=pt.y/1e6<=114 for pt in ends):continue
        if isinstance(t,p.PCB_VIA):
            item=p.PCB_VIA(b);item.SetPosition(v(t.GetPosition().x/1e6-69,t.GetPosition().y/1e6-72-(1.5 if t.GetPosition().y/1e6>110 else 0)));item.SetWidth(t.GetWidth(p.F_Cu));item.SetDrill(t.GetDrillValue());item.SetViaType(p.VIATYPE_THROUGH);item.SetLayerPair(p.F_Cu,p.B_Cu)
        else:
            item=p.PCB_TRACK(b);item.SetStart(v(t.GetStart().x/1e6-69,t.GetStart().y/1e6-72-(1.5 if t.GetStart().y/1e6>110 else 0)));item.SetEnd(v(t.GetEnd().x/1e6-69,t.GetEnd().y/1e6-72-(1.5 if t.GetEnd().y/1e6>110 else 0)));item.SetWidth(t.GetWidth());item.SetLayer(t.GetLayer())
        item.SetNet(netmap[target]);item.SetLocked(True);b.Add(item)
def route(net,points,width=.15,layer=p.F_Cu):
    for a,z in zip(points,points[1:]):
        t=p.PCB_TRACK(b);t.SetStart(v(a[0]+11,a[1]-1));t.SetEnd(v(z[0]+11,z[1]-1));t.SetWidth(p.FromMM(width));t.SetLayer(layer);t.SetNet(netmap[net]);t.SetLocked(True);b.Add(t)
route('XOUT',[(19.6,32.57),(19.6,35.47)])
route('XTAL_OUT',[(19.6,36.33),(20.3,37.03),(21.16,37.03),(21.16,37.5),(21.1,37.56),(21.1,39.05)])
route('XIN',[(19.2,32.57),(19.2,34.4),(19,34.6),(19,37.5)])
route('XIN',[(19,37.5),(20,38.5),(20,40.1),(19.35,40.75),(18.9,40.75)])
area([(0,-6),(70,-6),(70,60),(0,60)],[p.F_Cu],False,'GND')
area([(0,-6),(70,-6),(70,60),(0,60)],[p.B_Cu],False,'GND')
area([(0,-6),(70,-6),(70,60),(0,60)],[p.In1_Cu],False,'GND')
area([(0,-6),(70,-6),(70,60),(0,60)],[p.In2_Cu],False,'GND')
p.SaveBoard(str(R/'amigahid-pico.kicad_pcb'),b)
subprocess.run(['python3',str(R/'scripts/configure_rules.py')],check=True)
subprocess.run(['python3',str(R/'scripts/sync_assembly_fields.py')],check=True)
subprocess.run(['python3',str(R/'scripts/apply_orientation_marks.py')],check=True)
(R/'placement.json').write_text(json.dumps(positions,indent=2)+'\n')
print(f'Built {len(fps)} top-side footprints, 70 x 66 mm, four copper layers.')
