#ifndef HALO_VITA_D3D_SURFACE_H
#define HALO_VITA_D3D_SURFACE_H
#include "xdk_xbox.h"
#include <stdint.h>
/* Exports the actual color-slot format and dimensions, not an emulated Xbox
 * physical address. Retains one allocation view; caller must release its ID.
 * Do not overwrite a live output without releasing its previous reference. */
int halo_vita_d3d_surface_export(unsigned slot,D3DSurface *surface,uint32_t *reference);
/* Allocate a managed header with one reference to the backing allocation.
 * Releasing the final header reference frees only the header; scene GPU pins
 * continue protecting storage. Output unchanged on failure. */
int halo_vita_d3d_surface_create(unsigned slot,D3DSurface **output);
int halo_vita_d3d_logical_surface_export(unsigned slot,D3DSurface *surface,uint32_t *reference);
int halo_vita_d3d_logical_surface_create(unsigned slot,D3DSurface **output);
int halo_vita_d3d_logical_depth_export(unsigned slot,D3DSurface *surface,uint32_t *reference);
int halo_vita_d3d_logical_depth_create(unsigned slot,D3DSurface **output);
int halo_vita_d3d_resource_count(D3DResource *resource,unsigned *count);
int halo_vita_d3d_resource_retain(D3DResource *resource,unsigned *remaining);
int halo_vita_d3d_resource_release(D3DResource *resource,unsigned *remaining);
/* Checked internal version; output unchanged on unsupported/invalid input. */
int halo_vita_d3d_surface_describe(const void *header,size_t bytes,
    unsigned level,D3DSURFACE_DESC *description);
#endif
