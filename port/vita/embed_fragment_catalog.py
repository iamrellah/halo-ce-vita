"""Embed a selected Xita fragment catalog into PRIVATE generated build output."""
from pathlib import Path
import argparse, re, hashlib, json
p=argparse.ArgumentParser();p.add_argument('--shader-dir',type=Path,required=True);a=p.parse_args()
out=Path(__file__).resolve().parents[2]/'build/vita';out.mkdir(parents=True,exist_ok=True)
text=(a.shader_dir/'xv_ps_table.h').read_text()
body=re.search(r'static const xv_ps_entry_t xv_ps_table\[\]\s*=\s*\{(.*?)\n\};',text,re.S)[1]
pattern=r'\{\s*(0x[0-9A-F]+)u,\s*(0x[0-9A-F]+)u,\s*"app0:shaders/([^"/]+)",\s*(0x[0-9A-F]+),\s*(0x[0-9A-F]+),\s*(0x[0-9A-F]+),\s*(0x[0-9A-F]+)u\s*\}'
rows=re.findall(pattern,body)
if not rows or len(rows)!=body.count('{'):raise ValueError('Unknown/incomplete fragment catalog schema')
keys=[(int(r[0],16),int(r[6],16),int(r[5],16)) for r in rows]
if keys!=sorted(set(keys)):raise ValueError('Unsorted/duplicate fragment keys')
lines=[];assets={};receipt=[]
for row in rows:
 name=row[2]
 if not re.fullmatch(r'ps_[A-Za-z0-9_]+\.frag\.gxp',name):raise ValueError('Unsafe shader name')
 if name in assets:continue
 data=(a.shader_dir/name).read_bytes()
 if len(data)<64 or len(data)>1024*1024 or data[:3]!=b'GXP':raise ValueError('Invalid GXP envelope')
 symbol='native_fragment_'+str(len(assets));assets[name]=symbol
 lines.append(f'static const unsigned char {symbol}[] __attribute__((aligned(16)))={{')
 lines.extend(','.join(map(str,data[i:i+16]))+',' for i in range(0,len(data),16));lines.append('};')
 receipt.append(dict(path=str((a.shader_dir/name).resolve()),bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
lines.append('static const struct halo_vita_fragment_entry native_fragments[]={')
for vs,raw,name,cube,modes,c2d,key in rows:
 sym=assets[name]
 lines.append(f'{{{vs}u,{key}u,{raw}u,{cube},{modes},{c2d},{sym},sizeof({sym})}},')
lines.append('};')
(out/'native_fragment_catalog.h').write_text('\n'.join(lines)+'\n')
(out/'native-fragment-inputs.json').write_text(json.dumps(dict(entries=len(rows),table_sha256=hashlib.sha256(text.encode()).hexdigest(),assets=receipt),indent=2)+'\n')
print(f'Embedded {len(rows)} fragment entries, {len(assets)} distinct GXP payloads')
