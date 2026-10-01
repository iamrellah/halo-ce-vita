#include "halo_vita_vertex_buffer.h"
#include "halo_vita_buffer.h"
#include "halo_vita_allocation.h"
#include <stdlib.h>
#include <string.h>
#define BUFFER_LIMIT 128
static struct entry { D3DVertexBuffer *header; struct halo_vita_buffer storage; unsigned count; } buffers[BUFFER_LIMIT];
static struct entry *find(const void *resource)
{
    unsigned i;
    if(!resource)return NULL;
    for(i=0;i<BUFFER_LIMIT;i++)if(buffers[i].header==resource)return &buffers[i];
    return NULL;
}
int halo_vita_vertex_buffer_owned(const D3DResource *resource) { return find(resource)!=NULL; }
int halo_vita_vertex_buffers_collect(void)
{
    unsigned i;
    for(i=0;i<BUFFER_LIMIT;i++) {
        struct entry *b=&buffers[i];
        int busy;
        if(!b->header || b->count)continue;
        if(b->storage.allocation) {
            busy=halo_vita_allocation_busy(b->storage.allocation);
            if(busy<0)return -1;
            if(busy)continue;
        }
        if(halo_vita_buffer_destroy(&b->storage)<0)return -1;
        free(b->header);memset(b,0,sizeof(*b));
    }
    return 0;
}
int halo_vita_vertex_buffer_release(D3DResource *resource,unsigned *remaining)
{
    struct entry *b=find(resource);
    if(!b || !remaining || !b->count || (b->header->Common&0xffff)!=b->count)return -1;
    b->count--;b->header->Common=(b->header->Common&~0xffffUL)|b->count;
    *remaining=b->count;
    /* CPU stream/view and GPU scene refs keep released storage alive. */
    return halo_vita_vertex_buffers_collect();
}
static HRESULT create_buffer(UINT bytes,DWORD usage,DWORD fvf,DWORD pool,D3DVertexBuffer **out,int index)
{
    unsigned i;
    struct entry *b;
    int result;
    if(!out)return (HRESULT)0x80070057;
    *out=NULL;
    /* Halo uses no FVF here; declarations supply layout. Storage is native
     * resident for default/managed pools. Dynamic is a usage hint, not discard. */
    if(!bytes || fvf || pool>1 || (usage&~0x208UL))return (HRESULT)0x80070057;
    if(halo_vita_vertex_buffers_collect()<0)return (HRESULT)0x80004005;
    for(i=0;i<BUFFER_LIMIT;i++)if(!buffers[i].header)break;
    if(i==BUFFER_LIMIT)return (HRESULT)0x8007000e;
    b=&buffers[i];b->header=calloc(1,sizeof(*b->header));
    if(!b->header)return (HRESULT)0x8007000e;
    b->storage.uid=-1;
    result=halo_vita_buffer_create_format(&b->storage,bytes,index?HALO_VITA_ALLOCATION_INDEX_BYTES:HALO_VITA_ALLOCATION_VERTEX_BYTES);
    if(result<0) { halo_vita_vertex_buffers_collect();return (HRESULT)0x8007000e; }
    b->count=1;b->header->Common=index?0x01010001:0x01000001;
    b->header->Data=(DWORD)(uintptr_t)b->storage.base;
    *out=b->header;return 0;
}

int halo_vita_vertex_buffers_empty(void)
{
    unsigned i;
    for(i=0;i<BUFFER_LIMIT;i++)if(buffers[i].header)return 0;
    return 1;
}

#include "halo_vita_logical_targets.h"
#include "halo_vita_graphics.h"
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
int halo_vita_vertex_buffer_lock(D3DVertexBuffer *header,size_t offset,size_t bytes,DWORD flags,BYTE **out)
{
    struct entry *b=find(header);
    int busy;
    if(!b || !out || !b->count || (header->Common&0x70000) || (header->Common&0xffff)!=b->count ||
       header->Data!=(DWORD)(uintptr_t)b->storage.base || (flags&~0xa0UL) ||
       offset>=b->storage.bytes)return -1;
    if(!bytes)bytes=b->storage.bytes-offset;
    if(bytes>b->storage.bytes-offset)return -1;
    /* Halo's dynamic pointer cache uses READONLY on subsequent accesses.
     * Do not interpret that hint as permission to overwrite in-flight data.
     * NOOVERWRITE needs per-range tracking before relaxing this check. */
    busy=halo_vita_allocation_gpu_busy(b->storage.allocation);
    if(busy)return busy;
    *out=(BYTE *)b->storage.base+offset;return 0;
}
void WINAPI D3DVertexBuffer_Lock(D3DVertexBuffer *buffer,UINT offset,UINT bytes,BYTE **out,DWORD flags)
{
    unsigned attempt,i;
    if(!out){fprintf(stderr,"native Vita: null vertex lock output\n");abort();}
    *out=NULL;
    for(attempt=0;attempt<10000;attempt++) {
        int result=halo_vita_vertex_buffer_lock(buffer,offset,bytes,flags,out);
        if(!result)return;
        if(result<0)break;
        /* Poll submitted scenes only. Waiting for an unfinished active scene
         * on its own render thread would deadlock; report unsupported hazard. */
        if(halo_vita_graphics_scene_active())break;
        for(i=0;i<2;i++)if(halo_vita_logical_poll(i)<0)goto invalid;
        sceKernelDelayThread(1000);
    }
invalid:
    fprintf(stderr,"native Vita: invalid or hazardous vertex lock\n");abort();
}

HRESULT WINAPI D3DDevice_CreateVertexBuffer(UINT bytes,DWORD usage,DWORD fvf,DWORD pool,D3DVertexBuffer **out)
{ return create_buffer(bytes,usage,fvf,pool,out,0); }
HRESULT WINAPI D3DDevice_CreateIndexBuffer(UINT bytes,DWORD usage,D3DFORMAT format,DWORD pool,D3DIndexBuffer **out)
{
    D3DVertexBuffer *temporary=NULL;
    HRESULT result;
    if(!out)return (HRESULT)0x80070057;
    *out=NULL;
    if(format!=D3DFMT_INDEX16 || (bytes&1))return (HRESULT)0x80070057;
    result=create_buffer(bytes,usage,0,pool,&temporary,1);
    if(result>=0)*out=(D3DIndexBuffer *)temporary;
    return result;
}
