#include "halo_vita_shader_patcher.h"
#include <psp2/kernel/sysmem.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct patcher_pool { SceUID uid; void *base; unsigned size,offset; int mapped; };
static struct patcher_pool pools[3] = {
    {-1,NULL,512*1024,0,0},{-1,NULL,256*1024,0,0},{-1,NULL,256*1024,0,0}
};
static SceGxmShaderPatcher *patcher;
static unsigned references;
static int ready;
static void *host_alloc(void *user, unsigned bytes) { (void)user;return malloc(bytes); }
static void host_free(void *user, void *data) { (void)user;free(data); }

int halo_vita_shader_patcher_destroy(void)
{
    int i,result;
    if (references) return -1;
    ready=0;
    if (patcher) {
        result=sceGxmShaderPatcherDestroy(patcher);
        if (result<0) return result;
        patcher=NULL;
    }
    for (i=2;i>=0;i--) {
        if (pools[i].mapped) {
            result=i==2 ? sceGxmUnmapFragmentUsseMemory(pools[i].base) :
                i==1 ? sceGxmUnmapVertexUsseMemory(pools[i].base) :
                sceGxmUnmapMemory(pools[i].base);
            if (result<0) return result;
            pools[i].mapped=0;
        }
        if (pools[i].uid>=0) {
            result=sceKernelFreeMemBlock(pools[i].uid);
            if (result<0) return result;
            pools[i].uid=-1;pools[i].base=NULL;pools[i].offset=0;
        }
    }
    return 0;
}

int halo_vita_shader_patcher_create(void)
{
    unsigned i;
    int result;
    SceGxmShaderPatcherParams params;
    if (ready || patcher || references) return -1;
    for (i=0;i<3;i++) if (pools[i].uid>=0) return -1;
    for (i=0;i<3;i++) {
        pools[i].uid=sceKernelAllocMemBlock("halo shader patcher",
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,pools[i].size,NULL);
        if (pools[i].uid<0) { result=pools[i].uid;goto fail; }
        result=sceKernelGetMemBlockBase(pools[i].uid,&pools[i].base);
        if (result<0) goto fail;
        result=i==2 ? sceGxmMapFragmentUsseMemory(pools[i].base,pools[i].size,&pools[i].offset) :
            i==1 ? sceGxmMapVertexUsseMemory(pools[i].base,pools[i].size,&pools[i].offset) :
            sceGxmMapMemory(pools[i].base,pools[i].size,SCE_GXM_MEMORY_ATTRIB_READ);
        if (result<0) goto fail;
        pools[i].mapped=1;
    }
    memset(&params,0,sizeof(params));
    params.hostAllocCallback=host_alloc;params.hostFreeCallback=host_free;
    params.bufferMem=pools[0].base;params.bufferMemSize=pools[0].size;
    params.vertexUsseMem=pools[1].base;params.vertexUsseMemSize=pools[1].size;
    params.vertexUsseOffset=pools[1].offset;
    params.fragmentUsseMem=pools[2].base;params.fragmentUsseMemSize=pools[2].size;
    params.fragmentUsseOffset=pools[2].offset;
    result=sceGxmShaderPatcherCreate(&params,&patcher);
    if (result<0) goto fail;
    ready=1;return 0;
fail:
    halo_vita_shader_patcher_destroy();return result;
}

SceGxmShaderPatcher *halo_vita_shader_patcher_get(void) { return ready?patcher:NULL; }
int halo_vita_shader_patcher_retain(void)
{
    if (!ready || references==UINT_MAX) return -1;
    ++references;return 0;
}
int halo_vita_shader_patcher_release(void)
{
    if (!ready || !references) return -1;
    --references;return 0;
}
