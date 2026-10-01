"""Extract one bounded retail bitmap for PRIVATE native texture bring-up.
Not an asset converter or proof that the native engine supports retail maps.
Outputs never belong in a public package or source commit.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
from bitmap_inspection import predicted_hardware_size
from check_assets import inspect


def extract(game, tag_index, bitmap_index):
    if len(game.data) != game.file_size:
        raise ValueError('inflated size mismatch')
    if not 0 <= game.tag_offset <= game.tag_offset + game.tag_size <= len(game.data):
        raise ValueError('invalid tag span')
    def offset(address, size):
        relative = address - 0x803a6000
        if relative < 0 or size < 0 or relative + size > game.tag_size:
            raise ValueError('typed span outside tag region')
        return game.tag_offset + relative
    tags = [t for t in game.tags if t.index == tag_index]
    if len(tags) != 1 or tags[0].groups[0] != 'bitm' or tags[0].external:
        raise ValueError('requires a local bitmap tag')
    tag = tags[0]
    root = offset(tag.data_addr, 108)
    count, address, definition = struct.unpack_from('<iII', game.data, root + 96)
    if not 0 <= bitmap_index < count:
        raise ValueError('bitmap index outside block')
    record = offset(address, count * 48) + bitmap_index * 48
    if struct.unpack_from('<I', game.data, record)[0] != 0x6269746d:
        raise ValueError('invalid bitmap signature')
    width, height, depth, kind, fmt, flags = struct.unpack_from('<hhhhhH', game.data, record + 4)
    mips = struct.unpack_from('<h', game.data, record + 20)[0]
    displacement, size = struct.unpack_from('<ii', game.data, record + 24)
    # First fixture path deliberately supports only small 2D DXT chains.
    if kind != 0 or depth != 1 or fmt not in (14, 15, 16):
        raise ValueError('fixture requires a 2D DXT bitmap')
    if not (4 <= width <= 256 and 4 <= height <= 256) or width & (width-1) or height & (height-1):
        raise ValueError('fixture requires power-of-two dimensions 4..256')
    if predicted_hardware_size(width, height, depth, kind, fmt, flags, mips) != size:
        raise ValueError('serialized chain size disagrees with hardware layout')
    start = struct.unpack_from('<I', game.data, root + 56)[0] + displacement
    if displacement < 0 or size <= 0 or start < 2048 or start + size > game.tag_offset:
        raise ValueError('pixel chain outside pre-tag asset region')
    payload = game.data[start:start + size]
    return dict(tag_index=tag_index, bitmap_index=bitmap_index, name=tag.name,
                width=width, height=height, depth=depth, type=kind, format=fmt,
                flags=flags, mip_count=mips, inflated_offset=start, bytes=size,
                payload_sha256=hashlib.sha256(payload).hexdigest(),
                native_engine_compatible=False, hardware_sampling_verified=False), payload


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('map', type=Path)
    p.add_argument('--xita-source', required=True, type=Path)
    p.add_argument('--tag', required=True, type=int)
    p.add_argument('--bitmap', type=int, default=0)
    p.add_argument('--output-dir', required=True, type=Path)
    args = p.parse_args()
    header = inspect(args.map)
    if header.get('build') != '01.10.12.2276' or header['issues'] != ['different engine build; do not bypass this mismatch']:
        p.error('requires a structurally valid retail 2276 map')
    reader = args.xita_source / 'recompiler/halo_map.py'
    spec = importlib.util.spec_from_file_location('fixture_map_reader', reader)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    game = module.HaloMap(str(args.map), cache_dir=None)
    metadata, payload = extract(game, args.tag, args.bitmap)
    metadata.update(map_sha256=hashlib.sha256(args.map.read_bytes()).hexdigest(),
                    reader_sha256=hashlib.sha256(reader.read_bytes()).hexdigest(),
                    build=game.build, private_owned_asset=True)
    args.output_dir.mkdir(parents=True, exist_ok=False)
    (args.output_dir / 'pixels.bin').write_bytes(payload)
    (args.output_dir / 'fixture.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print(json.dumps(metadata))


if __name__ == '__main__':
    main()
