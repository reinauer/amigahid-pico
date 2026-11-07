#!/usr/bin/env python3
"""One-time migration of the preserved r6 circuit into the r7 design manifest.

Run only when deliberately starting the r7 migration again. Subsequent edits
belong in circuit.json; build_schematic.py does not depend on the r6 directory.
"""
import copy
import csv
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from sexpr import parse, get, children, dump, Atom

R = Path(__file__).resolve().parents[1]
old = parse((R.parent/'kicad_r6/amigahid-pico.kicad_sch').read_text())
netlist = ET.parse('/private/tmp/r7-r6-netlist.xml').getroot()
nets = {}
for net in netlist.find('nets'):
    name = net.get('name').lstrip('/')
    if name.startswith('unconnected-'):
        continue
    for node in net:
        nets.setdefault(node.get('ref'), {})[node.get('pin')] = name
sources = {r['Reference']: r for r in csv.DictReader((R/'SOURCING.csv').open())}
imported = parse((R/'lcsc.kicad_sym').read_text())
catalog = {}
for sym in children(imported, 'symbol'):
    props = {p[1]:p[2] for p in children(sym, 'property')}
    code = props.get('LCSC Part', props.get('LCSC Part #', ''))
    # Converter fields have changed names between releases; use the API cache.
    for cache in (R/'.easyeda_cache').glob('*.json'):
        result = json.loads(cache.read_text()).get('result') or {}
        if result.get('title', '').replace(' ', '') == str(sym[1]).replace(' ', ''):
            code = cache.stem
            break
    if code:
        catalog[code] = dict(symbol=sym[1], footprint=props['Footprint'],
                             mpn=props.get('MPN', ''), manufacturer=props.get('Manufacturer', ''),
                             datasheet=props.get('Datasheet', ''))
local = {}
for sym in children(get(old, 'lib_symbols'), 'symbol'):
    sym = copy.deepcopy(sym)
    name = sym[1].split(':')[-1]
    sym[1] = name
    local[name] = sym
parts = []
for sym in children(old, 'symbol'):
    props = {p[1]:p[2] for p in children(sym, 'property')}
    ref = props['Reference']
    if ref == 'U1' or ref.startswith('#'):
        continue
    source = sources.get(ref, {})
    code = source.get('LCSC Part #', '')
    footprint = catalog.get(code, {}).get('footprint', props['Footprint'])
    if ref == 'J4': footprint = 'lcsc:USB_A_C7279912'
    if ref in ('J5', 'J6'): footprint = 'lcsc:DE9_Female_C50380980'
    parts.append(dict(ref=ref, value=props['Value'], symbol=get(sym, 'lib_id')[1].split(':')[-1],
                      footprint=footprint, lcsc=code, mpn=source.get('MPN', ''),
                      manufacturer=source.get('Manufacturer', ''), datasheet=source.get('Datasheet', ''),
                      nets=nets.get(ref, {}), sheet='interfaces',
                      sch=[float(x) for x in get(sym,'at')[1:3]],
                      source_note='Retained r6 electrical function; LCSC package reviewed for r7.'))

def block(name, left, right, width=30.48):
    """Supplier-verified pin table laid out on the KiCad 50 mil grid."""
    h = max(len(left), len(right))*2.54
    s = parse(f'(symbol "{name}" (pin_names (offset 0.5)) (in_bom yes) (on_board yes) '
              '(property "Reference" "U" (at 0 5.08 0) (effects (font (size 1.27 1.27)))) '
              f'(property "Value" "{name}" (at 0 2.54 0) (effects (font (size 1.27 1.27)))))')
    s.append(parse(f'(symbol "{name}_0_1" (rectangle (start {-width/2} 2.54) '
                   f'(end {width/2} {-h}) (stroke (width 0.254) (type default)) (fill (type background))))'))
    sub = [Atom('symbol'), name+'_1_1']
    for side, pins in [('L',left),('R',right)]:
        for i,(num,label,typ) in enumerate(pins):
            x = -width/2-5.08 if side=='L' else width/2+5.08
            angle = 0 if side=='L' else 180
            sub.append(parse(f'(pin {typ} line (at {x} {-i*2.54} {angle}) (length 5.08) '
                             f'(name {json.dumps(label)} (effects (font (size 1 1)))) '
                             f'(number "{num}" (effects (font (size 1 1)))))'))
    s.append(sub)
    local[name] = s

rp = next(s for s in children(imported,'symbol') if s[1]=='RP2350A_C42411118')
rpnames = {get(p,'number')[1]: get(p,'name')[1] for sub in children(rp,'symbol') for p in children(sub,'pin')}
gpio = sorted([(int(name[4:].split('_')[0]),num,name) for num,name in rpnames.items() if name.startswith('GPIO')])
left = [(num,name,'bidirectional') for _,num,name in gpio]
right = []
for num,name in rpnames.items():
    if name.startswith('GPIO'):continue
    typ = ('power_in' if name in ['IOVDD','DVDD','ADC_AVDD','VREG_AVDD','VREG_VIN','VREG_PGND','USB_OTP_VDD','QSPI_IOVDD','GND']
           else 'power_out' if name=='VREG_LX'
           else 'input' if name in ['XIN','SWCLK','RUN','VREG_FB']
           else 'output' if name in ['XOUT','QSPI_SCLK','QSPI_SS'] else 'bidirectional')
    right.append((num,name,typ))
block('RP2350A',left,right,40.64)
block('RMC20452T',[(1,'GND','power_in'),(2,'NC','no_connect'),(3,'gSPI_CLK','input'),(4,'GND','power_in'),(5,'gSPI_DIN','input'),(6,'gSPI_DOUT','tri_state'),(7,'GND','power_in'),(8,'GPIO0','bidirectional'),(9,'gSPI_CS','input'),(10,'nIRQ','output'),(11,'GND','power_in')],
      [(21,'GND','power_in'),(20,'NC','no_connect'),(19,'NC','no_connect'),(18,'GPIO1','bidirectional'),(17,'GPIO2','bidirectional'),(16,'VIN','power_in'),(15,'GND','power_in'),(14,'VDDIO','power_in'),(13,'BT_ON','input'),(12,'WL_ON','input')],35.56)
block('W25Q32JVSNIQ',[(1,'~{CS}','input'),(6,'CLK','input'),(5,'IO0','bidirectional'),(2,'IO1','bidirectional')],[(8,'VCC','power_in'),(4,'GND','power_in'),(3,'IO2','bidirectional'),(7,'IO3','bidirectional')],20.32)
block('AP63203WU-7',[(3,'VIN','power_in'),(2,'EN','input'),(4,'GND','power_in')],[(6,'BST','passive'),(5,'SW','power_out'),(1,'FB','input')],20.32)
for name in ['AOTA-B201610S3R3-101-T','ABM8-272-T3_C20625731','TS-1088-AR02016','microUSBFemale','SRN4018-4R7M','KT-0603R']:
    sy = copy.deepcopy(next(s for s in children(imported,'symbol') if s[1]==name))
    for sub in children(sy,'symbol'):
        for pin in children(sub,'pin'):pin[1]=Atom('passive')
    local[name]=sy

def add(ref,value,code,symbol,ns,sheet,xy,note=''):
    cat = catalog[code]
    parts.append(dict(ref=ref,value=value,lcsc=code,symbol=symbol,footprint=cat['footprint'],
                      mpn=cat['mpn'],manufacturer=cat['manufacturer'],datasheet=cat['datasheet'],
                      nets={str(k):v for k,v in ns.items()},sheet=sheet,sch=list(xy),source_note=note))

# Preserve BOARD_HIDPICO_REV5 application GPIOs, matching Pico W module pins.
physical_gpio = {1:0,2:1,4:2,5:3,6:4,7:5,9:6,10:7,11:8,12:9,14:10,15:11,16:12,17:13,19:14,20:15,21:16,22:17,24:18,25:19,26:20,27:21,29:22,31:26,32:27,34:28}
gpio_pin={g:str(num) for g,num,_ in gpio}
rpnet={gpio_pin[g]:nets['U1'][str(module)] for module,g in physical_gpio.items() if str(module) in nets['U1']}
rpnet.update({str(p):'+3V3' for p in [1,11,20,30,38,44,45,49,53,54]})
rpnet.update({str(p):'+1V1' for p in [6,23,39,50]})
rpnet.update({'21':'XIN','22':'XOUT','24':'SWCLK','25':'SWDIO','26':'RUN','35':'WL_ON','36':'WL_DATA','37':'WL_CS','43':'WL_CLK','46':'VREG_AVDD','47':'GND','48':'VREG_LX','51':'USB_DM_MCU','52':'USB_DP_MCU','55':'QSPI_SD3','56':'QSPI_CLK','57':'QSPI_SD0','58':'QSPI_SD2','59':'QSPI_SD1','60':'QSPI_CS','61':'GND'})
add('U1','RP2350A','C42411118','RP2350A',rpnet,'controller',(76.2,45.72),'RP2350A QFN-60 plus exposed GND pad 61; GPIO map preserved from r6.')
add('U4','RMC20452T','C49451547','RMC20452T',{1:'GND',3:'WL_CLK',4:'GND',5:'WL_DATA',6:'WL_DOUT',7:'GND',8:'WL_LED',9:'WL_CS',10:'WL_IRQ',11:'GND',12:'WL_ON',13:'WL_ON',14:'+3V3',15:'GND',16:'+3V3',21:'GND'},'controller',(284.48,48.26),'RM2 datasheet DS-6 Table 2: shared GP24 data/IRQ via 470R and 10k; GP23 enable, GP25 CS, GP29 CLK.')
add('U5','W25Q32JVSNIQ / 4MB','C5355146','W25Q32JVSNIQ',{1:'QSPI_CS',2:'QSPI_SD1',3:'QSPI_SD2',4:'GND',5:'QSPI_SD0',6:'QSPI_CLK',7:'QSPI_SD3',8:'+3V3'},'controller',(180.34,48.26))
add('U6','AP63203WU-7 / 3V3','C780769','AP63203WU-7',{1:'+3V3',2:'VSYS',3:'VSYS',4:'GND',5:'BUCK_SW',6:'BUCK_BST'},'power',(76.2,48.26),'3V3 buck supply for MCU, RM2 and OLED; design load budget <= 0.8 A; 4.7uH 1.9A inductor.')
add('L1','3.3uH / dotted end = 1V1','C42411119','AOTA-B201610S3R3-101-T',{1:'+1V1',2:'VREG_LX'},'power',(180.34,48.26),'Use marked Abracon part. Pad 1 (dot) goes to output, pad 2 to LX; follow Raspberry Pi R4-S1 regulator placement.')
add('L2','4.7uH / 1.9A','C780206','SRN4018-4R7M',{1:'BUCK_SW',2:'+3V3'},'power',(111.76,48.26))
add('Y1','ABM8-272-T3 / 12MHz','C20625731','ABM8-272-T3_C20625731',{1:'XIN',3:'XTAL_OUT',2:'GND',4:'GND'},'controller',(182.88,101.6))
add('SW1','BOOTSEL','C720477','TS-1088-AR02016',{1:'BOOTSEL',2:'GND'},'controller',(180.34,157.48))
add('SW2','RESET','C720477','TS-1088-AR02016',{1:'RUN',2:'GND'},'controller',(231.14,157.48))
add('J10','USB programming / service','C10418','microUSBFemale',{1:'PROG_5V',2:'USB_D-',3:'USB_D+',5:'GND',**{x:'GND' for x in ['SH0','SH1','SH2','SH3']}},'controller',(355.6,132.08),'Shares native USB data with host port. Disconnect USB host devices before attaching a PC.')
add('D2','B5819W','C8598','D_Schottky',{1:'VSYS',2:'PROG_5V'},'power',(30.48,48.26),'Diode OR with D1: programming VBUS powers only VSYS/core, not Amiga +5V or host VBUS.')
add('D3','Status / red','C2286','KT-0603R',{1:'GND',2:'LED_A'},'controller',(353.06,76.2))
resistors = [
 ('R3','33R','C25105','+3V3','VREG_AVDD','power',(233.68,48.26)),
 ('R4','1k','C11702','XOUT','XTAL_OUT','controller',(152.4,101.6)),
 ('R5','10k','C25744','+3V3','QSPI_CS','controller',(157.48,73.66)),
 ('R6','1k','C11702','QSPI_CS','BOOTSEL','controller',(154.94,157.48)),
 ('R7','27R','C25100','USB_DM_MCU','USB_D-','controller',(157.48,210.82)),
 ('R8','27R','C25100','USB_DP_MCU','USB_D+','controller',(205.74,210.82)),
 ('R9','470R','C25118','WL_DOUT','WL_DATA','controller',(246.38,104.14)),
 ('R10','10k','C25744','WL_IRQ','WL_DATA','controller',(292.1,104.14)),
 ('R11','1k','C11702','WL_LED','LED_A','controller',(353.06,48.26)),
 ('R12','10k','C25744','+3V3','RUN','controller',(231.14,182.88)),
 ('R13','10k','C25744','WL_ON','GND','controller',(246.38,129.54)),
]
for ref,value,code,a,b,sheet,xy in resistors:add(ref,value,code,'R',{1:a,2:b},sheet,xy)
caps = [
 ('C6','4.7u / 10V','C23733','+3V3','GND',(180.34,73.66)),
 ('C7','4.7u / 10V','C23733','+1V1','GND',(205.74,73.66)),
 ('C8','4.7u / 10V','C23733','VREG_AVDD','GND',(233.68,73.66)),
 ('C9','10u / 25V','C15850','VSYS','GND',(53.34,81.28)),
 ('C10','22u / 6.3V','C59461','+3V3','GND',(83.82,81.28)),
 ('C11','22u / 6.3V','C59461','+3V3','GND',(111.76,81.28)),
 ('C12','100n / 16V','C1525','BUCK_BST','BUCK_SW',(137.16,48.26)),
 ('C13','100n / 16V','C1525','+3V3','GND',(30.48,152.4)),
 ('C14','100n / 16V','C1525','+1V1','GND',(55.88,152.4)),
 ('C15','100n / 16V','C1525','+3V3','GND',(81.28,152.4)),
 ('C16','100n / 16V','C1525','+3V3','GND',(106.68,152.4)),
 ('C17','100n / 16V','C1525','+1V1','GND',(132.08,152.4)),
 ('C18','100n / 16V','C1525','+3V3','GND',(157.48,152.4)),
 ('C19','100n / 16V','C1525','+3V3','GND',(182.88,152.4)),
 ('C20','100n / 16V','C1525','+1V1','GND',(208.28,152.4)),
 ('C21','100n / 16V','C1525','+3V3','GND',(233.68,152.4)),
 ('C22','100n / 16V','C1525','+3V3','GND',(259.08,152.4)),
 ('C23','100n / 16V','C1525','+3V3','GND',(284.48,152.4)),
 ('C24','100n / 16V','C1525','+3V3','GND',(309.88,152.4)),
 ('C25','100n / 16V','C1525','+3V3','GND',(335.28,152.4)),
 ('C26','10u / 25V','C15850','+3V3','GND',(335.28,81.28)),
 ('C27','100n / 16V','C1525','+3V3','GND',(360.68,152.4)),
]
for ref,val,code,a,b,xy in caps:add(ref,val,code,'C',{1:a,2:b},'power',xy)
for ref,net,x in [('C28','XIN',175.26),('C29','XTAL_OUT',203.2)]:add(ref,'15p / C0G','C86285','C',{1:net,2:'GND'},'controller',(x,127))
for i,net in enumerate(['+5V','GND','VSYS','USB_5V','+3V3','+1V1','PROG_5V']):
    parts.append(dict(ref=f'#FLG0{i+1}',value='PWR_FLAG',symbol='PWR_FLAG',footprint='',lcsc='',mpn='',manufacturer='',datasheet='',nets={'1':net},sheet='power',sch=[30.48+i*40.64,228.6],source_note='External input or converter output supply declaration.'))
(R/'r7.kicad_sym').write_text('(kicad_symbol_lib (version 20250114) (generator "kicad_symbol_editor")\n'+'\n'.join(dump(s) for s in local.values())+'\n)\n')
(R/'circuit.json').write_text(json.dumps(dict(parts=parts,catalog=catalog),indent=2)+'\n')
print(f'Created circuit.json: {len(parts)} symbols across three sheets')
