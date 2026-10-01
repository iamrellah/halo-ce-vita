#include "halo_vita_texture_upload.h"
#include <psp2/kernel/sysmem.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

int halo_vita_texture_upload_destroy(struct halo_vita_texture_upload *u)
{
    int result;
    if (!u || u->references) return -1;
    if (u->mapped) {
        result=sceGxmUnmapMemory(u->base);
        if (result<0) return result;
        u->mapped=0;
    }
    u->valid=0;
    if (u->uid>=0) {
        result=sceKernelFreeMemBlock(u->uid);
        if (result<0) return result;
        u->uid=-1;u->base=NULL;
    }
    return 0;
}

int halo_vita_texture_upload_create(struct halo_vita_texture_upload *u,
    const struct halo_vita_texture_view *v)
{
    size_t row,stride,size,allocation;
    uint32_t *decoded;
    unsigned y;
    int result;
    if (!u || u->uid>=0 || u->mapped || u->valid || u->base || u->references || !v ||
        !v->width || !v->height || v->width>4096 || v->height>4096) return -1;
    row=(size_t)v->width*4;stride=(row+63)&~(size_t)63;
    size=stride*v->height;allocation=(size+4095)&~(size_t)4095;
    decoded=malloc(row*v->height);
    if (!decoded) return -1;
    result=halo_vita_decode_texture(v,decoded,row*v->height);
    if (result<0) { free(decoded);return result; }
    u->uid=sceKernelAllocMemBlock("halo texture upload",SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,allocation,NULL);
    if (u->uid<0) { result=u->uid;free(decoded);return result; }
    result=sceKernelGetMemBlockBase(u->uid,&u->base);
    if (result<0) goto fail;
    memset(u->base,0,allocation);
    for (y=0;y<v->height;y++)
        memcpy((unsigned char *)u->base+y*stride,(unsigned char *)decoded+y*row,row);
    /* Uncached upload storage: drain CPU writes before publishing to GXM. */
    __asm__ volatile("dsb sy" ::: "memory");
    result=sceGxmMapMemory(u->base,allocation,SCE_GXM_MEMORY_ATTRIB_READ);
    if (result<0) goto fail;
    u->mapped=1;
    result=sceGxmTextureInitLinearStrided(&u->texture,u->base,
        SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,v->width,v->height,stride);
    if (result<0) goto fail;
    u->valid=1;
    free(decoded);
    return 0;
fail:
    free(decoded);
    halo_vita_texture_upload_destroy(u);
    return result;
}

int halo_vita_texture_upload_pin(struct halo_vita_texture_upload *u)
{
    if (!u || !u->valid || !u->mapped || u->references==UINT_MAX) return -1;
    u->references++;
    return 0;
}

int halo_vita_texture_upload_retire(struct halo_vita_texture_upload *u)
{
    if (!u || !u->references) return -1;
    u->references--;
    return 0;
}

int halo_vita_texture_upload_create_bc(struct halo_vita_texture_upload *u,
    const struct halo_vita_texture_view *v)
{
    static const SceGxmTextureFormat formats[3]={SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR,
        SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR,SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR};
    size_t size,allocation;
    int result;
    if (!u || u->uid>=0 || u->mapped || u->valid || u->base || u->references ||
        !v || !v->source || v->linear || v->bitmap_format<14 || v->bitmap_format>16 ||
        v->width<4 || v->height<4 || v->width>4096 || v->height>4096 ||
        (v->width&(v->width-1)) || (v->height&(v->height-1))) return -1;
    size=(size_t)(v->width/4)*(v->height/4)*(v->bitmap_format==14?8:16);
    if (v->source_bytes<size) return -1;
    allocation=(size+4095)&~(size_t)4095;
    u->uid=sceKernelAllocMemBlock("halo compressed texture",SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,allocation,NULL);
    if (u->uid<0) return u->uid;
    result=sceKernelGetMemBlockBase(u->uid,&u->base);if(result<0)goto fail;
    memset(u->base,0,allocation);
    result=halo_vita_reorder_bc(v,u->base,size);if(result<0)goto fail;
    __asm__ volatile("dsb sy" ::: "memory");
    result=sceGxmMapMemory(u->base,allocation,SCE_GXM_MEMORY_ATTRIB_READ);if(result<0)goto fail;
    u->mapped=1;
    result=sceGxmTextureInitSwizzled(&u->texture,u->base,formats[v->bitmap_format-14],v->width,v->height,1);
    if(result<0)goto fail;
    u->valid=1;return 0;
fail:
    halo_vita_texture_upload_destroy(u);return result;
}

/* Experimental: explicit caller only until physical GPU sampling is verified.
 * Square DXT chains end at 4x4; do not guess rectangular sub-block mip layout. */
int halo_vita_texture_upload_create_bc_chain(struct halo_vita_texture_upload *u,
    const struct halo_vita_bitmap_layout *layout,const void *source,size_t bytes)
{
    static const SceGxmTextureFormat formats[3]={SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR,
        SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR,SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR};
    struct halo_vita_texture_view views[11];
    unsigned levels=1,dimension,i;
    size_t size=0,offset=0,allocation;
    int result;
    if (!u || u->uid>=0 || u->mapped || u->valid || u->base || u->references ||
        !layout || layout->type!=0 || layout->depth!=1 || layout->format<14 || layout->format>16 ||
        layout->width!=layout->height || layout->width<4 || layout->width>4096 ||
        (layout->width&(layout->width-1)) || (layout->flags&16) || layout->mip_count>15) return -1;
    dimension=layout->width;
    while (dimension>4 && levels<=layout->mip_count) { levels++;dimension>>=1; }
    for (i=0;i<levels;i++) {
        result=halo_vita_texture_select(layout,source,bytes,i,0,&views[i]);
        if(result<0)return result;
        size+=views[i].source_bytes;
    }
    allocation=(size+4095)&~(size_t)4095;
    u->uid=sceKernelAllocMemBlock("halo BC mip test",SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,allocation,NULL);
    if(u->uid<0)return u->uid;
    result=sceKernelGetMemBlockBase(u->uid,&u->base);if(result<0)goto fail;
    memset(u->base,0,allocation);
    for(i=0;i<levels;i++) {
        result=halo_vita_reorder_bc(&views[i],(unsigned char *)u->base+offset,size-offset);
        if(result<0)goto fail;
        offset+=views[i].source_bytes;
    }
    __asm__ volatile("dsb sy" ::: "memory");
    result=sceGxmMapMemory(u->base,allocation,SCE_GXM_MEMORY_ATTRIB_READ);if(result<0)goto fail;
    u->mapped=1;
    result=sceGxmTextureInitSwizzled(&u->texture,u->base,formats[layout->format-14],
        layout->width,layout->height,levels);if(result<0)goto fail;
    if(levels>1) {
        result=sceGxmTextureSetMipFilter(&u->texture,SCE_GXM_TEXTURE_MIP_FILTER_ENABLED);
        if(result<0)goto fail;
    }
    u->valid=1;return 0;
fail:
    halo_vita_texture_upload_destroy(u);return result;
}
