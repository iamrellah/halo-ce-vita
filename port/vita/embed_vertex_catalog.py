"""Private generated build catalog; derived shader payloads must not be committed."""
from pathlib import Path
from vertex_layout import validate_raw_layout
import argparse,re,hashlib,json,struct
p=argparse.ArgumentParser();p.add_argument('--xita-source',required=True,type=Path);p.add_argument('--shader-dir',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];out=root/'build/vita';out.mkdir(parents=True,exist_ok=True)
layouts=(a.shader_dir/'xv_layouts.h').read_text().replace('#include "xv_shader.h"','#include "halo_vita_vertex_program.h"').replace('#include "runtime/xv_shader.h"','#include "halo_vita_vertex_program.h"')
def words(text):
 text=re.sub(r'/\*.*?\*/','',text,flags=re.S)
 return b''.join(struct.pack('<I',int(x,16)) for x in re.findall(r'0x[0-9a-fA-F]+',text))
blob=words((root/'source/rasterizer/xbox/rasterizer_xbox_vertex_shaders_data.inc').read_text())
entries=re.findall(r'VERTEX_SHADER_ENTRY\((0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+)\)',(root/'source/rasterizer/xbox/rasterizer_xbox_vertex_shaders.c').read_text())
init=(root/'source/rasterizer/xbox/rasterizer_xbox_vertex_shaders_initialize.c').read_text()
declarations=words(re.search(r'vertex_shader_declarations\[\]\s*=\s*\{(.*?)\};',init,re.S)[1])
assignments={int(i):int(off,16) for i,off in re.findall(r'vertex_shader_table\[(\d+)\]\.declaration\s*=\s*&vertex_shader_declarations\[(0x[0-9A-Fa-f]+) / sizeof\(unsigned long\)\]',init)}
if len(entries)!=67 or len(assignments)!=67:raise ValueError('Native shader table incomplete')
lines=[layouts];rows=[]
for i in range(67):
 name=f'halo_vs_{i:02d}';path=a.shader_dir/f'{name}.gxp';data=path.read_bytes()
 if len(data)<64 or len(data)>1024*1024 or data[:3]!=b'GXP':raise ValueError('Invalid GXP envelope')
 if not re.search(r'static const xv_vs_desc_t xv_vs_'+name+r'\s*=',layouts):raise ValueError('Missing layout')
 lines.append(f'static const unsigned char native_{name}[] __attribute__((aligned(16)))={{')
 lines.extend(','.join(map(str,data[j:j+16]))+',' for j in range(0,len(data),16));lines.append('};')
 off,n=map(lambda v:int(v,16),entries[i]);program=blob[off:off+n]
 if len(program)!=n or n!=4+16*(struct.unpack_from('<I',program)[0]>>16):raise ValueError('Native program extent')
 start=assignments[i];end=start
 while end+4<=len(declarations):
  token=struct.unpack_from('<I',declarations,end)[0];end+=4
  if token==0xffffffff:break
 else:raise ValueError('Declaration terminator')
 declaration=declarations[start:end]
 raw_layout=validate_raw_layout(name,declaration,layouts)
 body=re.search(r'static const xv_vs_desc_t xv_vs_'+name+r'\s*=\s*\{(.*?)\};',layouts,re.S)[1]
 metadata=re.search(r'(0x[0-9A-Fa-f]+)u,\s*(0x[0-9A-Fa-f]+)u,\s*(0x[0-9A-Fa-f]+)u,\s*(\d+)u\s*$',body.strip())
 h=2166136261
 for byte in program:h=((h^byte)*16777619)&0xffffffff
 if not metadata or int(metadata[3],16)!=h or int(metadata[4])!=n:raise ValueError('Native/catalog program mismatch')
 for suffix,payload in [('original',program),('declaration',declaration)]:
  lines.append(f'static const unsigned char native_{name}_{suffix}[]={{'+','.join(map(str,payload))+'};')
 rows.append(dict(**raw_layout,path=str(path.resolve()),layout_sha256=hashlib.sha256(layouts.encode()).hexdigest(),index=i,bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),original_sha256=hashlib.sha256(program).hexdigest(),declaration_sha256=hashlib.sha256(declaration).hexdigest()))
lines.append('static const struct {const xv_vs_desc_t *desc;const unsigned char *data;unsigned bytes;const unsigned char *original,*declaration;unsigned original_bytes,declaration_bytes;} native_vertex_catalog[]={')
for i in range(67):
 name=f'halo_vs_{i:02d}';lines.append(f'{{&xv_vs_{name},native_{name},sizeof(native_{name}),native_{name}_original,native_{name}_declaration,sizeof(native_{name}_original),sizeof(native_{name}_declaration)}},')
lines.append('};');(out/'native_vertex_catalog.h').write_text('\n'.join(lines)+'\n')
(out/'native-vertex-inputs.json').write_text(json.dumps(rows,indent=2)+'\n')
print('Embedded 67 existing translated vertex programs (private build output)')
