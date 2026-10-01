"""Read-only sound pointer plan; cache IDs/file offsets remain unchanged."""
import struct
from sound_inspection import audit_sounds

BASE = 0x803a6000

def plan_sounds(game):
    audit = audit_sounds(game)
    if audit['errors']:
        return dict(plans=[],errors=audit['errors'],complete_map_relocation=False)
    plans, errors = [], []
    for tag in game.tags:
        if tag.groups[0] != 'snd!': continue
        actions = []
        def position(address):
            return game.tag_offset+address-BASE
        def emit(at, kind, original, **extra):
            actions.append(dict(field_offset=at-game.tag_offset,action=kind,original=original,**extra))
        def pointer(at, address, size):
            if not size:
                emit(at,'clear_empty_data',address)
            else:
                emit(at,'rebase_tag_pointer',address,target_offset=address-BASE,span_bytes=size)
        def block(at,stride):
            count,address,definition = struct.unpack_from('<iII',game.data,at)
            if definition: raise ValueError('unhandled sound block definition')
            pointer(at+4,address,count*stride)
            return count,position(address)
        try:
            root = position(tag.data_addr)
            group,name,length,index = struct.unpack_from('<IIiI',game.data,root+112)
            if name:
                start = position(name)
                end = game.data.find(b'\0',start,game.tag_offset+game.tag_size)
                pointer(root+116,name,end-start+1)
            emit(root+124,'preserve_tag_id',index)
            count,ranges = block(root+152,72)
            for r in range(count):
                count_p,permutations = block(ranges+r*72+60,124)
                for i in range(count_p):
                    record = permutations+i*124
                    for field in (64,84,104):
                        size,flags,file_offset,address,definition = struct.unpack_from('<iiIII',game.data,record+field)
                        if definition: raise ValueError('unhandled sound data definition')
                        emit(record+field+8,'preserve_file_offset',file_offset)
                        if address: pointer(record+field+12,address,size)
                        elif size and file_offset+size > len(game.data):
                            raise ValueError('sound file span outside image')
                    runtime, = struct.unpack_from('<I',game.data,record+48)
                    emit(record+48,'clear_runtime_pointer',runtime)
            plans.append(dict(tag_index=tag.index,root_offset=root-game.tag_offset,
                              actions=actions,executable=False))
        except (ValueError,struct.error) as error:
            errors.append(dict(tag_index=tag.index,error=str(error)))
    return dict(plans=plans,errors=errors,complete_map_relocation=False)
