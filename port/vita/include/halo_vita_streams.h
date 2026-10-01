#ifndef HALO_VITA_STREAMS_H
#define HALO_VITA_STREAMS_H
#include "xdk_xbox.h"
#include <stdint.h>
#include <stddef.h>
struct halo_vita_stream_view { const void *data; size_t bytes; unsigned stride; uint32_t allocation; };
int halo_vita_stream_set(unsigned stream,const D3DVertexBuffer *buffer,unsigned stride);
/* Returned view owns a CPU allocation reference; caller releases it. GPU use
 * additionally requires a pin tied to the submitting scene's retirement. */
int halo_vita_stream_acquire(unsigned stream,size_t first,size_t count,size_t element_bytes,
    struct halo_vita_stream_view *output);
int halo_vita_streams_reset(void);
#endif
