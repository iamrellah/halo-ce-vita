"""Read-only retail sound layout audit; ABI checked by sound_layout_check.c."""
from collections import Counter
import struct

def audit_sounds(game):
    base = 0x803a6000
    counts, compression, data_states = Counter(), Counter(), Counter()
    errors = []
    references = Counter()
    tags_by_id = {t.tag_id:t for t in game.tags if hasattr(t,"tag_id")}
    def span(address, size):
        relative = address-base
        if size < 0 or relative < 0 or relative > game.tag_size or size > game.tag_size-relative:
            raise ValueError('sound span outside tag region')
        return game.tag_offset+relative
    def block(record, displacement, stride):
        count,address,definition = struct.unpack_from('<iII',game.data,record+displacement)
        if count < 0 or count > game.tag_size//stride:
            raise ValueError('invalid sound block count')
        if definition: data_states['nonnull_block_definition'] += 1
        return (count,span(address,count*stride)) if count else (0,0)
    for tag in game.tags:
        if tag.groups[0] != 'snd!': continue
        counts['groups'] += 1
        try:
            root = span(tag.data_addr,164)
            group,name,length,index = struct.unpack_from('<IIiI',game.data,root+112)
            if length < 0:
                raise ValueError('negative promotion name length')
            if name:
                name_offset = span(name,1)
                # Cache references may retain a name pointer with cleared length.
                # Bound the search to the tag region; never infer an empty name.
                end = game.data.find(b'\0',name_offset,game.tag_offset+game.tag_size)
                if end < 0 or (length and end-name_offset != length):
                    raise ValueError('invalid promotion name termination/length')
                if not length and end > name_offset:
                    references['name_length_cleared'] += 1
                references['resident_name'] += 1
            elif length:
                raise ValueError('promotion name length without address')
            if index != 0xffffffff:
                target = tags_by_id.get(index)
                if target is None or target.groups[0] != 'snd!' or group != 0x736e6421:
                    raise ValueError('invalid promotion sound tag ID/group')
                references['resolved_sound'] += 1
            else:
                references['null_sound'] += 1
            ranges,range_base = block(root,152,72)
            counts['pitch_ranges'] += ranges
            for r in range(ranges):
                count,permutations = block(range_base+r*72,60,124)
                counts['permutations'] += count
                for i in range(count):
                    record = permutations+i*124
                    codec,next_index = struct.unpack_from('<hh',game.data,record+40)
                    compression[codec] += 1
                    if next_index != -1 and not 0 <= next_index < count:
                        raise ValueError('next permutation outside range')
                    for field,label in ((64,'samples'),(84,'mouth'),(104,'subtitle')):
                        size,flags,file_offset,address,definition = struct.unpack_from('<iiIII',game.data,record+field)
                        if size < 0: raise ValueError('negative sound tag data size')
                        if definition: data_states[label+'_definition'] += 1
                        if address:
                            span(address,size)
                            data_states[label+'_resident'] += 1
                        elif size:
                            # File offset interpretation still needs cache-reader verification.
                            valid = file_offset <= len(game.data) and size <= len(game.data)-file_offset
                            data_states[label+('_file_span_bounded' if valid else '_file_span_outside')] += 1
                        else:
                            data_states[label+'_empty'] += 1
            counts['bounded_groups'] += 1
        except (ValueError,struct.error) as error:
            errors.append(dict(tag_index=tag.index,error=str(error)))
    return dict(counts=dict(counts),compression=dict(compression),data_states=dict(data_states),
                errors=errors,promotion_references=dict(references),native_compatibility_proven=False,
                scope='typed spans and permutation links only; codecs, flags and resource semantics unverified')
