import struct
from types import SimpleNamespace
import unittest
from extract_bitmap_fixture import extract

class FixtureTests(unittest.TestCase):
    def setUp(self):
        data = bytearray(8192)
        root, record, base = 4096, 4204, 0x803a6000
        struct.pack_into('<I',data,root+56,2048)
        struct.pack_into('<iII',data,root+96,1,base+108,0)
        struct.pack_into('<IhhhhhH',data,record,0x6269746d,4,4,1,0,14,2)
        struct.pack_into('<h',data,record+20,0)
        struct.pack_into('<ii',data,record+24,0,128)
        self.game=SimpleNamespace(data=data,file_size=8192,tag_offset=root,tag_size=4096,
            tags=[SimpleNamespace(index=6,groups=['bitm'],external=False,data_addr=base,name='fixture')])
    def test_exact_span(self):
        meta,payload=extract(self.game,6,0)
        self.assertEqual(len(payload),128)
        self.assertFalse(meta['hardware_sampling_verified'])
    def test_size_mismatch(self):
        struct.pack_into('<i',self.game.data,4204+28,127)
        with self.assertRaises(ValueError): extract(self.game,6,0)
    def test_negative_displacement(self):
        struct.pack_into('<i',self.game.data,4204+24,-1)
        with self.assertRaises(ValueError): extract(self.game,6,0)
    def test_tag_region_payload_rejected(self):
        struct.pack_into('<I',self.game.data,4096+56,4096)
        with self.assertRaises(ValueError): extract(self.game,6,0)
    def test_bad_block_pointer(self):
        struct.pack_into('<I',self.game.data,4096+100,0xffffffff)
        with self.assertRaises(ValueError): extract(self.game,6,0)
    def test_external_rejected(self):
        self.game.tags[0].external=True
        with self.assertRaises(ValueError): extract(self.game,6,0)

if __name__=='__main__': unittest.main()
