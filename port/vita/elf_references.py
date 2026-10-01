"""Bounded ELF32 little-endian relocation audit; no game execution."""
from pathlib import Path
import struct

def allocated_imports(path):
    data=Path(path).read_bytes()
    def span(offset,size):
        if offset<0 or size<0 or offset+size>len(data):raise ValueError('ELF span out of bounds')
        return data[offset:offset+size]
    header=struct.unpack('<16sHHIIIIIHHHHHH',span(0,52))
    if header[0][:6]!=b'\x7fELF\x01\x01' or header[2]!=40 or header[11]!=40 or not header[12]:
        raise ValueError('Requires ordinary ARM ELF32 little-endian sections')
    sections=[struct.unpack('<10I',span(header[6]+i*40,40)) for i in range(header[12])]
    def section(index):
        if not 0<=index<len(sections):raise ValueError('Invalid ELF section index')
        return sections[index]
    def content(index):
        s=section(index);return span(s[4],s[5])
    def string(table,index):
        if not 0<=index<len(table):raise ValueError('Invalid string index')
        end=table.find(b'\0',index)
        if end<0:raise ValueError('Unterminated ELF name')
        return table[index:end].decode('utf-8')
    imports=set();definitions=set()
    for s in sections:
        if s[1]==2:
            if s[9]!=16 or s[5]%16:raise ValueError('Invalid symbol table')
            names=content(s[6])
            for offset in range(s[4],s[4]+s[5],16):
                sym=struct.unpack('<IIIBBH',span(offset,16))
                if sym[5]:definitions.add(string(names,sym[0]))
        if s[1] not in (4,9) or not section(s[7])[2]&2:continue
        stride=12 if s[1]==4 else 8
        if s[9]!=stride or s[5]%stride:raise ValueError('Invalid relocation table')
        symbols=section(s[6]);names=content(symbols[6])
        if symbols[1]!=2 or symbols[9]!=16:raise ValueError('Invalid relocation symbol table')
        for offset in range(s[4],s[4]+s[5],stride):
            unused,info=struct.unpack('<II',span(offset,8));index=info>>8
            if index>=symbols[5]//16:raise ValueError('Invalid relocation symbol')
            sym=struct.unpack('<IIIBBH',span(symbols[4]+index*16,16))
            if not sym[5] and sym[0]:imports.add(string(names,sym[0]))
    return imports,definitions,sum(s[5] for s in sections if s[2]&2)
