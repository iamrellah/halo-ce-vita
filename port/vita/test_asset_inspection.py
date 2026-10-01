"""Synthetic binary parser tests only; does not execute Halo or Vita code."""
import struct
import tempfile
import unittest
from pathlib import Path
from check_assets import inspect, inspect_tag_index

class AssetInspection(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / 'synthetic.map'
        self.data = bytearray(0x900)
        struct.pack_into('<Iii', self.data, 0, 0x68656164, 5, len(self.data))
        struct.pack_into('<ii', self.data, 0x10, 0x800, 0x100)
        self.data[0x20:0x24] = b'b30\0'
        self.data[0x40:0x4e] = b'01.01.14.2342\0'
        struct.pack_into('<I', self.data, 0x7fc, 0x666f6f74)
        struct.pack_into('<I', self.data, 0x800, 0x803a6024)
        struct.pack_into('<i', self.data, 0x80c, 1)
        struct.pack_into('<I', self.data, 0x820, 0x74616773)
        struct.pack_into('<II', self.data, 0x834, 0x803a6050, 0x803a6060)

    def result(self):
        self.path.write_bytes(self.data)
        return inspect(self.path)

    def test_known_fields_not_a_full_plan(self):
        r = inspect_tag_index(self.path, self.result())
        self.assertEqual([x['offset'] for x in r['known_pointer_fields']], [0, 52, 56])
        self.assertFalse(r['complete_relocation_plan'])

    def test_directory_overrun(self):
        struct.pack_into('<i', self.data, 0x80c, 0x7fffffff)
        with self.assertRaisesRegex(ValueError, 'directory outside'):
            inspect_tag_index(self.path, self.result())

    def test_compressed_requires_unpacking(self):
        del self.data[0x800:]
        with self.assertRaisesRegex(ValueError, 'unpacked'):
            inspect_tag_index(self.path, self.result())

    def test_wrong_build(self):
        self.data[0x40:0x4e] = b'01.10.12.2276\0'
        self.assertFalse(self.result()['header_compatible'])

    def test_outside_pointer_is_not_rewritten(self):
        struct.pack_into('<I', self.data, 0x838, 0x90000000)
        r = inspect_tag_index(self.path, self.result())
        self.assertEqual(r['out_of_region'][0]['target'], 0x90000000)
        self.assertEqual(self.path.read_bytes(), self.data)

if __name__ == '__main__':
    unittest.main()
