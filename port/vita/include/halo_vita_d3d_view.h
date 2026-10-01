#ifndef HALO_VITA_D3D_VIEW_H
#define HALO_VITA_D3D_VIEW_H
#include <stddef.h>
#include <stdint.h>
#include <psp2/gxm.h>
struct halo_vita_d3d_view {
    uint32_t allocation,width,height,pitch,format;
    const void *pixels;
};
/* Reads the common 20-byte D3D pixel-container prefix from a surface/texture.
 * Data must already contain a registered native CPU address, never an Xbox
 * physical address or an offset. Supports one-mip 2D linear 32-bit color only.
 * Success retains the allocation once; caller releases via allocation_release.
 * Output unchanged on failure. Header changes require acquiring a new view. */
int halo_vita_d3d_view_acquire(const void *header,size_t header_bytes,
    struct halo_vita_d3d_view *view);
/* Depth metadata only: pitch is logical row size; pixels is native TILED
 * storage, not CPU-linear rows and not a color/sampled texture descriptor. */
int halo_vita_d3d_depth_view_acquire(const void *header,size_t header_bytes,
    struct halo_vita_d3d_view *view);
/* Build a descriptor over the same storage: no allocation, decode or copy.
 * The owner must have mapped storage for GPU read and published CPU writes.
 * Success retains one CPU view. Before drawing, pin its allocation and retain
 * that GPU pin until completion; a descriptor alone establishes no fence.
 * Do not sample a current render target. This helper does not submit draws.
 * Both outputs remain unchanged on failure. */
int halo_vita_d3d_texture_acquire(const void *header,size_t header_bytes,
    struct halo_vita_d3d_view *view,SceGxmTexture *texture);
#endif
