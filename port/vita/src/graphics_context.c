/* Native GXM context ownership. Uses SDK ring sizes and the same mapping modes
 * as Xita's working runtime; does not depend on guest CPU/D3D state. */
#include "halo_vita_graphics.h"
#include <psp2/kernel/sysmem.h>
#include <stdlib.h>
#include <string.h>

struct ring { SceUID uid; void *base; unsigned size; int mapped; };
static struct ring rings[4] = {
    {-1,NULL,SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,0},
    {-1,NULL,SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,0},
    {-1,NULL,SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,0},
    {-1,NULL,SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,0}
};
static SceGxmContext *context;
static SceGxmRenderTarget *target;
static void *host;
static int ready;
static unsigned scene_owner;

int halo_vita_graphics_destroy(void)
{
    int i, result;
    if(scene_owner)return -1;
    ready=0;
    /* Shutdown only. No full-GPU wait is introduced into a frame loop.
     * Display-queue/surface ownership belongs to a future presentation layer. */
    if (context) sceGxmFinish(context);
    if (target) {
        result=sceGxmDestroyRenderTarget(target);
        if (result<0) return result;
        target=NULL;
    }
    if (context) {
        result=sceGxmDestroyContext(context);
        if (result<0) return result;
        context=NULL;
    }
    for (i=3;i>=0;--i) {
        if (rings[i].mapped) {
            result=i==3 ? sceGxmUnmapFragmentUsseMemory(rings[i].base) :
                sceGxmUnmapMemory(rings[i].base);
            if (result<0) return result;
            rings[i].mapped=0;
        }
        if (rings[i].uid>=0) {
            result=sceKernelFreeMemBlock(rings[i].uid);
            if (result<0) return result;
            rings[i].uid=-1; rings[i].base=NULL;
        }
    }
    free(host);host=NULL;
    return 0;
}

int halo_vita_graphics_create(unsigned width, unsigned height)
{
    SceGxmContextParams cp;
    SceGxmRenderTargetParams rt;
    unsigned i, usse_offset=0;
    int result;
    if (!width || !height || width>960 || height>544 ||
        ready || context || target || host) return -1;
    for (i=0;i<4;i++) if (rings[i].uid>=0) return -1;
    host=calloc(1,SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
    if (!host) return -1;
    for (i=0;i<4;i++) {
        rings[i].uid=sceKernelAllocMemBlock("halo GXM ring",
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,rings[i].size,NULL);
        if (rings[i].uid<0) { result=rings[i].uid; goto fail; }
        result=sceKernelGetMemBlockBase(rings[i].uid,&rings[i].base);
        if (result<0) goto fail;
        result=i==3 ? sceGxmMapFragmentUsseMemory(rings[i].base,rings[i].size,&usse_offset) :
            sceGxmMapMemory(rings[i].base,rings[i].size,SCE_GXM_MEMORY_ATTRIB_READ);
        if (result<0) goto fail;
        rings[i].mapped=1;
    }
    memset(&cp,0,sizeof(cp));
    cp.hostMem=host;cp.hostMemSize=SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
    cp.vdmRingBufferMem=rings[0].base;cp.vdmRingBufferMemSize=rings[0].size;
    cp.vertexRingBufferMem=rings[1].base;cp.vertexRingBufferMemSize=rings[1].size;
    cp.fragmentRingBufferMem=rings[2].base;cp.fragmentRingBufferMemSize=rings[2].size;
    cp.fragmentUsseRingBufferMem=rings[3].base;cp.fragmentUsseRingBufferMemSize=rings[3].size;
    cp.fragmentUsseRingBufferOffset=usse_offset;
    result=sceGxmCreateContext(&cp,&context);
    if (result<0) goto fail;
    memset(&rt,0,sizeof(rt));
    rt.width=(unsigned short)width;rt.height=(unsigned short)height;
    rt.scenesPerFrame=1;rt.multisampleMode=SCE_GXM_MULTISAMPLE_NONE;
    rt.driverMemBlock=-1;
    result=sceGxmCreateRenderTarget(&rt,&target);
    if (result<0) goto fail;
    ready=1;
    return 0;
fail:
    /* Failed unmap/free retains ownership so destroy can be retried. */
    halo_vita_graphics_destroy();
    return result;
}

SceGxmContext *halo_vita_graphics_context(void) { return ready ? context : NULL; }
SceGxmRenderTarget *halo_vita_graphics_target(void) { return ready ? target : NULL; }

int halo_vita_graphics_scene_claim(unsigned owner)
{
    if(!ready || scene_owner || (owner!=1 && owner!=2))return -1;
    scene_owner=owner;return 0;
}
int halo_vita_graphics_scene_release(unsigned owner)
{
    if(!owner || scene_owner!=owner)return -1;
    scene_owner=0;return 0;
}

unsigned halo_vita_graphics_scene_active(void) { return scene_owner; }
