"""Check native declaration storage against generated GXM stream descriptors."""
import re
import struct

def validate_raw_layout(name, declaration, layouts):
    formats = {0x40: ('U8N', 4, 4), 0x16: ('U8', 4, 4), 0x72: ('F32', 3, 12)}
    for count in range(1, 5):
        for kind, fmt, width in [(2, 'F32', 4), (1, 'S16N', 2), (5, 'S16', 2), (4, 'U8N', 1)]:
            formats[count * 16 + kind] = (fmt, count, width * count)
    offsets = [0] * 4
    expected = {}
    stream = 0
    for token, in struct.iter_unpack('<I', declaration):
        if token == 0xffffffff:
            break
        kind = token >> 29
        if kind == 1:
            stream = token & 15
            if stream >= 4:
                raise ValueError('Unsupported stream')
        elif kind == 2 and not token & 0x10000000:
            reg = token & 31
            fmt, count, size = formats[(token >> 16) & 255]
            if reg >= 16 or reg in expected:
                raise ValueError('Invalid/repeated register')
            expected[reg] = (stream, offsets[stream], fmt, count)
            offsets[stream] += size
        else:
            raise ValueError('Unverified native declaration token')
    attrs = re.search(r'static const xv_attr_desc_t xv_attrs_' + name + r'\[\]\s*=\s*\{(.*?)\};', layouts, re.S)[1]
    seen = set()
    for stream, offset, fmt, count, reg in re.findall(r'\{\s*"[^"]+",\s*(\w+),\s*(\d+),\s*SCE_GXM_ATTRIBUTE_FORMAT_(\w+),\s*(\d+),\s*(\d+)\s*\}', attrs):
        reg = int(reg)
        if stream == 'XV_CONST_STREAM':
            if reg in expected or fmt != 'F32' or int(count) != 4:
                raise ValueError('Invalid persistent attribute')
            continue
        actual = (int(stream), int(offset), fmt, int(count))
        if expected.get(reg) != actual or reg in seen:
            raise ValueError(f'{name}: raw attribute mismatch at v{reg}')
        seen.add(reg)
    if seen != expected.keys():
        raise ValueError('Missing native attribute')
    desc = re.search(r'static const xv_vs_desc_t xv_vs_' + name + r'\s*=\s*\{(.*?)\};', layouts, re.S)[1]
    strides = re.search(r'"[^"]+",\s*(\d+),\s*\{([^}]+)\}', desc)
    if list(map(int, strides[2].split(','))) != offsets:
        raise ValueError(f'{name}: raw stride mismatch')
    return {'raw_layout_verified': True, 'strides': offsets, 'attributes': len(expected)}
