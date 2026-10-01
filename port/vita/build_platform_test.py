"""Build only. Never deploy or execute the platform test automatically."""
from pathlib import Path
import json
import hashlib
import subprocess
import sys
import zipfile
import argparse

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--xita-source", required=True, type=Path)
parser.add_argument("--shader-dir", required=True, type=Path, help="Explicit generated shader set from the qualified Xita build")
parser.add_argument("--private-texture-fixture", type=Path)
args = parser.parse_args()

root = Path(__file__).resolve().parents[2]
out = root / 'build/vita'
from embed_texture_fixture import emit
out.mkdir(parents=True, exist_ok=True)
fixture_metadata = emit(out / 'texture_fixture.h', args.private_texture_fixture)
names = ['fragment_texture', 'texture_state', 'fragment_uniforms', 'fragment_program', 'pixel_state', 'attributes', 'vertex_bind', 'd3d_vertex_shader', 'vertex_program', 'constants', 'index_snapshot', 'indices', 'vertex_buffer', 'buffer', 'streams', 'topology', 'd3d_state', 'd3d_render', 'd3d_device', 'd3d_backbuffer', 'logical_scene', 'logical_targets', 'd3d_resource', 'd3d_surface', 'd3d_texture', 'd3d_view', 'allocation', 'float_control', 'backtrace', 'file_timestamps', 'newlib_descriptor', 'file_time', 'texture_resource', 'texture_draw', 'graphics_device', 'clear_draw', 'shader_patcher', 'graphics_present', 'graphics_surfaces', 'graphics_context', 'crt_format', 'crt_strings', 'texture_upload', 'texture_layout', 'arenas', 'directories', 'tag_relocation', 'last_error', 'handles', 'files', 'file_roots', 'file_handle',
         'file_metadata', 'file_transfer', 'file_position', 'file_sync',
         'file_async', 'completion', 'sleep', 'events', 'profile_clock', 'kernel_clock', 'wait', 'mutex', 'thread', 'thread_native']
sources = ['port/vita/platform_test.c', 'source/cache/physical_memory_map.c'] + ['port/vita/src/' + n + '.c' for n in names]
subprocess.run([sys.executable, 'port/vita/embed_clear_shaders.py', '--xita-source', str(args.xita_source), '--shader-dir', str(args.shader_dir)], cwd=root, check=True)
subprocess.run([sys.executable, 'port/vita/embed_vertex_catalog.py', '--xita-source', str(args.xita_source), '--shader-dir', str(args.shader_dir)], cwd=root, check=True)
subprocess.run([sys.executable, 'port/vita/embed_fragment_catalog.py', '--shader-dir', str(args.shader_dir)], cwd=root, check=True)
subprocess.run([sys.executable, 'port/vita/compile_engine.py', *sources], cwd=root, check=True)
subprocess.run([sys.executable, 'port/vita/probe_xita_texture.py', '--xita-source', str(args.xita_source)], cwd=root, check=True)
cmd = ['/home/birchwoodgod/vitasdk/bin/arm-vita-eabi-gcc', '-mcpu=cortex-a9',
       '-mfloat-abi=hard', '-mfpu=neon', '-mthumb', '-Wl,--no-undefined',
       '-Wl,-T,port/vita/platform_test.ld',
       '-o', str(out / 'platform-test.elf')]
cmd += [str(out / (s + '.o')) for s in sources]
cmd += [str(out / 'texture-reuse/probe.o')]
cmd += ['-lpthread', '-lSceRtc_stub', '-lSceDisplay_stub', '-lSceSysmem_stub', '-lSceGxm_stub', '-lSceIofilemgr_stub', '-lSceKernelThreadMgr_stub', '-lSceLibKernel_stub']
result = subprocess.run(cmd, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(out / 'platform-test-link.log').write_text(result.stdout)
(out / 'platform-test-link.json').write_text(json.dumps({
    'command': cmd, 'exit_code': result.returncode, 'executed': False,
    'note': 'ABI warnings remain visible in link log; not hardware-qualified'
}, indent=2) + '\n')
print('link exit', result.returncode, '; see build/vita/platform-test-link.log')
if result.returncode:
    raise SystemExit(result.returncode)
sdk = Path('/home/birchwoodgod/vitasdk/bin')
undefined = subprocess.check_output([str(sdk / 'arm-vita-eabi-nm'), '-u',
                                    str(out / 'platform-test.elf')], text=True)
if undefined.strip():
    raise SystemExit('Unresolved symbols remain (including weak symbols): ' + undefined)
commands = [
    [str(sdk / 'vita-elf-create'), '-v', str(out / 'platform-test.elf'), str(out / 'platform-test.velf')],
    [str(sdk / 'vita-make-fself'), '-s', str(out / 'platform-test.velf'), str(out / 'platform-test.self')],
    [str(sdk / 'vita-mksfoex'), '-s', 'TITLE_ID=XITATST01', '-s', 'APP_VER=01.77',
     'Xita Platform Test', str(out / 'platform-test.sfo')],
]
for index, command in enumerate(commands):
    run = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / ('platform-package-%d.log' % index)).write_text(run.stdout)
    if run.returncode:
        raise SystemExit(run.returncode)
package = out / ('xita-platform-test-private.vpk' if fixture_metadata else 'xita-platform-test.vpk')
with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.write(out / 'platform-test.self', 'eboot.bin')
    archive.write(out / 'platform-test.sfo', 'sce_sys/param.sfo')
(out / 'platform-package.json').write_text(json.dumps({
    'commands': commands, 'title_id': 'XITATST01', 'executed': False,
    'package': str(package), 'sha256': hashlib.sha256(package.read_bytes()).hexdigest(),
    'bytes': package.stat().st_size,
    'private_owned_texture': fixture_metadata,
    'note': 'Separate test app; do not submit to Xita game updater'
}, indent=2) + '\n')
print('packaged', package)
