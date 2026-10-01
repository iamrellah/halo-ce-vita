"""Planner tests use synthetic metadata, not game execution."""
import struct
import unittest
import test_bitmap_inspection as fixtures
BASE = fixtures.BASE
from bitmap_relocation import plan_bitmap

class RelocationTests(unittest.TestCase):
    def fixture(self):
        return fixtures.BitmapInspectionTests().game()

    def test_typed_fields_and_no_mutation(self):
        game = self.fixture()
        before = bytes(game.data)
        plan = plan_bitmap(game,game.tags[0])
        fields = {a['field_offset']:a for a in plan['actions']}
        self.assertEqual(fields[100]['target_offset'],128)
        self.assertEqual(fields[100]['span_bytes'],48)
        self.assertEqual(fields[152]['action'],'preserve_pixel_offset')
        self.assertEqual(fields[172]['action'],'clear_runtime_pointer')
        self.assertEqual(fields[172]['original'],0x2270040)
        self.assertEqual(bytes(game.data),before)
        self.assertFalse(plan['executable'])

    def test_unsupported_definitions_fail_closed(self):
        for field in (104,44,64):
            game = self.fixture()
            struct.pack_into('<I',game.data,field,BASE+300)
            with self.assertRaises(ValueError):
                plan_bitmap(game,game.tags[0])

    def test_resident_data_not_guessed(self):
        game = self.fixture()
        struct.pack_into('<I',game.data,60,BASE+300)
        with self.assertRaises(ValueError):
            plan_bitmap(game,game.tags[0])

    def test_outside_target_rejected(self):
        game = self.fixture()
        struct.pack_into('<I',game.data,100,BASE+500)
        with self.assertRaises(ValueError):
            plan_bitmap(game,game.tags[0])

if __name__ == '__main__':
    unittest.main()
