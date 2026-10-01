"""Map unresolved partial-link symbols to object users and known SDK providers."""
from pathlib import Path
import json,subprocess,collections
root=Path(__file__).resolve().parents[2];out=root/'build/vita'
r=json.loads((out/'partial-link-receipt.json').read_text())
if r['exit_code']: raise SystemExit('partial link failed')
nm='/home/birchwoodgod/vitasdk/bin/arm-vita-eabi-nm'
objects=r['command'][4:]
unresolved={line.split()[-1] for line in (out/'partial-link-unresolved.txt').read_text().splitlines() if line.strip()}
owners=collections.defaultdict(set)
raw=subprocess.check_output([nm,'-A','-u',*objects],text=True)
for line in raw.splitlines():
    fields=line.split()
    if fields and fields[-1] in unresolved:
        owner=line.split(':',1)[0]
        owners[fields[-1]].add(str(Path(owner).relative_to(out)))
libroot=Path('/home/birchwoodgod/vitasdk/arm-vita-eabi/lib')
libraries=['libc.a','libm.a','libpthread.a','libSceSysmem_stub.a','libSceGxm_stub.a',
           'libSceIofilemgr_stub.a','libSceKernelThreadMgr_stub.a','libSceLibKernel_stub.a',
           'libSceDisplay_stub.a','libSceRtc_stub.a']
libraries.append(subprocess.check_output(
    ['/home/birchwoodgod/vitasdk/bin/arm-vita-eabi-gcc','-mcpu=cortex-a9',
     '-mfloat-abi=hard','-mfpu=neon','-mthumb','-print-libgcc-file-name'],text=True).strip())
providers=collections.defaultdict(set)
for name in libraries:
    lib=libroot/name
    if not lib.exists(): continue
    raw=subprocess.check_output([nm,'-g','--defined-only',str(lib)],text=True,stderr=subprocess.DEVNULL)
    for line in raw.splitlines():
        fields=line.split()
        if len(fields)>=3 and fields[-1] in unresolved:
            providers[fields[-1]].add(name)
def category(name):
    if providers[name]: return 'known_library_provider'
    if name.startswith(('D3D','Direct3D','XG')): return 'xbox_graphics'
    if name.startswith(('DirectSound','IDirectSound','DSound')): return 'xbox_audio'
    if name.startswith('Bink'): return 'bink'
    return 'needs_owner_review'
entries=[dict(symbol=n,category=category(n),providers=sorted(providers[n]),
              objects=sorted(owners[n]),object_users=len(owners[n])) for n in sorted(unresolved)]
summary=dict(collections.Counter(e['category'] for e in entries))
result=dict(summary=summary,symbols=entries,source_receipt='partial-link-receipt.json',
            scope='static object references, NOT call counts/performance; archive exports do not prove final link or ABI compatibility')
(out/'link-dependencies.json').write_text(json.dumps(result,indent=2)+'\n')
print(summary)
print('most widely referenced unresolved non-library symbols:')
for e in sorted((e for e in entries if not e['providers']),key=lambda e:-e['object_users'])[:12]:
    print(e['symbol'],e['object_users'],e['category'])
