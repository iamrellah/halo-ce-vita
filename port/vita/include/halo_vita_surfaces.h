#ifndef HALO_VITA_SURFACES_H
#define HALO_VITA_SURFACES_H
#include <psp2/gxm.h>
#include <stdint.h>
#define HALO_VITA_SURFACE_SLOTS 3
struct halo_vita_surface_slot {
    SceGxmColorSurface color;
    SceGxmDepthStencilSurface depth;
    SceGxmSyncObject *sync;
    void *pixels;
    void *depth_pixels;
    uint32_t color_allocation, depth_allocation;
    unsigned width, height, stride;
};
/* Single render owner; GXM already initialized. This does not submit or display.
 * Pin BEFORE publishing GPU/display references. Retire only after completion,
 * including scanout retirement; these functions do not infer fence completion. */
int halo_vita_surfaces_create(unsigned width, unsigned height);
int halo_vita_surfaces_destroy(void);
const struct halo_vita_surface_slot *halo_vita_surface_get(unsigned slot);
int halo_vita_surface_pin(unsigned slot);
int halo_vita_surface_retire(unsigned slot);
#endif
