#ifndef HALO_VITA_D3D_RENDER_H
#define HALO_VITA_D3D_RENDER_H
#include "xdk_xbox.h"
void halo_vita_d3d_render_reset(void);
int halo_vita_d3d_viewport_set(const D3DVIEWPORT8 *viewport);
/* Primary owned color/depth pairs only. 1 means target still in flight. */
int halo_vita_d3d_target_set(D3DSurface *color,D3DSurface *depth);
#endif
