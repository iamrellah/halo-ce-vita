"""Compile-only reuse probe against an explicit existing Xita source checkout."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--xita-source',required=True,type=Path)
a=p.parse_args()
root=Path(__file__).resolve().parents[2]
out=root/'build/vita/texture-reuse';out.mkdir(parents=True,exist_ok=True)
source=a.xita_source.resolve()/'runtime'
headers=[source/'xv_texture_decode.h',source/'xv_bc_layout.h']
for h in headers:
    if not h.is_file(): raise SystemExit('missing decoder header: '+str(h))
command=['clang','--target=arm-vita-eabi','--sysroot=/home/birchwoodgod/vitasdk/arm-vita-eabi',
         '-mcpu=cortex-a9','-mfloat-abi=hard','-mfpu=neon','-mthumb','-std=gnu11','-O2',
         '-I'+str(source),'-I'+str(root/'port/vita/include'),'-c',str(root/'port/vita/graphics/texture_adapter.c'),'-o',str(out/'probe.o')]
r=subprocess.run(command,text=True,capture_output=True)
(out/'compile.log').write_text(r.stdout+r.stderr)
unresolved=None
if not r.returncode:
    unresolved=subprocess.check_output(['/home/birchwoodgod/vitasdk/bin/arm-vita-eabi-nm','-u',str(out/'probe.o')],text=True)
receipt=dict(command=command,exit_code=r.returncode,executed=False,
             headers={str(h):hashlib.sha256(h.read_bytes()).hexdigest() for h in headers},unresolved=unresolved,
             adapter_sha256=hashlib.sha256((root/'port/vita/graphics/texture_adapter.c').read_bytes()).hexdigest(),
             scope='bounded adapter compilation only; mip conversion and pixel correctness not hardware-tested')
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('compile exit',r.returncode,'unresolved:',repr(unresolved))
raise SystemExit(r.returncode)
