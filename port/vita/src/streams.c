#include "halo_vita_streams.h"
#include "halo_vita_indices.h"
#include "halo_vita_allocation.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static struct { uintptr_t address; uint32_t allocation; unsigned stride; } streams[16];
int halo_vita_stream_set(unsigned slot,const D3DVertexBuffer *buffer,unsigned stride)
{
    D3DVertexBuffer header;
    uint32_t allocation=0;
    uintptr_t address=0;
    if(slot>=16)return -1;
    if(buffer) {
        memcpy(&header,buffer,sizeof(header));
        if((header.Common&0x70000u)!=0 || !header.Data || !stride)return -1;
        address=header.Data;
        if(halo_vita_allocation_resolve((const void *)address,1,
            HALO_VITA_ALLOCATION_VERTEX_BYTES,&allocation)<0 ||
            halo_vita_allocation_retain(allocation)<0)return -1;
    } else if(stride)return -1;
    /* Retain new storage before dropping old, including rebinding aliases. */
    if(streams[slot].allocation && halo_vita_allocation_release(streams[slot].allocation)<0) {
        if(allocation)halo_vita_allocation_release(allocation);return -1;
    }
    streams[slot].address=address;streams[slot].allocation=allocation;streams[slot].stride=stride;
    return 0;
}
int halo_vita_stream_acquire(unsigned slot,size_t first,size_t count,size_t element,
    struct halo_vita_stream_view *out)
{
    struct halo_vita_stream_view view;
    size_t offset,span;
    uint32_t id;
    uintptr_t address;
    unsigned stride;
    if(slot>=16 || !out || !count || !element || !streams[slot].allocation)return -1;
    stride=streams[slot].stride;
    if(element>stride || first>SIZE_MAX/stride || count-1>(SIZE_MAX-element)/stride)return -1;
    offset=first*stride;span=(count-1)*stride+element;
    if(offset>UINTPTR_MAX-streams[slot].address)return -1;
    address=streams[slot].address+offset;
    if(halo_vita_allocation_resolve((const void *)address,span,HALO_VITA_ALLOCATION_VERTEX_BYTES,&id)<0 ||
       id!=streams[slot].allocation || halo_vita_allocation_retain(id)<0)return -1;
    view.data=(const void *)address;view.bytes=span;view.stride=stride;view.allocation=id;
    *out=view;return 0;
}
int halo_vita_streams_reset(void)
{
    unsigned i;
    for(i=0;i<16;i++)if(halo_vita_stream_set(i,NULL,0)<0)return -1;
    if(halo_vita_indices_set(NULL,0)<0)return -1;
    return 0;
}
void WINAPI D3DDevice_SetStreamSource(UINT stream,D3DVertexBuffer *buffer,UINT stride)
{
    if(halo_vita_stream_set(stream,buffer,stride)<0) {
        fprintf(stderr,"native Vita: invalid/unregistered vertex stream %u\n",stream);abort();
    }
}
