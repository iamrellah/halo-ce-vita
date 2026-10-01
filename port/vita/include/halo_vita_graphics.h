#ifndef HALO_VITA_GRAPHICS_H
#define HALO_VITA_GRAPHICS_H
#include <psp2/gxm.h>
/* Single render-thread owner. GXM must already be initialized and must remain
 * initialized through destroy. This module owns no display queue or surfaces. */
int halo_vita_graphics_create(unsigned width, unsigned height);
int halo_vita_graphics_destroy(void);
SceGxmContext *halo_vita_graphics_context(void);
SceGxmRenderTarget *halo_vita_graphics_target(void);
/* Single CPU owner: 1=display scene, 2=logical scene. Prevent nested scenes. */
unsigned halo_vita_graphics_scene_active(void);
int halo_vita_graphics_scene_claim(unsigned owner);
int halo_vita_graphics_scene_release(unsigned owner);
#endif
