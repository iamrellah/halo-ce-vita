#ifndef HALO_VITA_TEXTURE_UPLOAD_H
#define HALO_VITA_TEXTURE_UPLOAD_H
#include "halo_vita_texture.h"
#include <psp2/gxm.h>
struct halo_vita_texture_upload {
    int uid, mapped, valid;
    unsigned references;
    void *base;
    SceGxmTexture texture;
};
#define HALO_VITA_TEXTURE_UPLOAD_INITIALIZER { -1, 0, 0, 0, NULL, {0} }
/* Single render-owner API; initialize object with macro. One decoded mip only.
 * On failure call destroy (retry if cleanup fails) before reuse.
 * Pin before publishing a draw; release ONLY after its GPU fence retires. */
int halo_vita_texture_upload_create(struct halo_vita_texture_upload *upload,
    const struct halo_vita_texture_view *view);
/* Explicit compressed 2D top-mip upload; preserves DXT blocks. */
int halo_vita_texture_upload_create_bc(struct halo_vita_texture_upload *,
    const struct halo_vita_texture_view *);
/* Experimental square DXT mip chain; explicit bring-up caller only. */
int halo_vita_texture_upload_create_bc_chain(struct halo_vita_texture_upload *,
    const struct halo_vita_bitmap_layout *, const void *, size_t);
int halo_vita_texture_upload_pin(struct halo_vita_texture_upload *upload);
int halo_vita_texture_upload_retire(struct halo_vita_texture_upload *upload);
int halo_vita_texture_upload_destroy(struct halo_vita_texture_upload *upload);
#endif
