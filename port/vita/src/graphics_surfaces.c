#include "halo_vita_surfaces.h"
#include "halo_vita_allocation.h"
#include <psp2/kernel/sysmem.h>
#include <limits.h>
#include <string.h>

struct storage { SceUID uid; void *base; int mapped; };
static struct storage memory[6] = {
    {-1,NULL,0},{-1,NULL,0},{-1,NULL,0},
    {-1,NULL,0},{-1,NULL,0},{-1,NULL,0}
};
static struct halo_vita_surface_slot slots[HALO_VITA_SURFACE_SLOTS];
static uint32_t allocation_ids[6];
static unsigned references[HALO_VITA_SURFACE_SLOTS];
static int ready;
static unsigned align_up(unsigned value, unsigned alignment)
{ return (value+alignment-1)&~(alignment-1); }

int halo_vita_surfaces_destroy(void)
{
    int i,result;
    for (i=0;i<HALO_VITA_SURFACE_SLOTS;i++) if (references[i]) return -1;
    /* Reject before any teardown if a copied resource view is still live. */
    for (i=0;i<6;i++)
        if (allocation_ids[i] && halo_vita_allocation_busy(allocation_ids[i])!=0) return -1;
    ready=0;
    for (i=HALO_VITA_SURFACE_SLOTS-1;i>=0;i--) {
        if (slots[i].sync) {
            result=sceGxmSyncObjectDestroy(slots[i].sync);
            if (result<0) return result;
            slots[i].sync=NULL;
        }
    }
    for (i=5;i>=0;i--) {
        if (allocation_ids[i]) {
            result=halo_vita_allocation_unregister(allocation_ids[i]);
            if(result<0)return result;
            allocation_ids[i]=0;
        }
        if (memory[i].mapped) {
            result=sceGxmUnmapMemory(memory[i].base);
            if (result<0) return result;
            memory[i].mapped=0;
        }
        if (memory[i].uid>=0) {
            result=sceKernelFreeMemBlock(memory[i].uid);
            if (result<0) return result;
            memory[i].uid=-1;memory[i].base=NULL;
        }
    }
    memset(slots,0,sizeof(slots));
    return 0;
}

int halo_vita_surfaces_create(unsigned width, unsigned height)
{
    unsigned i,stride,depth_width,depth_height,color_size,depth_size;
    int result;
    if (!width || !height || width>960 || height>544 || ready) return -1;
    for (i=0;i<6;i++) if (memory[i].uid>=0 || allocation_ids[i]) return -1;
    for (i=0;i<HALO_VITA_SURFACE_SLOTS;i++)
        if (slots[i].sync || references[i]) return -1;
    stride=align_up(width,64);
    depth_width=align_up(width,SCE_GXM_TILE_SIZEX);
    depth_height=align_up(height,SCE_GXM_TILE_SIZEY);
    /* Dimensions are bounded above: products and CDRAM rounding cannot wrap. */
    color_size=align_up(stride*height*4,256*1024);
    depth_size=align_up(depth_width*depth_height*4,256*1024);
    for (i=0;i<6;i++) {
        unsigned bytes=(i&1)?depth_size:color_size;
        memory[i].uid=sceKernelAllocMemBlock("halo GXM surface",
            SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,bytes,NULL);
        if (memory[i].uid<0) { result=memory[i].uid;goto fail; }
        result=sceKernelGetMemBlockBase(memory[i].uid,&memory[i].base);
        if (result<0) goto fail;
        memset(memory[i].base,0,bytes);
        result=sceGxmMapMemory(memory[i].base,bytes,
            SCE_GXM_MEMORY_ATTRIB_READ|SCE_GXM_MEMORY_ATTRIB_WRITE);
        if (result<0) goto fail;
        memory[i].mapped=1;
        result=halo_vita_allocation_register(memory[i].base,
            (i&1)?depth_width*depth_height*4:stride*height*4,
            (i&1)?HALO_VITA_ALLOCATION_S8D24_TILED:HALO_VITA_ALLOCATION_ABGR_LINEAR,
            &allocation_ids[i]);
        if(result<0)goto fail;
    }
    __asm__ volatile("dsb sy" ::: "memory");
    for (i=0;i<HALO_VITA_SURFACE_SLOTS;i++) {
        slots[i].pixels=memory[i*2].base;
        slots[i].depth_pixels=memory[i*2+1].base;
        slots[i].color_allocation=allocation_ids[i*2];
        slots[i].depth_allocation=allocation_ids[i*2+1];
        slots[i].width=width;slots[i].height=height;slots[i].stride=stride;
        result=sceGxmColorSurfaceInit(&slots[i].color,SCE_GXM_COLOR_FORMAT_A8B8G8R8,
            SCE_GXM_COLOR_SURFACE_LINEAR,SCE_GXM_COLOR_SURFACE_SCALE_NONE,
            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,width,height,stride,slots[i].pixels);
        if (result<0) goto fail;
        result=sceGxmDepthStencilSurfaceInit(&slots[i].depth,
            SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
            depth_width,memory[i*2+1].base,NULL);
        if (result<0) goto fail;
        result=sceGxmSyncObjectCreate(&slots[i].sync);
        if (result<0) goto fail;
    }
    ready=1;
    return 0;
fail:
    halo_vita_surfaces_destroy();
    return result;
}

const struct halo_vita_surface_slot *halo_vita_surface_get(unsigned slot)
{ return ready && slot<HALO_VITA_SURFACE_SLOTS ? &slots[slot] : NULL; }
int halo_vita_surface_pin(unsigned slot)
{
    if (!ready || slot>=HALO_VITA_SURFACE_SLOTS || references[slot]==UINT_MAX) return -1;
    ++references[slot];return 0;
}
int halo_vita_surface_retire(unsigned slot)
{
    if (!ready || slot>=HALO_VITA_SURFACE_SLOTS || !references[slot]) return -1;
    --references[slot];return 0;
}
