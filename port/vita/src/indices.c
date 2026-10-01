#include "halo_vita_indices.h"
#include "halo_vita_allocation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
WORD *D3D__IndexData;
static uint32_t binding;
static unsigned base_vertex;
int halo_vita_indices_set(const D3DIndexBuffer *buffer,unsigned base)
{
    D3DIndexBuffer h;
    uint32_t id=0;
    WORD *data=NULL;
    if(buffer) {
        memcpy(&h,buffer,sizeof(h));
        if((h.Common&0x70000)!=0x10000 || !h.Data || (h.Data&1) ||
           halo_vita_allocation_resolve((const void *)(uintptr_t)h.Data,2,HALO_VITA_ALLOCATION_INDEX_BYTES,&id)<0 ||
           halo_vita_allocation_retain(id)<0)return -1;
        data=(WORD *)(uintptr_t)h.Data;
    } else if(base)return -1;
    if(binding && halo_vita_allocation_release(binding)<0) {
        if(id)halo_vita_allocation_release(id);return -1;
    }
    binding=id;base_vertex=base;D3D__IndexData=data;return 0;
}
int halo_vita_indices_acquire(size_t first,size_t count,struct halo_vita_index_view *out)
{
    struct halo_vita_index_view v;
    uintptr_t address=(uintptr_t)D3D__IndexData;
    uint32_t id;
    if(!out || !binding || !count || first>(UINTPTR_MAX-address)/2 || count>SIZE_MAX/2)return -1;
    address+=first*2;
    if(halo_vita_allocation_resolve((const void *)address,count*2,HALO_VITA_ALLOCATION_INDEX_BYTES,&id)<0 ||
       id!=binding || halo_vita_allocation_retain(id)<0)return -1;
    v.data=(const uint16_t *)address;v.count=count;v.base_vertex=base_vertex;v.allocation=id;
    *out=v;return 0;
}
void WINAPI D3DDevice_SetIndices(D3DIndexBuffer *buffer,UINT base)
{
    if(halo_vita_indices_set(buffer,base)<0) {
        fprintf(stderr,"native Vita: invalid/unregistered index buffer\n");abort();
    }
}
