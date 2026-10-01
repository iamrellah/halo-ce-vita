"""Read-only cache header preflight, not a map validator or game runtime."""
import argparse
import json
from pathlib import Path
import struct

EXPECTED_BUILD = '01.01.14.2342'

def inspect(path):
    path = Path(path)
    with path.open('rb') as stream:
        header = stream.read(0x800)
    result = {'path': str(path), 'physical_bytes': path.stat().st_size,
              'expected_build': EXPECTED_BUILD, 'issues': [],
              'scope': 'header only; payload, compression, pointers and checksum unverified'}
    if len(header) != 0x800:
        result['issues'].append('truncated 2048-byte header')
        result['header_compatible'] = False
        return result
    signature, version, length = struct.unpack_from('<Iii', header)
    offset, size = struct.unpack_from('<ii', header, 0x10)
    footer, = struct.unpack_from('<I', header, 0x7fc)
    for key, start in [('name', 0x20), ('build', 0x40)]:
        raw = header[start:start+32]
        if b'\0' not in raw:
            result['issues'].append(key + ' is not terminated')
        try:
            result[key] = raw.split(b'\0', 1)[0].decode('ascii')
        except UnicodeDecodeError:
            result[key] = None
            result['issues'].append(key + ' is not ASCII')
    result.update(version=version, declared_bytes=length, tag_offset=offset, tag_bytes=size)
    if signature != 0x68656164 or footer != 0x666f6f74:
        result['issues'].append('header/footer signature mismatch')
    if version != 5:
        result['issues'].append('requires Xbox cache version 5')
    if result['build'] != EXPECTED_BUILD:
        result['issues'].append('different engine build; do not bypass this mismatch')
    if length < 0x800 or length > 0x11600000:
        result['issues'].append('declared length outside engine limits')
    if offset < 0x800 or size < 36 or size > 0x1600000 or offset + size > length:
        result['issues'].append('invalid tag region for declared uncompressed file')
    # Xbox distribution maps can be compressed. Do not interpret the declared
    # uncompressed offset as a position in a shorter compressed source file.
    result['physical_matches_declared'] = length == result['physical_bytes']
    result['header_compatible'] = not result['issues']
    return result

def inspect_tag_index(path, header):
    """Inventory known outer pointers only; never mutate or guess nested fields."""
    if not header['header_compatible']:
        raise ValueError('tag inspection requires a compatible header')
    if not header['physical_matches_declared']:
        raise ValueError('requires an unpacked cache; compressed offsets are not file offsets')
    base = 0x803a6000
    size = header['tag_bytes']
    with Path(path).open('rb') as stream:
        stream.seek(header['tag_offset'])
        tags = stream.read(size)
    if len(tags) != size:
        raise ValueError('truncated tag region')
    table, = struct.unpack_from('<I', tags)
    count, = struct.unpack_from('<i', tags, 12)
    signature, = struct.unpack_from('<I', tags, 32)
    if signature != 0x74616773:
        raise ValueError('invalid tags signature')
    offset = table - base
    if count < 0 or offset < 36 or offset > size or count > (size-offset)//32:
        raise ValueError('tag directory outside loaded tag region')
    fields = [{'offset': 0, 'target': table, 'kind': 'tag_instances'}]
    out_of_region = []
    for index in range(count):
        record = offset + index*32
        for displacement, kind in ((16, 'name'), (20, 'base_address')):
            value, = struct.unpack_from('<I', tags, record+displacement)
            if not value:
                continue
            field = {'offset': record+displacement, 'target': value,
                     'kind': kind, 'tag_index': index}
            if base <= value < base+size:
                fields.append(field)
            else:
                out_of_region.append(field)
    return {'tag_count': count, 'serialized_base': base,
            'known_pointer_fields': fields, 'out_of_region': out_of_region,
            'complete_relocation_plan': False,
            'remaining': 'nested blocks/data/references, BSP ranges, GPU resources and save pointers'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('maps', nargs='+', type=Path)
    parser.add_argument('--tag-index', action='store_true', help='inspect known pointers in an unpacked compatible map')
    args = parser.parse_args()
    results = []
    for path in args.maps:
        try:
            item = inspect(path)
            if args.tag_index:
                try:
                    item['tag_index'] = inspect_tag_index(path, item)
                except ValueError as error:
                    item['issues'].append(str(error))
                    item['tag_inspection_failed'] = True
            results.append(item)
        except OSError as error:
            results.append({'path': str(path), 'header_compatible': False, 'issues': [str(error)]})
    print(json.dumps(results, indent=2))
    return 0 if all(r['header_compatible'] and not r.get('tag_inspection_failed') for r in results) else 1

if __name__ == '__main__':
    raise SystemExit(main())
