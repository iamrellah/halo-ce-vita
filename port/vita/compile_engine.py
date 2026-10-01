"""Cross-compile engine units only; never execute them on the build host."""
from pathlib import Path
import json, subprocess, sys
from datetime import datetime, timezone
from engine_units import engine_units, PSEUDO_UNITS
root=Path(__file__).resolve().parents[2]
out=root/'build/vita';out.mkdir(parents=True,exist_ok=True)
project=json.loads((root/'config/config.json').read_text())['projects'][0]
config=project['options']
flags=['clang','--target=arm-vita-eabi','--sysroot=/home/birchwoodgod/vitasdk/arm-vita-eabi','-mcpu=cortex-a9','-mfloat-abi=hard','-mfpu=neon','-mthumb','-std=gnu89','-fms-extensions','-fshort-wchar','-fno-builtin','-funwind-tables','-ffunction-sections','-fdata-sections','-fsigned-char','-fcommon','-fno-strict-aliasing','-fwrapv','-ffp-contract=off','-O2','-g','-Wno-error=implicit-function-declaration','-Wno-error=int-conversion','-include','port/vita/include/halo_vita_prefix.h','-Iport/vita/include','-idirafter','port/include/xdk']
subprocess.run([sys.executable,'tools/linux_msvc_semantics.py','--output',str(out/'halo_msvc_semantics.h'),'--all-inlines','--tags','source','--inlines','source','--inlines','port/include/xdk'],cwd=root,check=True)
flags += ['-Dxbox','-include',str(out/'halo_msvc_semantics.h')]
flags += ['-I'+d for d in config['include_dirs'] if d!='xbox/include']
results=[]
stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
names=sys.argv[1:] or ['source/math/real_math.c']
if names==['--engine']:
 names=engine_units(project)
 (out/'engine-excluded-pseudo-units.json').write_text(json.dumps(PSEUDO_UNITS,indent=2)+'\n')
for name in names:
 obj=out/(name+'.o');obj.parent.mkdir(parents=True,exist_ok=True)
 extra=[]
 if name=='port/vita/src/newlib_descriptor.c':
  subprocess.run([sys.executable,'port/vita/verify_newlib_descriptor.py'],cwd=root,check=True)
  extra=['-fshort-enums']
 cmd=flags+extra+['-c',name,'-o',str(obj)]
 run=subprocess.run(cmd,cwd=root,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 log=out/(name+'.log');log.write_text(run.stdout)
 results.append({'source':name,'command':cmd,'exit_code':run.returncode,'log':str(log)})
 if run.returncode: obj.unlink(missing_ok=True)
 (out/('compile-receipt-'+stamp+'.json')).write_text(json.dumps(results,indent=2)+'\n')
 print(name,'exit',run.returncode,flush=True)
 if run.returncode: print('\n'.join(x for x in run.stdout.splitlines() if 'error:' in x)[:2000])
(out/('compile-receipt-'+stamp+'.json')).write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(any(r['exit_code'] for r in results))
