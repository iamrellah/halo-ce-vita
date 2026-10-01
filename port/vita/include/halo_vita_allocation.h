#ifndef HALO_VITA_ALLOCATION_H
#define HALO_VITA_ALLOCATION_H
#include <stdint.h>
#include <stddef.h>
/* Single render-owner registry of BORROWED storage. Never frees memory.
 * Only the storage owner registers/unregisters; view users retain/release.
 * IDs survive copied D3D headers; no Xbox format/address bit decoding here.
 * Caller normalizes the header to an address and an exact native view format.
 * No cross-format reinterpretation is permitted by this first implementation.
 * Retain before publishing a CPU view, pin before GPU submission, retire only
 * after confirmed completion. Unregister before unmapping/freeing storage. */
enum halo_vita_allocation_format {
    HALO_VITA_ALLOCATION_ABGR_LINEAR=1,
    HALO_VITA_ALLOCATION_S8D24_TILED=2,
    HALO_VITA_ALLOCATION_ARGB_LINEAR=3,
    HALO_VITA_ALLOCATION_VERTEX_BYTES=4,
    HALO_VITA_ALLOCATION_INDEX_BYTES=5
};
/* 0 = no external references, 1 = busy, -1 = stale/unknown ID. */
int halo_vita_allocation_busy(uint32_t id);
/* GPU read ownership only: retained CPU views do not prevent rewriting. */
int halo_vita_allocation_gpu_busy(uint32_t id);
int halo_vita_allocation_register(void *base,size_t bytes,uint32_t format,uint32_t *id);
int halo_vita_allocation_resolve(const void *address,size_t bytes,uint32_t format,uint32_t *id);
int halo_vita_allocation_retain(uint32_t id);
int halo_vita_allocation_release(uint32_t id);
int halo_vita_allocation_pin(uint32_t id);
int halo_vita_allocation_retire(uint32_t id);
int halo_vita_allocation_unregister(uint32_t id);
#endif
