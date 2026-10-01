#ifndef HALO_VITA_TEXTURE_DRAW_H
#define HALO_VITA_TEXTURE_DRAW_H
#include "halo_vita_texture.h"
#include "halo_vita_texture_resource.h"
/* One immutable uploaded texture; bring-up only. Live patcher required.
 * Submit inside native scene. Destroy after presentation shutdown, before
 * patcher/context destruction. Owns a pin until GPU-finished teardown. */
int halo_vita_texture_draw_create(const struct halo_vita_texture_view *view);
/* Borrow a registered resource; caller retains sidecar until draw destruction.
 * Returns 1 if I/O pending, without allocating draw resources. */
int halo_vita_texture_draw_create_resource(struct halo_vita_texture_resource *, uint32_t token);
/* Borrow native mapped storage described by a D3D pixel-container header.
 * Keeps one CPU view reference and records allocation pins per submitted scene. */
int halo_vita_texture_draw_create_d3d(const void *header,size_t bytes);
/* Between scenes, switch the borrowed view without waiting for old GPU reads. */
int halo_vita_texture_draw_bind_d3d(const void *header,size_t bytes);
int halo_vita_texture_draw_submit(void);
int halo_vita_texture_draw_destroy(void);
#endif
