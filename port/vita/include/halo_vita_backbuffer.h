#ifndef HALO_VITA_BACKBUFFER_H
#define HALO_VITA_BACKBUFFER_H
#include "halo_vita_d3d_surface.h"
/* Color backbuffer API over stable logical buffers. Open after logical-target
 * creation; close before destroying them. Not the complete D3D device.
 * Device/frame owner explicitly selects index between scenes. */
int halo_vita_backbuffers_open(void);
int halo_vita_backbuffers_close(void);
int halo_vita_backbuffers_select(unsigned index);
int halo_vita_backbuffer_acquire(D3DSurface **output);
/* Cached primary headers select the CURRENT logical render buffer; their
 * stable Data values still identify specific buffers when copied for sampling.
 * Only managed matching color/depth pairs are accepted here. */
int halo_vita_backbuffers_target_index(D3DSurface *color,D3DSurface *depth,unsigned *index);
#endif
