"""Offline byte comparison only; no game execution or proprietary payload output."""
from pathlib import Path
import argparse,hashlib,json,re,struct
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--xita-source',type=Path,required=True)
p.add_argument('--xbe',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[2]
def words(text):
    text=re.sub(r'/\*.*?\*/','',text,flags=re.S)
    return b''.join(struct.pack('<I',int(x,16)) for x in re.findall(r'0x[0-9a-fA-F]+',text))
def fnv(data):
    h=2166136261
    for b in data:h=((h^b)*16777619)&0xffffffff
    return h
xbe=a.xbe.read_bytes()
def span(offset,size):
    if offset<0 or size<0 or offset+size>len(xbe):raise ValueError('XBE range')
    return xbe[offset:offset+size]
def u32(offset):return struct.unpack('<I',span(offset,4))[0]
if span(0,4)!=b'XBEH':raise ValueError('Not XBE')
base=u32(0x104);section_count=u32(0x11c);table=u32(0x120)-base
if section_count>256:raise ValueError('Section count')
sections=[]
for i in range(section_count):
    _,va,vs,raw,rs=struct.unpack('<5I',span(table+i*56,20))
    span(raw,rs);sections.append((va,raw,rs))
def read_va(va,size):
    matches=[span(raw+va-start,size) for start,raw,n in sections if start<=va and va+size<=start+n]
    if len(matches)!=1:raise ValueError('Unmapped/ambiguous shader address')
    return matches[0]
blob=words((root/'source/rasterizer/xbox/rasterizer_xbox_vertex_shaders_data.inc').read_text())
entries=re.findall(r'VERTEX_SHADER_ENTRY\((0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+)\)',(root/'source/rasterizer/xbox/rasterizer_xbox_vertex_shaders.c').read_text())
init=(root/'source/rasterizer/xbox/rasterizer_xbox_vertex_shaders_initialize.c').read_text()
declarations=words(re.search(r'vertex_shader_declarations\[\]\s*=\s*\{(.*?)\};',init,re.S)[1])
assignments={int(i):int(off,16) for i,off in re.findall(r'vertex_shader_table\[(\d+)\]\.declaration\s*=\s*&vertex_shader_declarations\[(0x[0-9A-Fa-f]+) / sizeof\(unsigned long\)\]',init)}
layouts=(a.xita_source/'shaders/xv_layouts.h').read_text();lookup={}
for name,body in re.findall(r'static const xv_vs_desc_t (\w+) = \{(.*?)\};',layouts,re.S):
    m=re.search(r'(0x[0-9A-F]+)u,\s*(0x[0-9A-F]+)u,\s*(0x[0-9A-F]+)u,\s*(\d+)u\s*$',body.strip())
    if m:
        va,dva,h,n=[int(x,16) for x in m.groups()[:3]]+[int(m[4])]
        lookup.setdefault((h,n),[]).append((name,va,dva))
rows=[]
for i,(off,n) in enumerate(entries):
    off=int(off,16);n=int(n,16);program=blob[off:off+n]
    if len(program)!=n or n!=4+16*(struct.unpack_from('<I',program)[0]>>16):raise ValueError('Program extent')
    dstart=assignments[i];dend=dstart
    while dend+4<=len(declarations):
        token=struct.unpack_from('<I',declarations,dend)[0];dend+=4
        if token==0xffffffff:break
    else:raise ValueError('Declaration terminator')
    declaration=declarations[dstart:dend];candidates=[]
    for name,va,dva in lookup.get((fnv(program),n),[]):
        candidates.append(dict(layout=name,program_equal=read_va(va,n)==program,
            declaration_equal=read_va(dva,len(declaration))==declaration))
    rows.append(dict(shader=i,program_sha256=hashlib.sha256(program).hexdigest(),
        declaration_sha256=hashlib.sha256(declaration).hexdigest(),candidates=candidates))
matched=sum(any(c['program_equal'] and c['declaration_equal'] for c in row['candidates']) for row in rows)
report=dict(scope='Offline original bytecode + declaration identity, not translated GXP correctness or runtime validation',
    xbe_sha256=hashlib.sha256(xbe).hexdigest(),layout_sha256=hashlib.sha256(layouts.encode()).hexdigest(),
    shaders=len(rows),exact_program_and_declaration_matches=matched,rows=rows)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='rows'}))
