#include "halo_vita_logical_targets.h"
#include "halo_vita_allocation.h"
#include <psp2/kernel/sysmem.h>
#include <string.h>

/* Storage addresses do not rotate with display. The engine selects the
 * logical target explicitly; native presentation samples its color buffer. */
struct logical_memory { SceUID uid; void *base; int mapped; uint32_t id; };
static struct logical_memory memory[4]={{-1,NULL,0,0},{-1,NULL,0,0},{-1,NULL,0,0},{-1,NULL,0,0}};
static struct halo_vita_surface_slot slots[2];
static SceGxmRenderTarget *target;
static int ready;
int halo_vita_logical_targets_destroy(void)
{
    int i,result;
    for(i=0;i<4;i++)if(memory[i].id && halo_vita_allocation_busy(memory[i].id)!=0)return -1;
    ready=0;
    if(target) {
        result=sceGxmDestroyRenderTarget(target);if(result<0)return result;
        target=NULL;
    }
    for(i=1;i>=0;i--)if(slots[i].sync) {
        result=sceGxmSyncObjectDestroy(slots[i].sync);if(result<0)return result;
        slots[i].sync=NULL;
    }
    for(i=3;i>=0;i--) {
        if(memory[i].id) {
            result=halo_vita_allocation_unregister(memory[i].id);if(result<0)return result;
            memory[i].id=0;
        }
        if(memory[i].mapped) {
            result=sceGxmUnmapMemory(memory[i].base);if(result<0)return result;
            memory[i].mapped=0;
        }
        if(memory[i].uid>=0) {
            result=sceKernelFreeMemBlock(memory[i].uid);if(result<0)return result;
            memory[i].uid=-1;memory[i].base=NULL;
        }
    }
    memset(slots,0,sizeof(slots));return 0;
}
int halo_vita_logical_targets_create(void)
{
    SceGxmRenderTargetParams params;
    const unsigned width=640,height=480;
    unsigned i,depth_width,depth_height;
    int result;
    if(ready || target)return -1;
    for(i=0;i<4;i++)if(memory[i].uid>=0 || memory[i].id || memory[i].mapped)return -1;
    for(i=0;i<2;i++)if(slots[i].sync)return -1;
    depth_width=(width+SCE_GXM_TILE_SIZEX-1)&~(SCE_GXM_TILE_SIZEX-1);
    depth_height=(height+SCE_GXM_TILE_SIZEY-1)&~(SCE_GXM_TILE_SIZEY-1);
    for(i=0;i<4;i++) {
        unsigned logical_bytes=(i&1)?depth_width*depth_height*4:width*height*4;
        unsigned bytes=(logical_bytes+262143)&~262143;
        memory[i].uid=sceKernelAllocMemBlock("halo logical target",SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,bytes,NULL);
        if(memory[i].uid<0){result=memory[i].uid;goto fail;}
        result=sceKernelGetMemBlockBase(memory[i].uid,&memory[i].base);if(result<0)goto fail;
        memset(memory[i].base,0,bytes);
        result=sceGxmMapMemory(memory[i].base,bytes,SCE_GXM_MEMORY_ATTRIB_READ|SCE_GXM_MEMORY_ATTRIB_WRITE);
        if(result<0)goto fail;
        memory[i].mapped=1;
        result=halo_vita_allocation_register(memory[i].base,logical_bytes,
            (i&1)?HALO_VITA_ALLOCATION_S8D24_TILED:HALO_VITA_ALLOCATION_ARGB_LINEAR,&memory[i].id);
        if(result<0)goto fail;
    }
    __asm__ volatile("dsb sy" ::: "memory");
    for(i=0;i<2;i++) {
        slots[i].pixels=memory[2*i].base;slots[i].depth_pixels=memory[2*i+1].base;slots[i].width=width;slots[i].height=height;slots[i].stride=width;
        slots[i].color_allocation=memory[2*i].id;slots[i].depth_allocation=memory[2*i+1].id;
        result=sceGxmColorSurfaceInit(&slots[i].color,SCE_GXM_COLOR_FORMAT_A8R8G8B8,
            SCE_GXM_COLOR_SURFACE_LINEAR,SCE_GXM_COLOR_SURFACE_SCALE_NONE,
            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,width,height,width,slots[i].pixels);
        if(result<0)goto fail;
        result=sceGxmDepthStencilSurfaceInit(&slots[i].depth,SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
            SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,depth_width,memory[2*i+1].base,NULL);
        if(result<0)goto fail;
        result=sceGxmSyncObjectCreate(&slots[i].sync);if(result<0)goto fail;
    }
    memset(&params,0,sizeof(params));params.width=width;params.height=height;
    params.scenesPerFrame=1;params.multisampleMode=SCE_GXM_MULTISAMPLE_NONE;params.driverMemBlock=-1;
    result=sceGxmCreateRenderTarget(&params,&target);if(result<0)goto fail;
    ready=1;return 0;
fail:
    halo_vita_logical_targets_destroy();return result;
}
const struct halo_vita_surface_slot *halo_vita_logical_target_get(unsigned index)
{ return ready && index<2 ? &slots[index]:NULL; }
SceGxmRenderTarget *halo_vita_logical_render_target(void)
{ return ready?target:NULL; }
