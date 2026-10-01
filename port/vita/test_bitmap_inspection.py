"""Synthetic malformed-map tests; never executes game code."""
import struct
from types import SimpleNamespace
import unittest
from bitmap_inspection import audit_bitmaps, predicted_pixel_size, predicted_hardware_size

BASE = 0x803a6000

class BitmapInspectionTests(unittest.TestCase):
    def game(self):
        data = bytearray(512)
        struct.pack_into('<iII', data, 96, 1, BASE+128, 0)
        struct.pack_into('<I', data, 128, 0x6269746d)
        struct.pack_into('<hhhhhH',data,132,8,4,1,0,0,0)
        struct.pack_into('<ii', data, 152, 256, 32)
        struct.pack_into('<I', data, 172, 0x2270040)
        return SimpleNamespace(data=data, tag_offset=0, tag_size=512,
            tags=[SimpleNamespace(groups=['bitm'], data_addr=BASE, index=7)])

    def test_serialized_address_is_not_dereferenced(self):
        result = audit_bitmaps(self.game())
        self.assertEqual(result['errors'], [])
        self.assertEqual(result['serialized_base_values'], {'0x2270040':1})
        self.assertEqual(result['payload_span_candidates'], {'bounded_candidate':1})
        self.assertFalse(result['native_compatibility_proven'])

    def test_mip_sizes(self):
        self.assertEqual(predicted_pixel_size(8,4,1,0,0,0,0),32)
        self.assertEqual(predicted_pixel_size(4,4,1,0,14,2,2),24)
        self.assertEqual(predicted_pixel_size(4,4,1,2,14,2,2),144)
        self.assertEqual(predicted_pixel_size(4,4,4,1,0,0,2),73)
        with self.assertRaises(ValueError):
            predicted_pixel_size(4,4,1,0,14,0,0)

    def test_hardware_rectangular_compressed_tail(self):
        self.assertEqual(predicted_hardware_size(128,32,1,0,15,3,7),5504)
        self.assertEqual(predicted_hardware_size(1024,256,1,0,16,3,10),349568)
        self.assertEqual(predicted_hardware_size(64,64,1,2,14,3,6),16896)

    def test_hardware_linear_row_padding(self):
        self.assertEqual(predicted_hardware_size(10,3,1,0,0,16,0),256)
        with self.assertRaises(ValueError):
            predicted_hardware_size(4,4,1,0,14,18,0)

    def test_bitmap_block_overrun(self):
        game = self.game()
        struct.pack_into('<iII', game.data, 96, 1, BASE+500, 0)
        self.assertEqual(len(audit_bitmaps(game)['errors']), 1)

    def test_negative_block_count(self):
        game = self.game()
        struct.pack_into('<i', game.data, 96, -1)
        self.assertEqual(len(audit_bitmaps(game)['errors']), 1)

    def test_bad_record_signature(self):
        game = self.game()
        struct.pack_into('<I', game.data, 128, 0)
        self.assertEqual(len(audit_bitmaps(game)['errors']), 1)

    def test_payload_overrun_and_negative_offset(self):
        for start, size in ((500,32), (-1,32), (256,-1)):
            game = self.game()
            struct.pack_into('<ii', game.data, 152, start, size)
            self.assertEqual(audit_bitmaps(game)['payload_span_candidates'],
                             {'invalid_candidate':1})

    def test_nested_sprite_overrun(self):
        game = self.game()
        struct.pack_into('<iII', game.data, 84, 1, BASE+200, 0)
        struct.pack_into('<iII', game.data, 252, 2, BASE+480, 0)
        self.assertEqual(len(audit_bitmaps(game)['errors']), 1)

if __name__ == '__main__':
    unittest.main()
