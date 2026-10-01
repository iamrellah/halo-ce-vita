#!/usr/bin/env python3
"""Walk an Xbox Halo CE cache file's tag data and structure BSPs with the
Invader layouts (tagdefs.py) and audit which words point into the tag cache
window: every such word should be a pointer field the walk reached.

usage: tagwalk.py <map> [...]"""
import struct, sys, zlib, collections, bisect
from tagdefs import Layout

BASE, WINDOW = 0x803A6000, 0x01600000
layout = Layout()
groups = layout.groups()


def decompressed(path):
    raw = open(path, 'rb').read()
    head = raw[:0x800]
    file_length = struct.unpack_from('<I', head, 8)[0]
    if len(raw) == file_length:
        return raw
    body = zlib.decompress(raw[0x800:])
    assert len(body) + 0x800 == file_length, (len(body), file_length)
    return head + body


class Memory:
    def __init__(self):
        self.regions = []   # (xbox address, bytes, name)

    def add(self, address, data, name):
        self.regions.append((address, data, name))

    def region(self, address):
        for r in self.regions:
            if r[0] <= address < r[0] + len(r[1]):
                return r
        return None

    def u32(self, address):
        r = self.region(address)
        if not r or address + 4 > r[0] + len(r[1]):
            return None
        return struct.unpack_from('<I', r[1], address - r[0])[0]


def walk_map(path):
    data = decompressed(path)
    tag_offset, tag_size = struct.unpack_from('<II', data, 0x10)
    mem = Memory()
    mem.add(BASE, data[tag_offset:tag_offset + tag_size], 'tags')
    pointers = {}             # location -> value
    owners = []               # (start, end, description)
    visited = set()
    problems = collections.Counter()

    def mark(location, what):
        value = mem.u32(location)
        if value is None:
            problems['mark outside memory ' + what] += 1
            return None
        pointers[location] = value
        if value and not (BASE <= value < BASE + WINDOW):
            problems['non-window pointer ' + what] += 1
        return value

    def walk(name, address, trail):
        key = (name, address)
        if key in visited:
            return
        visited.add(key)
        size = layout.sizes[name]
        if mem.u32(address) is None:
            problems['struct outside memory ' + name] += 1
            return
        owners.append((address, address + size, f'{trail}:{name}'))
        for offset, kind, child in layout.fields[name]:
            at = address + offset
            if kind == 'block':
                count = mem.u32(at)
                pointer = mark(at + 4, name + '.block')
                if pointer and BASE <= pointer < BASE + WINDOW and count:
                    if count > 0x100000:
                        problems['huge count ' + name] += 1
                        continue
                    child_size = layout.sizes[child]
                    for index in range(count):
                        walk(child, pointer + index * child_size, trail)
            elif kind == 'data':
                length = mem.u32(at)
                pointer = mark(at + 12, name + '.data')
                if pointer and BASE <= pointer < BASE + WINDOW and length:
                    owners.append((pointer, pointer + length, f'{trail}:{name}.data'))
            elif kind == 'reference':
                mark(at + 4, name + '.reference')
            elif kind == 'pointer':
                mark(at, name + '.pointer')
            elif kind == 'struct':
                walk(child, at, trail)

    # the tag header
    instances, scenario_index, checksum, count, vcount, vbuffers, icount, ibuffers, signature = struct.unpack_from('<9I', data, tag_offset)
    assert signature == 0x74616773, hex(signature)
    for location in (BASE, BASE + 0x14, BASE + 0x1C):
        mark(location, 'tag header')
    for index in range(vcount):
        mark(vbuffers + index * 12 + 4, 'tag vertex buffer')
    for index in range(icount):
        mark(ibuffers + index * 12 + 4, 'tag index buffer')
    owners.append((instances, instances + count * 32, 'tag index'))
    missing_groups = collections.Counter()
    scenario_address = None
    for index in range(count):
        entry = instances + index * 32
        group = mem.u32(entry)
        mark(entry + 16, 'tag index name')
        base = mark(entry + 20, 'tag index base')
        tag_datum = mem.u32(entry + 12)
        if (tag_datum & 0xffff) == scenario_index & 0xffff:
            scenario_address = base
        if not base or not (BASE <= base < BASE + tag_size):
            continue
        if group not in groups:
            missing_groups[group.to_bytes(4, 'big').decode('latin1')] += 1
            continue
        walk(groups[group], base, f'tag{index}')
    # structure BSPs
    scenario = groups[0x73636E72]
    bsp_block = [f for f in layout.fields[scenario] if f[1] == 'block' and f[2] == 'ScenarioBSP'][0][0]
    bsp_count = mem.u32(scenario_address + bsp_block)
    bsp_array = mem.u32(scenario_address + bsp_block + 4)
    bsps = []
    for index in range(bsp_count):
        start, size, address = struct.unpack_from('<III', data, tag_offset + bsp_array + index * 32 - BASE)
        bsps.append((start, size, address))
    results = [('tags', BASE, tag_size)]
    for number, (start, size, address) in enumerate(bsps):
        # each BSP is loaded on its own, over the same memory
        mem.regions = [r for r in mem.regions if r[2] == 'tags']
        mem.add(address, data[start:start + size], f'bsp{number}')
        header_pointer = mark(address, 'bsp header')
        vcount, vbuffers, lcount, lbuffers = struct.unpack_from('<IIII', data, start + 4)
        mark(address + 8, 'bsp header'); mark(address + 16, 'bsp header')
        for index in range(vcount):
            mark(vbuffers + index * 12 + 4, 'bsp vertex buffer')
        for index in range(lcount):
            mark(lbuffers + index * 12 + 4, 'bsp lightmap vertex buffer')
        walk('ScenarioStructureBSP', header_pointer, f'bsp{number}')
        results.append((f'bsp{number}', address, size))
    return data, mem, pointers, owners, problems, missing_groups, bsps, tag_offset, results


def audit(path):
    data, mem, pointers, owners, problems, missing_groups, bsps, tag_offset, results = walk_map(path)
    instances, _, _, count = struct.unpack_from('<4I', data, tag_offset)
    index_names = {}
    for index in range(count):
        entry = tag_offset + instances - BASE + index * 32
        index_names[struct.unpack_from('<I', data, entry + 12)[0]] = struct.unpack_from('<I', data, entry + 16)[0]
    owners.sort()
    starts = [o[0] for o in owners]

    def owner_of(address):
        i = bisect.bisect_right(starts, address) - 1
        best = None
        while i >= 0 and i >= bisect.bisect_right(starts, address) - 64:
            s, e, d = owners[i]
            if s <= address < e and (best is None or e - s < best[1] - best[0]):
                best = owners[i]
            i -= 1
        return best

    print(f'== {path}: {len(pointers)} pointer fields, bsps {[(hex(a), hex(s)) for _, s, a in bsps]}')
    if missing_groups:
        print('   groups without layouts:', dict(missing_groups))
    for k, v in sorted(problems.items()):
        print(f'   problem: {k}: {v}')
    unexplained = collections.Counter(); examples = collections.defaultdict(list)
    for name, address, size in results:
        blob = data[tag_offset:tag_offset + size] if name == 'tags' else data[bsps[int(name[3:])][0]:][:size]
        words = struct.unpack_from('<%dI' % (size // 4), blob)
        relocated = 0
        for i, v in enumerate(words):
            if not (BASE <= v < BASE + WINDOW):
                continue
            location = address + i * 4
            if location in pointers:
                relocated += 1
                continue
            o = owner_of(location)
            key = (name, o[2].split(':', 1)[1] if o else 'no owner')
            # would a structural guess call it a pointer?
            prev = words[i - 1] if i else 0
            nxt = words[i + 1] if i + 1 < len(words) else 0
            after2 = words[i + 2] if i + 2 < len(words) else 0
            printable = all(32 <= (prev >> (8 * k)) & 255 < 127 for k in range(4))
            if printable and index_names.get(after2) == v:
                key = key + ('VERIFIED REFERENCE MISSED',)
            elif printable and (after2 >> 16) >= 0xe000:
                key = key + ('reference-like',)
            elif 1 <= prev <= 0x10000 and nxt == 0:
                key = key + ('looks like block',)
            unexplained[key] += 1
            if len(examples[key]) < 3:
                examples[key].append((location, (location - o[0]) if o else None, v))
        print(f'   {name}: {relocated} walked pointers in window')
    for key, n in unexplained.most_common():
        print(f'   unexplained {key}: {n}  e.g. ' + ', '.join(f'@{l:08x}+{off}={v:08x}' for l, off, v in examples[key]))


def run(path):
    import io, contextlib
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        audit(path)
    return out.getvalue()


if __name__ == '__main__':
    import multiprocessing
    with multiprocessing.Pool(6) as pool:
        for text in pool.imap(run, sys.argv[1:]):
            print(text, end='', flush=True)
