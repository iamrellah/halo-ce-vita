#ifndef HALO_VITA_VERTEX_BUFFER_H
#define HALO_VITA_VERTEX_BUFFER_H
#include "xdk_xbox.h"
#include <stddef.h>
int halo_vita_vertex_buffer_owned(const D3DResource *resource);
int halo_vita_vertex_buffer_release(D3DResource *resource,unsigned *remaining);
int halo_vita_vertex_buffers_collect(void);
int halo_vita_vertex_buffers_empty(void);
/* 0 success, 1 GPU reads pending, negative invalid. Failure preserves output. */
int halo_vita_vertex_buffer_lock(D3DVertexBuffer *buffer,size_t offset,size_t bytes,DWORD flags,BYTE **out);
#endif
