"""Validate and normalize typed partial plans; does not apply or enable them."""
import struct

PRESERVE = {'preserve_file_offset','preserve_pixel_offset','preserve_tag_id'}
CLEAR = {'clear_empty_block','clear_empty_data','clear_runtime_pointer'}

def normalize_plans(game, groups):
    fields = {}
    for group in groups:
        if group['errors']:
            raise ValueError('cannot normalize an incomplete group plan')
        for plan in group['plans']:
            for action in plan['actions']:
                at = action['field_offset']
                if not isinstance(at,int) or at < 0 or at & 3 or at > game.tag_size-4:
                    raise ValueError('invalid patch field span/alignment')
                expected = action['original']
                actual, = struct.unpack_from('<I',game.data,game.tag_offset+at)
                if expected != actual:
                    raise ValueError('plan original does not match source data')
                kind = action['action']
                target,size,code = 0,0,0
                if kind == 'rebase_tag_pointer':
                    target,size,code = action['target_offset'],action['span_bytes'],1
                    if target < 0 or size <= 0 or target >= game.tag_size or size > game.tag_size-target:
                        raise ValueError('invalid patch target span')
                    if expected != 0x803a6000+target:
                        raise ValueError('patch target does not match serialized pointer')
                elif kind in CLEAR:
                    code = 2
                elif kind not in PRESERVE:
                    raise ValueError('unknown relocation action')
                entry = (at,expected,target,size,code)
                if at in fields and fields[at] != entry:
                    raise ValueError('conflicting typed actions for one field')
                fields[at] = entry
    entries = [entry for at,entry in sorted(fields.items()) if entry[4]]
    return dict(entries=entries,entry_bytes=20,patch_count=len(entries),
                preserved_fields=sum(entry[4]==0 for entry in fields.values()),
                complete_map_relocation=False,loadable=False,
                scope='partial typed groups; five uint32 fields match halo_vita_relocation')
