#ifndef HALO_VITA_INDEX_SNAPSHOT_H
#define HALO_VITA_INDEX_SNAPSHOT_H
#include "halo_vita_indices.h"
#include <psp2/gxm.h>
struct halo_vita_index_snapshot {
    const uint16_t *data; size_t count; SceGxmPrimitiveType primitive;
    unsigned base_vertex,minimum,maximum;
};
int halo_vita_index_snapshots_create(void);
int halo_vita_index_snapshots_destroy(void);
/* Internal scene-begin hook: only after prior slot GPU completion. */
void halo_vita_index_snapshots_reset(unsigned slot);
int halo_vita_index_snapshot(D3DPRIMITIVETYPE type,size_t first,size_t count,
    struct halo_vita_index_snapshot *out);
#endif
