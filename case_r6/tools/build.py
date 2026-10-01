#!/usr/bin/env python3
"""Render printable parts, previews, CSG, and check simplified assembly interference."""
import os,shutil,subprocess,sys,json
from pathlib import Path
R=Path(__file__).resolve().parents[1];out=R/'build';out.mkdir(exist_ok=True)
scad=os.environ.get('OPENSCAD') or shutil.which('openscad') or shutil.which('openscad-nightly')
if not scad and Path('/snap/bin/openscad-nightly').exists():scad='/snap/bin/openscad-nightly'
if not scad:sys.exit('Install OpenSCAD or set OPENSCAD to its executable.')
def run(view,ext,extra=(),defs=(),empty=False):
 target=out/f'{view}.{ext}'
 if empty and target.exists():target.unlink()
 cmd=[scad,'-o',str(target),'-D',f'view="{view}"',*extra]
 for define in defs:cmd+=['-D',define]
 cmd+=[str(R/'amigahid-pico-case.scad')]
 result=subprocess.run(cmd,capture_output=True,text=True);log=result.stdout+result.stderr
 (out/f'{view}-{ext}.log').write_text(log)
 assert 'WARNING:' not in log and 'ERROR:' not in log,log
 if empty:
  assert 'Current top level object is empty' in log, 'Case intersects a modelled hardware envelope; inspect collision.stl'
 else:assert result.returncode==0 and target.exists(),log
for view in ['base','lid','print']:run(view,'stl')
for view in ['base','lid']:run(view,'csg')
run('collision','stl',empty=True)
(out/'fit-validation.json').write_text(json.dumps({'hardware_intersection':'empty','intentional_support_contact_tolerance_mm':0.01,'physical_fit_tested':False},indent=2)+'\n')
for view in ['assembled','fit','print']:
 run(view,'png',extra=['--imgsize=1400,1100','--autocenter','--viewall','--projection=ortho','--camera=0,0,0,55,0,35,0'])
subprocess.run([sys.executable,str(R/'tools/verify_mesh.py')],check=True)
print('Built and verified:',out)
