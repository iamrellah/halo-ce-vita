#ifndef HALO_VITA_TEXTURE_H
#define HALO_VITA_TEXTURE_H
#include <stdint.h>
#include <stddef.h>
/* One 2D mip/face/slice. Caller resolves mip offsets and owns buffers until return.
 * Output is packed ABGR8888 CPU memory; no GXM upload or cache publication. */
struct halo_vita_texture_view {
    const void *source;
    size_t source_bytes;
    unsigned width, height, bitmap_format, linear_pitch;
    int linear;
    const uint32_t *palette;
    size_t palette_entries;
};
int halo_vita_decode_texture(const struct halo_vita_texture_view *view,
    uint32_t *output, size_t output_bytes);
/* Bounded Xbox row-major DXT blocks to GXM block order; no decompression. */
int halo_vita_reorder_bc(const struct halo_vita_texture_view *, void *, size_t);
struct halo_vita_bitmap_layout {
    unsigned width, height, depth, type, format, flags, mip_count;
};
/* Select an engine-order cube face (0..5), or face 0 for 2D. No 3D swizzle yet.
 * Resource must contain the complete aligned Xbox hardware payload. */
int halo_vita_texture_select(const struct halo_vita_bitmap_layout *layout,
    const void *resource, size_t resource_bytes, unsigned mip, unsigned face,
    struct halo_vita_texture_view *view);
#endif
