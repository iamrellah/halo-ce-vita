#ifndef HALO_VITA_LOGICAL_TARGETS_H
#define HALO_VITA_LOGICAL_TARGETS_H
#include "halo_vita_surfaces.h"
/* Two stable 640x480 ARGB logical targets, independent of display slots.
 * GXM must be live. Single render owner. No draws/scenes are submitted here.
 * All GPU accesses must pin the corresponding allocations until completion;
 * destroy must be called after any scenes/render-target use has finished. */
int halo_vita_logical_targets_create(void);
int halo_vita_logical_targets_destroy(void);
const struct halo_vita_surface_slot *halo_vita_logical_target_get(unsigned index);
SceGxmRenderTarget *halo_vita_logical_render_target(void);
/* Scene operations return 1 for pending reads/writes, negative for failure.
 * Poll completion before sampling. Only one scene may own the shared context. */
int halo_vita_logical_begin(unsigned index);
int halo_vita_logical_end(void);
int halo_vita_logical_active_index(void);
int halo_vita_logical_poll(unsigned index);
int halo_vita_logical_read_ready(uint32_t allocation);
/* Caller retains a CPU view first. Pins once per active scene; returns 1 if
 * a source logical target is still being written. Rejects target feedback. */
int halo_vita_logical_use_allocation(uint32_t allocation);
struct halo_vita_texture_resource;
/* Prepared upload retained once per scene/generation until fragment completion.
 * The sidecar object itself must stay alive through that completion. */
int halo_vita_logical_use_texture(struct halo_vita_texture_resource *,uint32_t token);
#endif
