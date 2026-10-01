"""Read-only retail-map inventory using Xita's existing offline cache reader.
This does not enable retail maps in the native engine or execute the game.
"""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
from check_assets import inspect
from bitmap_inspection import audit_bitmaps
from bitmap_relocation import plan_bitmaps
from sound_inspection import audit_sounds
from sound_relocation import plan_sounds
from relocation_plan import normalize_plans

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('map', type=Path)
    parser.add_argument('--xita-source', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--sound-layout', action='store_true')
    parser.add_argument('--sound-relocation', action='store_true')
    parser.add_argument('--bitmap-layout', action='store_true')
    parser.add_argument('--bitmap-relocation', action='store_true')
    args = parser.parse_args()
    header = inspect(args.map)
    if header.get('build') != '01.10.12.2276' or header['issues'] != ['different engine build; do not bypass this mismatch']:
        raise SystemExit('Requires a structurally valid retail 2276 header for this scoped audit')
    reader = args.xita_source / 'recompiler/halo_map.py'
    spec = importlib.util.spec_from_file_location('halo_map_inventory_reader', reader)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    game = module.HaloMap(str(args.map), cache_dir=None)
    if len(game.data) != game.file_size:
        raise SystemExit('Inflated size mismatch')
    if not 0 <= game.tag_offset <= game.tag_offset + game.tag_size <= len(game.data):
        raise SystemExit('Invalid tag span')
    table = game.tag_array_addr - module.TAG_BASE
    if not 36 <= table <= game.tag_size or not 0 <= game.tag_count <= (game.tag_size-table)//32:
        raise SystemExit('Invalid tag directory')
    groups = Counter(t.groups[0] for t in game.tags)
    outside = [t.index for t in game.tags if t.data_addr and not module.TAG_BASE <= t.data_addr < module.TAG_BASE+game.tag_size]
    bsps = game.structure_bsps()
    for bsp in bsps:
        if not 0 <= bsp['file_offset'] <= bsp['file_offset']+bsp['size'] <= len(game.data):
            raise SystemExit('BSP outside inflated map')
    digest = hashlib.sha256()
    with args.map.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            digest.update(chunk)
    result = {'map': str(args.map), 'build': game.build, 'map_sha256': digest.hexdigest(),
              'reader': str(reader), 'reader_sha256': hashlib.sha256(reader.read_bytes()).hexdigest(),
              'inflated_bytes': len(game.data), 'tag_count': game.tag_count,
              'groups': dict(sorted(groups.items())), 'out_of_tag_region_indices': outside,
              'bsps': [{k: b[k] for k in ('index','file_offset','size','load_address')} for b in bsps],
              'native_engine_compatible': False,
              'scope': 'retail outer structures only; nested pointers/layouts not validated'}
    if args.sound_relocation:
        result['sound_relocation'] = plan_sounds(game)
    if args.sound_layout:
        result['sound_layout'] = audit_sounds(game)
    if args.bitmap_layout:
        result['bitmap_layout'] = audit_bitmaps(game)
    if args.bitmap_relocation:
        result['bitmap_relocation'] = plan_bitmaps(game)
    if args.sound_relocation and args.bitmap_relocation:
        result['partial_relocation'] = normalize_plans(game,[result['sound_relocation'],result['bitmap_relocation']])
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({'tags': game.tag_count, 'groups':len(groups), 'bsps':len(bsps),
                      'outside_tag_region':len(outside), 'report':str(args.output)}))

if __name__ == '__main__':
    main()
