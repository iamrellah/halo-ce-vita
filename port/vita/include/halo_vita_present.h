#ifndef HALO_VITA_PRESENT_H
#define HALO_VITA_PRESENT_H
#include <psp2/gxm.h>
#include "halo_vita_texture_resource.h"
/* Exclusive owner of GXM display queue. Configure BEFORE sceGxmInitialize.
 * Requires live native context and 960x544 surface slots; no scaling yet. */
void halo_vita_present_configure(SceGxmInitializeParams *params);
int halo_vita_present_initialize(void);
/* 0 opens a scene; 1 means all slots still owned by display/GPU; <0 error. */
int halo_vita_present_begin(unsigned *slot);
int halo_vita_present_end(void);
/* Reclaim completed display slots without opening another scene. */
int halo_vita_present_reap(void);
/* Before submitting a draw, pin once per active scene/resource generation.
 * Sidecar remains alive until retirement. Returns error on capacity exhaustion;
 * caller MUST NOT submit an untracked draw. CPU render owner only. */
int halo_vita_present_use_texture(struct halo_vita_texture_resource *, uint32_t token);
/* Retain a registered allocation for this scene; deduplicated by ID.
 * Rejects active color/depth target feedback. Caller retains CPU view until
 * this call succeeds. GPU pin retires with this scene, including on shutdown. */
int halo_vita_present_use_allocation(uint32_t id);
/* No active scene/producers. Drains, detaches scanout, then retires pins.
 * Call before destroying surfaces/context or terminating GXM. */
int halo_vita_present_shutdown(void);
#endif
