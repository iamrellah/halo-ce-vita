"""Read-only bitmap layout probe; offsets checked by bitmap_layout_check.c."""
from collections import Counter
import struct

# Offline interpretation of source/bitmaps/bitmaps.c, not game execution.
BITS_PER_PIXEL = (8,8,8,16,0,0,16,0,16,16,32,32,0,0,4,8,8,8)

def predicted_pixel_size(width, height, depth, kind, fmt, flags, mips):
    if min(width,height,depth) <= 0 or kind not in (0,1,2):
        raise ValueError('invalid bitmap dimensions/type')
    if not 0 <= fmt < len(BITS_PER_PIXEL) or not BITS_PER_PIXEL[fmt]:
        raise ValueError('unsupported bitmap format')
    if not 0 <= mips <= 15:
        raise ValueError('invalid mip count')
    if bool(flags & 2) != (14 <= fmt <= 16):
        raise ValueError('compressed flag/format disagreement')
    pixels = 0
    for level in range(mips+1):
        w,h,d = (max(n >> level,1) for n in (width,height,depth))
        if flags & 2:
            w,h = (w+3)&~3, (h+3)&~3
        pixels += w*h*d*(6 if kind == 2 else 1)
    size = pixels*BITS_PER_PIXEL[fmt]//8
    if size > 0x7fffffff:
        raise ValueError('pixel size exceeds signed engine range')
    return size

def predicted_hardware_size(width, height, depth, kind, fmt, flags, mips):
    # source/rasterizer/rasterizer_swizzle.c: Xbox resource layout, unlike
    # the generic CPU bitmap chain. This does not describe Vita GXM layout.
    predicted_pixel_size(width,height,depth,kind,fmt,flags,mips)
    maximum = 0
    if flags & 1 and not flags & 16:
        dimension = max(width//4,height//4,depth) if flags & 2 else max(width,height,depth)
        maximum = min(mips,dimension.bit_length()-1)
    faces = 6 if kind == 2 else 1
    size = predicted_pixel_size(width,height,depth,kind,fmt,flags,maximum)//faces
    if flags & 16:
        if flags & 2:
            raise ValueError('linear compressed bitmap')
        row = width*BITS_PER_PIXEL[fmt]//8
        size += height*((-row)&63)
    return ((size+127)&~127)*faces

def audit_bitmaps(game):
    base, limit = 0x803a6000, game.tag_size
    totals = Counter()
    formats = Counter()
    nonnull = Counter()
    errors = []
    addresses = Counter()
    payloads = Counter()
    mip_sizes = Counter()
    size_examples = []
    def offset(address, length):
        relative = address-base
        if length < 0 or relative < 0 or relative > limit or length > limit-relative:
            raise ValueError('typed span outside loaded tag region')
        return game.tag_offset+relative
    def block(record, displacement, stride):
        count, address, definition = struct.unpack_from('<iII', game.data, record+displacement)
        if count < 0 or count > limit//stride:
            raise ValueError('invalid typed block count')
        if definition:
            nonnull['block_definition'] += 1
        if not count:
            if address: nonnull['empty_block_address'] += 1
            return 0, 0
        return count, offset(address, count*stride)
    for tag in game.tags:
        if tag.groups[0] != 'bitm': continue
        totals['groups'] += 1
        try:
            root = offset(tag.data_addr,108)
            for displacement, label in ((28,'import'),(48,'pixels')):
                size, flags, file_offset, address, definition = struct.unpack_from('<iiIII',game.data,root+displacement)
                if size < 0: raise ValueError('negative tag_data size')
                if address: nonnull[label+'_data_address'] += 1
                if definition: nonnull[label+'_data_definition'] += 1
            pixel_file_offset, = struct.unpack_from("<I", game.data, root+56)
            count, sequences = block(root,84,64)
            totals['sequences'] += count
            for index in range(count):
                sprites, unused = block(sequences+index*64,52,32)
                totals['sprites'] += sprites
            count, bitmaps = block(root,96,48)
            totals['bitmap_records'] += count
            for index in range(count):
                record=bitmaps+index*48
                signature, = struct.unpack_from('<I',game.data,record)
                if signature != 0x6269746d: raise ValueError('bitmap record signature mismatch')
                fmt, = struct.unpack_from('<h',game.data,record+12)
                formats[fmt] += 1
                hardware, address = struct.unpack_from('<II',game.data,record+40)
                if hardware: nonnull['hardware_format'] += 1
                if address: nonnull['serialized_base_address'] += 1
                addresses[address] += 1
                pixels_offset, pixels_size = struct.unpack_from('<ii', game.data, record+24)
                width,height,depth,kind,fmt,flags = struct.unpack_from('<hhhhhH',game.data,record+4)
                mips, = struct.unpack_from('<h',game.data,record+20)
                predicted = predicted_pixel_size(width,height,depth,kind,fmt,flags,mips)
                relation = 'equal' if predicted == pixels_size else ('predicted_larger' if predicted > pixels_size else 'predicted_smaller')
                mip_sizes[relation] += 1
                hardware_size = predicted_hardware_size(width,height,depth,kind,fmt,flags,mips)
                mip_sizes['hardware_equal' if hardware_size == pixels_size else 'hardware_mismatch'] += 1
                faces = 6 if kind == 2 else 1
                aligned = ((predicted//faces + 127)&~127)*faces
                mip_sizes['per_face_128_aligned_equal' if aligned == pixels_size else 'per_face_128_aligned_mismatch'] += 1
                if relation != 'equal' and (relation == 'predicted_larger' or len(size_examples) < 12):
                    size_examples.append(dict(tag=tag.index,bitmap=index,format=fmt,type=kind,
                        dimensions=[width,height,depth],mips=mips,predicted=predicted,serialized=pixels_size))
                start = pixel_file_offset + pixels_offset
                # Candidate interpretation used by texture_cache_bitmap_new.
                # Bounds alone do not prove texture format, mip sizes or contents.
                valid = (pixels_offset >= 0 and pixels_size >= 0 and
                         0 <= start <= len(game.data) and pixels_size <= len(game.data)-start)
                payloads['bounded_candidate' if valid else 'invalid_candidate'] += 1
            totals['bounded_groups'] += 1
        except (ValueError,struct.error) as error:
            errors.append({'tag_index':tag.index,'error':str(error)})
    return {'counts':dict(totals),'formats':dict(sorted(formats.items())),
            'nonnull_runtime_or_metadata_fields':dict(nonnull),'errors':errors,
            'serialized_base_values': {hex(k):v for k,v in sorted(addresses.items())},
            'payload_span_candidates':dict(payloads),
            'mip_size_comparison':dict(mip_sizes), 'size_mismatch_examples':size_examples,
            'base_address_policy':'unclassified serialized value; upstream cache initialization clears it, do not blindly relocate',
            'native_compatibility_proven':False,
            'scope':'outer blocks and bitmap record signatures only; pixel payload/semantics and relocation unverified'}
