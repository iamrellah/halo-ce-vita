/* Compile-only ABI contract for the read-only bitmap inventory. */
#include "cseries.h"
#include "tag_files/tag_groups.h"
#include "bitmaps/bitmap_group.h"
#include <stddef.h>
#define CHECK(name, condition) typedef char name[(condition) ? 1 : -1]
CHECK(group_size, sizeof(struct bitmap_group) == 108);
CHECK(group_import, offsetof(struct bitmap_group, import_bitmap) == 28);
CHECK(group_pixels, offsetof(struct bitmap_group, pixel_data) == 48);
CHECK(group_sequences, offsetof(struct bitmap_group, sequences) == 84);
CHECK(group_bitmaps, offsetof(struct bitmap_group, bitmaps) == 96);
CHECK(sequence_size, sizeof(struct bitmap_group_sequence) == 64);
CHECK(sequence_sprites, offsetof(struct bitmap_group_sequence, sprites) == 52);
CHECK(sprite_size, sizeof(struct bitmap_group_sprite) == 32);
CHECK(data_size, sizeof(struct bitmap_data) == 48);
CHECK(data_hardware, offsetof(struct bitmap_data, hardware_format) == 40);
CHECK(data_base, offsetof(struct bitmap_data, base_address) == 44);
