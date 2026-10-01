"""Embed Xita's runtime-written clear shaders in PRIVATE build output only."""
from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--xita-source',type=Path,required=True);p.add_argument('--shader-dir',type=Path,required=True);a=p.parse_args()
out=Path(__file__).resolve().parents[2]/'build/vita';out.mkdir(parents=True,exist_ok=True)
lines=['/* Generated from Xita runtime shaders; retain Xita license on redistribution. */']
receipt=[]
for name,filename in [('native_clear_vs','xv_clear.gxp'),('native_clear_fs','xv_color.frag.gxp'),('native_texture_vs','xv_test_tex.gxp'),('native_texture_fs','xv_tex0.frag.gxp')]:
 source=a.shader_dir/filename;data=source.read_bytes()
 if len(data)<64 or len(data)>1024*1024 or data[:3]!=b'GXP': raise SystemExit('Invalid GXP envelope: '+str(source))
 lines.append('static const unsigned char '+name+'[] __attribute__((aligned(16))) = {')
 for start in range(0,len(data),16):lines.append(','.join(str(v) for v in data[start:start+16])+',')
 lines.append('};');receipt.append(dict(path=str(source),bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
(out/'clear_gxp.h').write_text('\n'.join(lines)+'\n');(out/'clear-shader-inputs.json').write_text(json.dumps(receipt,indent=2)+'\n')
