import re,json,pathlib
class Atom(str): pass
def parse(s):
 ts=re.findall(r'"(?:\\.|[^"\\])*"|\(|\)|[^\s()]+',s); stack=[]; out=None
 for t in ts:
  if t=='(': stack.append([])
  elif t==')':
   v=stack.pop()
   if stack: stack[-1].append(v)
   else: out=v
  else: stack[-1].append(json.loads(t) if t.startswith('"') else Atom(t))
 return out
def get(a,k): return next((x for x in a if isinstance(x,list) and x[0]==k),None)
def children(a,k):return [x for x in a if isinstance(x,list) and x[0]==k]
def dump(a):
 if isinstance(a,list):return '('+' '.join(dump(x) for x in a)+')'
 return str(a) if isinstance(a,Atom) else json.dumps(a)
if __name__=='__main__':
 for lib,name in [('MCU_Module','RaspberryPi_Pico_Extensive'),('Connector','DB9_Female'),('Device','R_Pack04'),('Connector','USB_A')]:
  data=parse(pathlib.Path('/usr/share/kicad/symbols/'+lib+'.kicad_sym').read_text())
  sy=next((x for x in children(data,'symbol') if x[1]==name),None)
  print(lib,name)
  if sy:
   for sub in children(sy,'symbol'):
    for p in children(sub,'pin'): print(get(p,'number')[1],get(p,'name')[1],get(p,'at')[1:])
