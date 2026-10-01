"""Linker reachability from actual native main; unresolved imports permitted.
This is a relocatable static audit, NOT an executable or runtime validation.
"""
from pathlib import Path
import subprocess,json
from elf_references import allocated_imports
root=Path(__file__).resolve().parents[2];out=root/'build/vita'
receipt=json.loads((out/'partial-link-receipt.json').read_text())
if receipt['exit_code'] or receipt['engine_failed']:
    raise SystemExit('Requires successful complete engine aggregate')
engine=json.loads(Path(receipt['engine_receipt']).read_text())
if not all('-ffunction-sections' in row['command'] and '-fdata-sections' in row['command'] for row in engine):
    raise SystemExit('Rebuild engine with per-function/data sections first')
sdk=Path('/home/birchwoodgod/vitasdk/bin')
command=[str(sdk/'arm-vita-eabi-ld'),'-r','--gc-sections','--undefined=main',
    '-Map='+str(out/'startup-link.map'),'-o',str(out/'startup-engine.o'),str(out/'partial-engine.o')]
r=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'startup-link.log').write_text(r.stdout)
if r.returncode:raise SystemExit(r.stdout)
raw=subprocess.check_output([str(sdk/'arm-vita-eabi-nm'),'-u',str(out/'startup-engine.o')],text=True)
(out/'startup-link-unresolved.txt').write_text(raw)
symbol_table_undefined={line.split()[-1] for line in raw.splitlines() if line.split()}
remaining,definitions,loaded_bytes=allocated_imports(out/'startup-engine.o')
if not {'main','main_loop','shell_initialize'}<=definitions:
    raise SystemExit('Required startup roots absent')
all_symbols={line.split()[-1] for line in (out/'partial-link-unresolved.txt').read_text().splitlines() if line.split()}
result=dict(scope='Static native-main rooted relocatable audit; no executable; unresolved imports allowed',
    command=command,root='source/shell/shell_xbox.c:main',engine_receipt=receipt['engine_receipt'],
    unresolved=len(remaining),aggregate_unresolved=len(all_symbols),
    removed_unreachable=sorted(all_symbols-remaining),retained_unresolved=sorted(remaining),
    runtime_executed=False,loaded_section_bytes=loaded_bytes,
    symbol_table_undefined=len(symbol_table_undefined),
    reference_method='undefined symbols referenced by relocations in SHF_ALLOC sections; excludes debug sections and orphan undefined names')
(out/'startup-link-receipt.json').write_text(json.dumps(result,indent=2)+'\n')
print({k:v for k,v in result.items() if k not in ('command','retained_unresolved','removed_unreachable')})
print('unreachable imports:',', '.join(result['removed_unreachable']))
