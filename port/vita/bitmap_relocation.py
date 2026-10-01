"""Typed, read-only bitmap relocation plan. Never rewrites a map or enables it."""
import struct

BASE = 0x803a6000

def plan_bitmap(game, tag):
    actions = []
    def span(address, size):
        relative = address-BASE
        if size < 0 or relative < 0 or relative > game.tag_size or size > game.tag_size-relative:
            raise ValueError('bitmap relocation span outside tag region')
        return game.tag_offset+relative
    def action(position, kind, value, **details):
        actions.append(dict(field_offset=position-game.tag_offset, action=kind,
                            original=value, **details))
    def block(record, displacement, stride):
        position = record+displacement
        count,address,definition = struct.unpack_from('<iII',game.data,position)
        if count < 0 or count > game.tag_size//stride:
            raise ValueError('invalid relocation block count')
        if definition:
            raise ValueError('non-null serialized block definition requires schema handling')
        if not count:
            action(position+4,'clear_empty_block',address)
            return 0,0
        target = span(address,count*stride)
        action(position+4,'rebase_tag_pointer',address,
               target_offset=target-game.tag_offset,span_bytes=count*stride)
        return count,target
    if tag.groups[0] != 'bitm':
        raise ValueError('bitmap planner requires bitm tag')
    root = span(tag.data_addr,108)
    for displacement in (28,48):
        size,flags,file_offset,address,definition = struct.unpack_from('<iiIII',game.data,root+displacement)
        if size < 0:
            raise ValueError('negative tag data size')
        if address or definition:
            raise ValueError('resident tag data/definition requires separate handling')
        action(root+displacement+8,'preserve_file_offset',file_offset)
    count,sequences = block(root,84,64)
    for index in range(count):
        block(sequences+index*64,52,32)
    count,bitmaps = block(root,96,48)
    for index in range(count):
        record = bitmaps+index*48
        signature, = struct.unpack_from('<I',game.data,record)
        if signature != 0x6269746d:
            raise ValueError('bitmap signature mismatch')
        offset, = struct.unpack_from('<I',game.data,record+24)
        action(record+24,'preserve_pixel_offset',offset)
        for displacement in (40,44):
            value, = struct.unpack_from('<I',game.data,record+displacement)
            action(record+displacement,'clear_runtime_pointer',value)
    return dict(tag_index=tag.index, root_offset=root-game.tag_offset,
                actions=actions, executable=False,
                scope='bitmap fields only; tag directory and other groups excluded')

def plan_bitmaps(game):
    plans, errors = [], []
    for tag in game.tags:
        if tag.groups[0] != 'bitm':
            continue
        try:
            plans.append(plan_bitmap(game,tag))
        except (ValueError,struct.error) as error:
            errors.append(dict(tag_index=tag.index,error=str(error)))
    return dict(plans=plans,errors=errors,complete_map_relocation=False)
