"""Audit a fresh engine compilation; a relocatable link is NOT an executable."""
from pathlib import Path
import json
import subprocess
import sys
import argparse
from engine_units import engine_units, PSEUDO_UNITS
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("receipt", type=Path)
parser.add_argument("--xita-source", required=True, type=Path)
parser.add_argument("--shader-dir", required=True, type=Path, help="Explicit generated shader set from the qualified Xita build")
args = parser.parse_args()

root = Path(__file__).resolve().parents[2]
out = root / 'build/vita'
receipt = args.receipt
entries = json.loads(receipt.read_text())
expected = set(engine_units(json.loads((root / 'config/config.json').read_text())['projects'][0]))
if {x['source'] for x in entries} != expected:
    raise SystemExit('Supply a complete --engine compile receipt')
objects = [out / (x['source'] + '.o') for x in entries if not x['exit_code']]
platform = sorted((root / 'port/vita/src').glob('*.c')) + [
    root / 'port/linux/src/msvc_wide.c',
    # Real out-of-line bodies for MSVC header inline calls seen through extern
    # prototypes. Do not accept unresolved weak references as zero addresses.
    root / 'port/linux/game/msvc_comdat.c',
]
subprocess.run([sys.executable, 'port/vita/embed_clear_shaders.py', '--xita-source', str(args.xita_source), '--shader-dir', str(args.shader_dir)], cwd=root, check=True)
subprocess.run([sys.executable, 'port/vita/embed_vertex_catalog.py', '--xita-source', str(args.xita_source), '--shader-dir', str(args.shader_dir)], cwd=root, check=True)
subprocess.run([sys.executable, 'port/vita/embed_fragment_catalog.py', '--shader-dir', str(args.shader_dir)], cwd=root, check=True)
subprocess.run([sys.executable, 'port/vita/compile_engine.py',
                *[str(x.relative_to(root)) for x in platform]], cwd=root, check=True)
objects += [out / (str(x.relative_to(root)) + '.o') for x in platform]
subprocess.run([sys.executable, 'port/vita/probe_xita_texture.py', '--xita-source', str(args.xita_source)], cwd=root, check=True)
objects.append(out / 'texture-reuse/probe.o')
sdk = Path('/home/birchwoodgod/vitasdk/bin')
cmd = [str(sdk / 'arm-vita-eabi-ld'), '-r', '-o', str(out / 'partial-engine.o'), *map(str, objects)]
run = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(out / 'partial-link.log').write_text(run.stdout)
summary = {'scope': 'relocatable link only; unresolved references permitted; no executable',
           'engine_receipt': str(receipt), 'excluded_pseudo_units': PSEUDO_UNITS, 'engine_compiled': sum(not x['exit_code'] for x in entries),
           'engine_failed': [x['source'] for x in entries if x['exit_code']],
           'object_count': len(objects), 'command': cmd, 'exit_code': run.returncode}
if not run.returncode:
    symbols = subprocess.check_output([str(sdk / 'arm-vita-eabi-nm'), '-u', str(out / 'partial-engine.o')], text=True)
    (out / 'partial-link-unresolved.txt').write_text(symbols)
    names = [line.split()[-1] for line in symbols.splitlines() if line.split()]
    summary['unresolved_count'] = len(names)
    summary['weak_unresolved_count'] = sum(line.split()[0] in ('w', 'v') for line in symbols.splitlines() if line.split())
(out / 'partial-link-receipt.json').write_text(json.dumps(summary, indent=2) + '\n')
print({k: v for k, v in summary.items() if k != 'command'})
raise SystemExit(run.returncode or bool(summary.get('weak_unresolved_count')))
