#include "halo_vita_allocation.h"
#include <string.h>
#define CAPACITY 128
struct allocation {
    uintptr_t base,end;
    uint32_t id,format,views,gpu;
};
static struct allocation entries[CAPACITY];
static uint32_t serial;
static struct allocation *find(uint32_t id)
{
    unsigned i;
    if(!id)return NULL;
    for(i=0;i<CAPACITY;i++)if(entries[i].id==id)return &entries[i];
    return NULL;
}
static int range(const void *base,size_t bytes,uintptr_t *end)
{
    uintptr_t start=(uintptr_t)base;
    if(!start || !bytes || bytes>UINTPTR_MAX-start)return -1;
    *end=start+bytes;return 0;
}
int halo_vita_allocation_register(void *base,size_t bytes,uint32_t format,uint32_t *id)
{
    uintptr_t end;
    struct allocation *free_entry=NULL;
    unsigned i;
    if(!id || range(base,bytes,&end)<0 || serial==UINT32_MAX)return -1;
    for(i=0;i<CAPACITY;i++) {
        struct allocation *a=&entries[i];
        if(!a->id) { if(!free_entry)free_entry=a;continue; }
        if((uintptr_t)base<a->end && end>a->base)return -1;
    }
    if(!free_entry)return -1;
    free_entry->base=(uintptr_t)base;free_entry->end=end;
    free_entry->format=format;free_entry->id=++serial;
    *id=free_entry->id;return 0;
}
int halo_vita_allocation_resolve(const void *address,size_t bytes,uint32_t format,uint32_t *id)
{
    uintptr_t end;
    unsigned i;
    if(!id || range(address,bytes,&end)<0)return -1;
    for(i=0;i<CAPACITY;i++) {
        const struct allocation *a=&entries[i];
        if(a->id && a->format==format && (uintptr_t)address>=a->base && end<=a->end) {
            *id=a->id;return 0;
        }
    }
    return -1;
}
int halo_vita_allocation_retain(uint32_t id)
{
    struct allocation *a=find(id);
    if(!a || a->views==UINT32_MAX)return -1;
    a->views++;return 0;
}
int halo_vita_allocation_release(uint32_t id)
{
    struct allocation *a=find(id);
    if(!a || !a->views)return -1;
    a->views--;return 0;
}
int halo_vita_allocation_pin(uint32_t id)
{
    struct allocation *a=find(id);
    if(!a || !a->views || a->gpu==UINT32_MAX)return -1;
    a->gpu++;return 0;
}
int halo_vita_allocation_retire(uint32_t id)
{
    struct allocation *a=find(id);
    if(!a || !a->gpu)return -1;
    a->gpu--;return 0;
}
int halo_vita_allocation_unregister(uint32_t id)
{
    struct allocation *a=find(id);
    if(!a || a->views || a->gpu)return -1;
    memset(a,0,sizeof(*a));return 0;
}

int halo_vita_allocation_busy(uint32_t id)
{
    struct allocation *a=find(id);
    return a ? (a->views!=0 || a->gpu!=0) : -1;
}

int halo_vita_allocation_gpu_busy(uint32_t id)
{
    struct allocation *a=find(id);
    return a ? a->gpu!=0 : -1;
}
