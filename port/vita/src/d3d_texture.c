#include "halo_vita_d3d_view.h"
#include "halo_vita_allocation.h"

int halo_vita_d3d_texture_acquire(const void *header,size_t header_bytes,
    struct halo_vita_d3d_view *view,SceGxmTexture *texture)
{
    struct halo_vita_d3d_view candidate;
    SceGxmTexture descriptor;
    SceGxmTextureFormat format;
    int result;
    if(!view || !texture)return -1;
    result=halo_vita_d3d_view_acquire(header,header_bytes,&candidate);
    if(result<0)return result;
    format=candidate.format==HALO_VITA_ALLOCATION_ARGB_LINEAR ?
        SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB : SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
    result=sceGxmTextureInitLinearStrided(&descriptor,candidate.pixels,format,
        candidate.width,candidate.height,candidate.pitch);
    if(result<0)goto fail;
    result=sceGxmTextureSetMinFilter(&descriptor,SCE_GXM_TEXTURE_FILTER_POINT);
    if(result<0)goto fail;
    result=sceGxmTextureSetMagFilter(&descriptor,SCE_GXM_TEXTURE_FILTER_POINT);
    if(result<0)goto fail;
    result=sceGxmTextureSetUAddrMode(&descriptor,SCE_GXM_TEXTURE_ADDR_CLAMP);
    if(result<0)goto fail;
    result=sceGxmTextureSetVAddrMode(&descriptor,SCE_GXM_TEXTURE_ADDR_CLAMP);
    if(result<0)goto fail;
    result=sceGxmTextureValidate(&descriptor);
    if(result<0)goto fail;
    *view=candidate;*texture=descriptor;return 0;
fail:
    halo_vita_allocation_release(candidate.allocation);
    return result;
}
