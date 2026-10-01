import struct
import unittest
from types import SimpleNamespace
from relocation_plan import normalize_plans

class PlanTests(unittest.TestCase):
    def fixture(self):
        data = bytearray(32)
        struct.pack_into('<I',data,0,0x803a6010)
        action = dict(field_offset=0,original=0x803a6010,action='rebase_tag_pointer',target_offset=16,span_bytes=4)
        return SimpleNamespace(data=data,tag_offset=0,tag_size=32),action
    def run_plan(self,game,actions):
        return normalize_plans(game,[dict(errors=[],plans=[dict(actions=actions)])])
    def test_native_field_order_and_duplicate_merge(self):
        game,a=self.fixture()
        r=self.run_plan(game,[a,a.copy()])
        self.assertEqual(r['entries'],[(0,0x803a6010,16,4,1)])
        self.assertFalse(r['loadable'])
    def test_preserve_conflict_rejected(self):
        game,a=self.fixture()
        b=a.copy();b['action']='preserve_file_offset'
        with self.assertRaises(ValueError): self.run_plan(game,[a,b])
    def test_stale_expected_value(self):
        game,a=self.fixture();a['original']=123
        with self.assertRaises(ValueError): self.run_plan(game,[a])
    def test_wrong_rebase_target(self):
        game,a=self.fixture();a['target_offset']=20
        with self.assertRaises(ValueError): self.run_plan(game,[a])
    def test_bad_target_span(self):
        game,a=self.fixture();a['span_bytes']=17
        with self.assertRaises(ValueError): self.run_plan(game,[a])
    def test_unknown_action(self):
        game,a=self.fixture();a['action']='guess_pointer'
        with self.assertRaises(ValueError): self.run_plan(game,[a])
if __name__=='__main__': unittest.main()
