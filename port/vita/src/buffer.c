#include "halo_vita_buffer.h"
#include "halo_vita_allocation.h"
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <string.h>
int halo_vita_buffer_destroy(struct halo_vita_buffer *b)
{
    int result;
    if(!b)return -1;
    if(b->allocation) {
        result=halo_vita_allocation_unregister(b->allocation);
        if(result<0)return result;
        b->allocation=0;
    }
    if(b->mapped) {
        result=sceGxmUnmapMemory(b->base);if(result<0)return result;
        b->mapped=0;
    }
    if(b->uid>=0) {
        result=sceKernelFreeMemBlock(b->uid);if(result<0)return result;
        b->uid=-1;b->base=NULL;b->bytes=0;
    }
    return 0;
}
int halo_vita_buffer_create_format(struct halo_vita_buffer *b,size_t bytes,uint32_t format)
{
    size_t rounded;
    int result;
    if((format!=HALO_VITA_ALLOCATION_VERTEX_BYTES && format!=HALO_VITA_ALLOCATION_INDEX_BYTES) || !b || b->uid>=0 || b->base || b->mapped || b->allocation || !bytes || bytes>SIZE_MAX-4095)return -1;
    rounded=(bytes+4095)&~(size_t)4095;
    b->uid=sceKernelAllocMemBlock("halo vertex storage",SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,rounded,NULL);
    if(b->uid<0)return b->uid;
    result=sceKernelGetMemBlockBase(b->uid,&b->base);if(result<0)goto fail;
    memset(b->base,0,rounded);
    __asm__ volatile("dsb sy" ::: "memory");
    result=sceGxmMapMemory(b->base,rounded,SCE_GXM_MEMORY_ATTRIB_READ);if(result<0)goto fail;
    b->mapped=1;b->bytes=bytes;
    /* Padding is not valid vertex data. */
    result=halo_vita_allocation_register(b->base,bytes,format,&b->allocation);
    if(result<0)goto fail;
    return 0;
fail:
    /* A failed cleanup preserves remaining ownership for caller retry. */
    halo_vita_buffer_destroy(b);return result;
}

int halo_vita_buffer_create(struct halo_vita_buffer *b,size_t bytes)
{ return halo_vita_buffer_create_format(b,bytes,HALO_VITA_ALLOCATION_VERTEX_BYTES); }
