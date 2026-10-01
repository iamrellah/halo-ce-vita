import struct
import unittest
import test_sound_inspection as fixtures
from sound_relocation import plan_sounds

class SoundRelocationTests(unittest.TestCase):
    def game(self):
        return fixtures.SoundInspectionTests().game()

    def test_classified_fields_preserved_and_rebased(self):
        game = self.game()
        before = bytes(game.data)
        result = plan_sounds(game)
        self.assertEqual(result['errors'],[])
        fields = {a['field_offset']:a for a in result['plans'][0]['actions']}
        self.assertEqual(fields[308]['action'],'preserve_file_offset')
        self.assertEqual(fields[308]['original'],400)
        self.assertEqual(fields[332]['target_offset'],440)
        self.assertEqual(fields[332]['span_bytes'],8)
        self.assertEqual(fields[284]['action'],'clear_runtime_pointer')
        self.assertEqual(fields[124]['action'],'preserve_tag_id')
        self.assertEqual(bytes(game.data),before)

    def test_definition_not_silently_rebased(self):
        game = self.game()
        struct.pack_into('<I',game.data,316,fixtures.BASE+400)
        result = plan_sounds(game)
        self.assertEqual(len(result['errors']),1)
        self.assertEqual(result['plans'],[])

    def test_file_overrun_blocks_plan(self):
        game = self.game()
        struct.pack_into('<I',game.data,308,500)
        self.assertEqual(len(plan_sounds(game)['errors']),1)

if __name__ == '__main__':
    unittest.main()
