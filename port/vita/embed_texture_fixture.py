"""Generate private optional texture-test input; no fixture in default builds."""
import hashlib
import json
from pathlib import Path

def emit(output, fixture=None):
    output = Path(output)
    if fixture is None:
        output.write_text('#define HALO_PRIVATE_TEXTURE_FIXTURE 0\n')
        return None
    fixture = Path(fixture)
    meta = json.loads((fixture/'fixture.json').read_text())
    data = (fixture/'pixels.bin').read_bytes()
    if meta.get('private_owned_asset') is not True or meta.get('build') != '01.10.12.2276':
        raise ValueError('not a private retail fixture')
    if len(data) != meta['bytes'] or hashlib.sha256(data).hexdigest() != meta['payload_sha256']:
        raise ValueError('fixture payload integrity mismatch')
    from bitmap_inspection import predicted_hardware_size
    fields = [meta[k] for k in ('width','height','depth','type','format','flags','mip_count')]
    if any(type(v) is not int for v in fields): raise ValueError('non-integer layout')
    w,h,d,kind,fmt,flags,mips = fields
    if d != 1 or kind != 0 or fmt not in (14,15,16) or not (4<=w<=256 and 4<=h<=256) or w&(w-1) or h&(h-1):
        raise ValueError('unsupported fixture layout')
    if predicted_hardware_size(*fields) != len(data): raise ValueError('fixture layout size mismatch')
    output.write_text('#define HALO_PRIVATE_TEXTURE_FIXTURE 1\n'
        'static const struct halo_vita_bitmap_layout private_layout = {'+
        ','.join(map(str,fields))+'};\nstatic const unsigned char private_pixels[] = {\n'+
        '\n'.join(','.join(str(v) for v in data[i:i+32])+',' for i in range(0,len(data),32))+'\n};\n')
    return meta
