#include "halo_vita_d3d_surface.h"
#include "halo_vita_allocation.h"
#include "halo_vita_vertex_buffer.h"
#include <stdlib.h>
#include <stdio.h>

/* Header ownership is distinct from backing storage and GPU ownership.
 * Single render owner. Only headers allocated here may be freed here. */
#define HEADER_LIMIT 64
struct owned_header { D3DSurface *surface; uint32_t allocation; unsigned count; };
static struct owned_header headers[HEADER_LIMIT];
static struct owned_header *find_header(const void *pointer)
{
    unsigned i;
    if(!pointer)return NULL;
    for(i=0;i<HEADER_LIMIT;i++)if(headers[i].surface==pointer)return &headers[i];
    return NULL;
}
static int create_surface(unsigned slot,D3DSurface **output,int logical)
{
    unsigned i;
    D3DSurface *surface;
    uint32_t allocation;
    int result;
    if(!output)return -1;
    for(i=0;i<HEADER_LIMIT;i++)if(!headers[i].surface)break;
    if(i==HEADER_LIMIT)return -1;
    surface=malloc(sizeof(*surface));
    if(!surface)return -1;
    result=logical==2 ? halo_vita_d3d_logical_depth_export(slot,surface,&allocation) :
        logical ? halo_vita_d3d_logical_surface_export(slot,surface,&allocation) :
        halo_vita_d3d_surface_export(slot,surface,&allocation);
    if(result<0) {
        free(surface);return -1;
    }
    surface->Common|=0x01000000; /* D3DCREATED header, not ownership of pixels. */
    headers[i].surface=surface;headers[i].allocation=allocation;headers[i].count=1;
    *output=surface;return 0;
}
int halo_vita_d3d_surface_create(unsigned slot,D3DSurface **output)
{ return create_surface(slot,output,0); }
int halo_vita_d3d_logical_surface_create(unsigned slot,D3DSurface **output)
{ return create_surface(slot,output,1); }
int halo_vita_d3d_logical_depth_create(unsigned slot,D3DSurface **output)
{ return create_surface(slot,output,2); }
int halo_vita_d3d_resource_count(D3DResource *resource,unsigned *count)
{
    struct owned_header *h=find_header(resource);
    if(!h || !count || (h->surface->Common&0xffff)!=h->count)return -1;
    *count=h->count;return 0;
}
int halo_vita_d3d_resource_retain(D3DResource *resource,unsigned *remaining)
{
    struct owned_header *h=find_header(resource);
    if(!h || !remaining || h->count==0xffff ||
       (h->surface->Common&0xffff)!=h->count)return -1;
    h->count++;h->surface->Common=(h->surface->Common&~0xffffUL)|h->count;
    *remaining=h->count;return 0;
}
int halo_vita_d3d_resource_release(D3DResource *resource,unsigned *remaining)
{
    struct owned_header *h=find_header(resource);
    if(!h || !remaining || !h->count ||
       (h->surface->Common&0xffff)!=h->count)return -1;
    if(h->count==1) {
        if(halo_vita_allocation_release(h->allocation)<0)return -1;
        free(h->surface);h->surface=NULL;h->allocation=0;h->count=0;
        *remaining=0;return 0;
    }
    h->count--;h->surface->Common=(h->surface->Common&~0xffffUL)|h->count;
    *remaining=h->count;return 0;
}
ULONG WINAPI D3DResource_Release(D3DResource *resource)
{
    unsigned remaining;
    int result=halo_vita_vertex_buffer_owned(resource) ?
        halo_vita_vertex_buffer_release(resource,&remaining) :
        halo_vita_d3d_resource_release(resource,&remaining);
    if(result<0) {
        fprintf(stderr,"native Vita: unsupported or invalid D3DResource_Release\n");
        abort(); /* Other resource types need their own ownership implementation. */
    }
    return remaining;
}
