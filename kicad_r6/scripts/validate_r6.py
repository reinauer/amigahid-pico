#!/usr/bin/python3
"""Validate r6 connectivity, mechanics, USB matching, ERC and schematic/PCB parity.
Run with system Python (KiCad pcbnew), from any directory. Writes only review reports.
"""
from pathlib import Path
import subprocess,xml.etree.ElementTree as E,json,math,csv,re
import pcbnew as p
R=Path(__file__).resolve().parents[1];review=R/'review';sch=R/'amigahid-pico.kicad_sch';board=R/'amigahid-pico.kicad_pcb'
def run(*args):subprocess.run(['kicad-cli',*map(str,args)],check=True,cwd=R)
run('sch','export','netlist','--format','kicadxml','-o',review/'netlist.xml',sch)
run('sch','erc','--exit-code-violations','--format','json','-o',review/'erc.json',sch)
run('pcb','drc','--schematic-parity','--exit-code-violations','--format','json','-o',review/'drc.json',board)
r=E.parse(review/'netlist.xml').getroot();b=p.LoadBoard(str(board));fps={f.GetReference():f for f in b.GetFootprints()};nets={}
for net in r.find('nets'):
 for node in net:
  ref,pin=node.attrib['ref'],node.attrib['pin'];nets[ref,pin]=net.attrib['name']
  if ref.startswith('#'):continue
  pads=[v for v in fps[ref].Pads() if v.GetNumber()==pin];assert pads,(ref,pin)
  assert all(v.GetNetname()==net.attrib['name'] for v in pads),(ref,pin)
def expect(ref,pin,net):assert nets[ref,str(pin)]==('GND' if net=='GND' else '/'+net),(ref,pin,net,nets[ref,str(pin)])
def nc(ref,pin):assert nets[ref,str(pin)].startswith('unconnected-'),(ref,pin)
# Pin table checked against TI SCDS084G, not inferred from part-number similarity.
high=['KCLK','KDAT','KRESET','P1_RIGHT','P1_LEFT','P1_DOWN','P1_UP','P1_FIRE','P1_B2','P1_B3','P2_UP','P2_DOWN','P2_LEFT','P2_RIGHT','P2_FIRE','P2_B2','P2_B3']
physical=[9,7,6,10,11,12,14,15,16,17,32,31,29,27,26,25,24]
for i,(name,pin) in enumerate(zip(high,physical)):
 ref='U2' if i<10 else 'U3';j=i if i<10 else i-10
 expect('U1',pin,'L_'+name);expect(ref,j+2,'L_'+name);expect(ref,22-j,name)
for ref in ['U2','U3']:
 expect(ref,24,'+5V');expect(ref,12,'GND');expect(ref,23,'GND');nc(ref,1)
for port in [1,2]:
 for pin,suffix in {1:'UP',2:'DOWN',3:'LEFT',4:'RIGHT',5:'B3',6:'FIRE',9:'B2'}.items():expect('J'+str(port+4),pin,f'P{port}_{suffix}')
 expect('J'+str(port+4),8,'GND');nc('J'+str(port+4),7)
for pin,name in {1:'KDAT',3:'GND',4:'AMIGA_5V',5:'KCLK'}.items():expect('J7',pin,name)
for pin in [2,6]:nc('J7',pin)
for pin,name in {1:'+3V3',2:'GND',3:'SCL',4:'SDA'}.items():expect('J3',pin,name)
for ref,pin,name in [('J9',1,'AUX_5V'),('J9',2,'GND'),('JP1',1,'AMIGA_5V'),('JP1',2,'+5V'),('JP1',3,'AUX_5V'),('D1',1,'VSYS'),('D1',2,'+5V'),('F1',1,'+5V'),('F1',2,'USB_5V'),('J4',1,'USB_5V'),('U1','TP2','USB_D-'),('U1','TP3','USB_D+')]:expect(ref,pin,name)
assert len({nets['JP1',str(i)] for i in [1,2,3]})==3,'Power sources must not be shorted on copper'
assert not any(f.GetValue() in ['BSS138','TXS0108','TXS0108E'] for f in fps.values())
positions={ref:(f.GetPosition().x/1e6,f.GetPosition().y/1e6) for ref,f in fps.items()}
x,y=positions['H1'];assert positions['H2']==(x+30.5,y);assert positions['H3']==(x,round(y+28.6,2));assert positions['H4']==(x+30.5,round(y+28.6,2))
assert 'PinSocket' in str(fps['J3'].GetFPID().GetLibItemName())
lengths={n:sum(t.GetLength() for t in b.GetTracks() if t.GetNetname()=='/'+n and not isinstance(t,p.PCB_VIA))/1e6 for n in ['USB_D+','USB_D-']}
assert abs(lengths['USB_D+']-lengths['USB_D-'])<.3,lengths
report=json.loads((review/'drc.json').read_text());assert not any(report[k] for k in ['violations','unconnected_items','schematic_parity'])
project=json.loads((R/'amigahid-pico.kicad_pro').read_text());assert not project['board']['design_settings']['drc_exclusions']
# Verify procurement fields against the reviewed stock snapshot and fitted packages.
source={row['Reference']:row for row in csv.DictReader((R/'SOURCING.csv').open())}
snapshot=json.loads((review/'sourcing/stock-snapshot.json').read_text())
from sexpr import parse,children
schematic_props={}
for symbol in children(parse(sch.read_text()),'symbol'):
 props={v[1]:v[2] for v in children(symbol,'property')}
 schematic_props[props['Reference']]=props
for ref,row in source.items():
 code=row['LCSC Part #']
 assert schematic_props[ref]['LCSC Part #']==code
 assert fps[ref].GetField('LCSC Part #').GetText()==code
 assert not fps[ref].HasField('LCSC #')
 assert 'LCSC #' not in schematic_props[ref]
 assert fps[ref].GetField('MPN').GetText()==row['MPN']
 if code:
  assert re.fullmatch(r'C[0-9]+',code)
  assert snapshot[code]['stock']>0 and snapshot[code]['ships_immediately']>0
  assert snapshot[code]['mpn']==row['MPN']
  assert str(snapshot[code]['stock'])==row['Stock at check']
 else:
  raise AssertionError(('Missing stocked selection', ref))
for ref in ['U2','U3']:
 assert fps[ref].GetValue()=='SN74CBTD3861DBQR'
 assert str(fps[ref].GetFPID().GetLibItemName())=='SSOP-24_3.9x8.7mm_P0.635mm'
 pads={v.GetNumber():v for v in fps[ref].Pads()}
 assert abs(pads['2'].GetPosition().y-pads['1'].GetPosition().y)==p.FromMM(.635)
for ref in ['J5','J6']:
 for pad in fps[ref].Pads():
  if pad.GetNumber().isdigit():assert pad.GetDrillSize().x==p.FromMM(1.1)
result={'status':'PASS','stocked_lcsc_components':32,'separately_sourced':[],'kicad':p.Version(),'pcb_components':len(fps),'checked_schematic_pin_assignments':sum(not ref.startswith('#') for ref,pin in nets),'level_shift_channels':17,'usb_trace_lengths_mm':lengths,'usb_skew_mm':abs(lengths['USB_D+']-lengths['USB_D-']),'oled_hole_pitch_mm':[30.5,28.6],'drc_violations':0,'unconnected':0,'schematic_parity_issues':0,'erc_violations':0,'bench_validation':'NOT PERFORMED','firmware':'Pico 2 W Bluetooth build blocked by existing flash-storage overlap; SH1106 driver not implemented'}
(review/'validation.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
