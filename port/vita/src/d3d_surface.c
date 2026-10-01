#include "halo_vita_d3d_surface.h"
#include "halo_vita_d3d_view.h"
#include "halo_vita_surfaces.h"
#include "halo_vita_logical_targets.h"
#include "halo_vita_allocation.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int export_color(const struct halo_vita_surface_slot *s,D3DFORMAT format,
    D3DSurface *surface,uint32_t *reference)
{
    D3DSurface result;
    struct halo_vita_d3d_view checked;
    unsigned pitch;
    if(!s || !surface || !reference || !s->width || !s->height ||
       s->width>4096 || s->height>4096 || s->stride>4096)return -1;
    pitch=s->stride*4;
    if(!pitch || pitch%64 || pitch<s->width*4)return -1;
    memset(&result,0,sizeof(result));
    result.Common=0x00050001; /* Surface, one header reference. */
    result.Data=(unsigned long)(uintptr_t)s->pixels;
    result.Format=((unsigned long)format<<8)|0x21;
    result.Size=((pitch/64-1)<<24)|((s->height-1)<<12)|(s->width-1);
    if(halo_vita_d3d_view_acquire(&result,sizeof(result),&checked)<0)return -1;
    if(checked.allocation!=s->color_allocation) {
        halo_vita_allocation_release(checked.allocation);return -1;
    }
    *surface=result;*reference=checked.allocation;return 0;
}
int halo_vita_d3d_surface_export(unsigned slot,D3DSurface *surface,uint32_t *reference)
{
    return export_color(halo_vita_surface_get(slot),D3DFMT_LIN_A8B8G8R8,surface,reference);
}
int halo_vita_d3d_logical_surface_export(unsigned slot,D3DSurface *surface,uint32_t *reference)
{
    return export_color(halo_vita_logical_target_get(slot),D3DFMT_LIN_A8R8G8B8,surface,reference);
}
int halo_vita_d3d_logical_depth_export(unsigned slot,D3DSurface *surface,uint32_t *reference)
{
    const struct halo_vita_surface_slot *s=halo_vita_logical_target_get(slot);
    D3DSurface result;
    struct halo_vita_d3d_view checked;
    if(!s || !surface || !reference || s->width!=640 || s->height!=480)return -1;
    memset(&result,0,sizeof(result));
    result.Common=0x00050001;result.Data=(unsigned long)(uintptr_t)s->depth_pixels;
    /* Xbox API metadata; backing address resolves explicitly to native tiled
     * depth. Lock/readback and depth-as-color need conversion, never alias it. */
    result.Format=((unsigned long)D3DFMT_LIN_D24S8<<8)|0x21;
    result.Size=0x271df27f;
    if(halo_vita_d3d_depth_view_acquire(&result,sizeof(result),&checked)<0)return -1;
    if(checked.allocation!=s->depth_allocation) {
        halo_vita_allocation_release(checked.allocation);return -1;
    }
    *surface=result;*reference=checked.allocation;return 0;
}
int halo_vita_d3d_surface_describe(const void *header,size_t bytes,
    unsigned level,D3DSURFACE_DESC *description)
{
    struct halo_vita_d3d_view view;
    D3DSURFACE_DESC result;
    if(!description || level)return -1;
    if(halo_vita_d3d_view_acquire(header,bytes,&view)<0 &&
       halo_vita_d3d_depth_view_acquire(header,bytes,&view)<0)return -1;
    memset(&result,0,sizeof(result));
    result.Format=view.format==HALO_VITA_ALLOCATION_ARGB_LINEAR ?
        D3DFMT_LIN_A8R8G8B8:D3DFMT_LIN_A8B8G8R8;
    if(view.format==HALO_VITA_ALLOCATION_S8D24_TILED)result.Format=D3DFMT_LIN_D24S8;
    result.Type=D3DRTYPE_SURFACE;
    result.Width=view.width;result.Height=view.height;result.Size=view.pitch*view.height;
    result.MultiSampleType=0x0011; /* Xbox D3DMULTISAMPLE_NONE. */
    /* Usage remains zero as in upstream describe_level; current engine callers
     * consume dimensions and Size. Resource usage tracking is not implemented. */
    halo_vita_allocation_release(view.allocation);
    *description=result;return 0;
}
static void unsupported_description(const char *api)
{
    fprintf(stderr,"native Vita: unsupported or invalid resource in %s\n",api);
    abort(); /* Xbox void API cannot propagate HRESULT; never publish fiction. */
}
void WINAPI D3DSurface_GetDesc(D3DSurface *surface,D3DSURFACE_DESC *description)
{
    if(halo_vita_d3d_surface_describe(surface,sizeof(*surface),0,description)<0)
        unsupported_description("D3DSurface_GetDesc");
}
void WINAPI D3DTexture_GetLevelDesc(D3DTexture *texture,UINT level,D3DSURFACE_DESC *description)
{
    if(halo_vita_d3d_surface_describe(texture,sizeof(*texture),level,description)<0)
        unsupported_description("D3DTexture_GetLevelDesc");
}
