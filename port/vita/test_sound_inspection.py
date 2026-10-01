import struct
from types import SimpleNamespace
import unittest
from sound_inspection import audit_sounds

BASE = 0x803a6000
class SoundInspectionTests(unittest.TestCase):
    def game(self):
        data = bytearray(512)
        struct.pack_into('<I',data,124,0xffffffff)
        struct.pack_into('<iII',data,152,1,BASE+164,0)
        struct.pack_into('<iII',data,224,1,BASE+236,0)
        struct.pack_into('<hh',data,276,1,-1)
        struct.pack_into('<iiIII',data,300,32,0,400,0,0)
        struct.pack_into('<iiIII',data,320,8,0,0,BASE+440,0)
        return SimpleNamespace(data=data,tag_offset=0,tag_size=512,
            tags=[SimpleNamespace(groups=['snd!'],data_addr=BASE,index=0)])

    def test_resident_and_file_data(self):
        game = self.game()
        before = bytes(game.data)
        result = audit_sounds(game)
        self.assertEqual(result['errors'],[])
        self.assertEqual(result['data_states']['samples_file_span_bounded'],1)
        self.assertEqual(result['data_states']['mouth_resident'],1)
        self.assertEqual(bytes(game.data),before)
        self.assertFalse(result['native_compatibility_proven'])

    def test_promotion_id_must_resolve(self):
        game = self.game()
        struct.pack_into('<I',game.data,112,0x736e6421)
        struct.pack_into('<I',game.data,124,123)
        self.assertEqual(len(audit_sounds(game)['errors']),1)
        game.tags[0].tag_id = 123
        self.assertEqual(audit_sounds(game)['errors'],[])

    def test_cached_name_with_zero_length(self):
        game = self.game()
        struct.pack_into('<I',game.data,116,BASE+470)
        game.data[470:474] = b'abc\0'
        result = audit_sounds(game)
        self.assertEqual(result['errors'],[])
        self.assertEqual(result['promotion_references']['name_length_cleared'],1)
        struct.pack_into('<i',game.data,120,2)
        self.assertEqual(len(audit_sounds(game)['errors']),1)

    def test_bad_permutation_link(self):
        game = self.game()
        struct.pack_into('<h',game.data,278,1)
        self.assertEqual(len(audit_sounds(game)['errors']),1)

    def test_resident_overrun(self):
        game = self.game()
        struct.pack_into('<I',game.data,332,BASE+510)
        self.assertEqual(len(audit_sounds(game)['errors']),1)

    def test_file_overrun_reported(self):
        game = self.game()
        struct.pack_into('<I',game.data,308,500)
        self.assertEqual(audit_sounds(game)['data_states']['samples_file_span_outside'],1)

if __name__ == '__main__':
    unittest.main()
