#ifndef HALO_VITA_TEXTURE_RESOURCE_H
#define HALO_VITA_TEXTURE_RESOURCE_H
#include "halo_vita_texture_upload.h"
/* Render-owner sidecar for ONE selected mip/face, not a full D3D texture.
 * Initialize once; never reset generation when a cache datum is reused.
 * Owner must keep source and loaded flag alive through unregister. I/O publishes
 * loaded with release ordering; payload stays immutable once loaded is true.
 * Registration does not inspect pixels. Retirement requires actual GPU completion.
 */
struct halo_vita_texture_resource {
    uint32_t generation;
    int registered, failed;
    const unsigned char *loaded;
    struct halo_vita_texture_view view;
    struct halo_vita_texture_upload upload;
};
#define HALO_VITA_TEXTURE_RESOURCE_INITIALIZER {0,0,0,NULL,{0},HALO_VITA_TEXTURE_UPLOAD_INITIALIZER}
int halo_vita_texture_resource_register(struct halo_vita_texture_resource *,
    const struct halo_vita_bitmap_layout *, const void *, size_t,
    unsigned mip, unsigned face, const unsigned char *loaded, uint32_t *token);
/* 0 ready, 1 I/O pending, negative failure. Ready conversion is reused. */
int halo_vita_texture_resource_prepare(struct halo_vita_texture_resource *, uint32_t);
int halo_vita_texture_resource_busy(const struct halo_vita_texture_resource *, uint32_t);
int halo_vita_texture_resource_pin(struct halo_vita_texture_resource *, uint32_t);
int halo_vita_texture_resource_retire(struct halo_vita_texture_resource *, uint32_t);
const SceGxmTexture *halo_vita_texture_resource_get(const struct halo_vita_texture_resource *, uint32_t);
int halo_vita_texture_resource_unregister(struct halo_vita_texture_resource *, uint32_t);
#endif
