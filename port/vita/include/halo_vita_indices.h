#ifndef HALO_VITA_INDICES_H
#define HALO_VITA_INDICES_H
#include "xdk_xbox.h"
#include <stddef.h>
#include <stdint.h>
extern WORD *D3D__IndexData;
struct halo_vita_index_view { const uint16_t *data; size_t count; unsigned base_vertex; uint32_t allocation; };
int halo_vita_indices_set(const D3DIndexBuffer *buffer,unsigned base_vertex);
/* CPU reference only. Xbox inline locks can rewrite this data: copy indices
 * into scene-owned mapped storage before deferred GPU submission. */
int halo_vita_indices_acquire(size_t first,size_t count,struct halo_vita_index_view *out);
#endif
