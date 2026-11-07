#!/usr/bin/env python3
"""Set fabrication constraints after pcbnew creates a fresh project."""
from pathlib import Path
import json,copy
r=Path(__file__).resolve().parents[1];f=r/'amigahid-pico.kicad_pro';p=json.loads(f.read_text())
s=p['board']['design_settings'];s['drc_exclusions']=[]
s['rules'].update(min_clearance=.127,min_track_width=.127,min_via_diameter=.45,min_through_hole_diameter=.2,min_copper_edge_clearance=.25,min_hole_clearance=.2,min_hole_to_hole=.25,min_text_height=.6)
c=p['net_settings']['classes'][0];c.update(clearance=.127,track_width=.2,via_diameter=.5,via_drill=.25,diff_pair_width=.2474,diff_pair_gap=.1501)
p['net_settings']['classes']=[c]
p['net_settings']['netclass_patterns']=[]
for name,width,nets in [('Power5V',.6,['AMIGA_5V','USB_5V','SYS_5V','USB_C_VBUS','HOST_POWER_RAW']),
                        ('Power3V3',.3,['+3V3']),('Core',.2,['+1V1']),
                        ('USB',.2,['USB_D+','USB_D-','USB_HOST_D+','USB_HOST_D-',
                                    'USB_SERVICE_D+','USB_SERVICE_D-',
                                    'USB1_D+','USB1_D-','USB2_D+','USB2_D-'])]:
    cls=copy.deepcopy(c);cls.update(name=name,track_width=width)
    p['net_settings']['classes'].append(cls)
    p['net_settings']['netclass_patterns'] += [{'netclass':name,'pattern':n} for n in nets]
f.write_text(json.dumps(p,indent=2)+'\n')
