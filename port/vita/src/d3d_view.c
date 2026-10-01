#include "halo_vita_d3d_view.h"
#include "halo_vita_allocation.h"
#include "xdk_xbox.h"
#include <string.h>
/* Packed-field constants from port/include/xdk/xdk_d3d8.h. Including its
 * inline API bodies would emit unrelated engine render-state imports here. */
enum {
    VITA_D3DCOMMON_TYPE_MASK = 0x00070000,
    VITA_D3DCOMMON_TYPE_TEXTURE = 0x00040000,
    VITA_D3DCOMMON_TYPE_SURFACE = 0x00050000,
    VITA_D3DFORMAT_DMACHANNEL_A = 0x00000001,
    VITA_D3DFORMAT_BORDERSOURCE_COLOR = 0x00000008,
    VITA_D3DFORMAT_DIMENSION_MASK = 0x000000F0,
    VITA_D3DFORMAT_DIMENSION_SHIFT = 4,
    VITA_D3DFORMAT_FORMAT_MASK = 0x0000FF00,
    VITA_D3DFORMAT_FORMAT_SHIFT = 8,
    VITA_D3DFORMAT_MIPMAP_MASK = 0x000F0000,
    VITA_D3DFORMAT_MIPMAP_SHIFT = 16,
    VITA_D3DSIZE_WIDTH_MASK = 0x00000FFF,
    VITA_D3DSIZE_HEIGHT_MASK = 0x00FFF000,
    VITA_D3DSIZE_HEIGHT_SHIFT = 12,
    VITA_D3DSIZE_PITCH_MASK = 0xFF000000,
    VITA_D3DSIZE_PITCH_SHIFT = 24
};
_Static_assert(sizeof(D3DBaseTexture)==20,"Xbox pixel-container prefix");
_Static_assert(offsetof(D3DSurface,Parent)==20,"Xbox surface prefix");
static int acquire_view(const void *header,size_t header_bytes,
    struct halo_vita_d3d_view *view,int depth)
{
    D3DBaseTexture pixel;
    struct halo_vita_d3d_view candidate;
    uint32_t type,format,allowed,mips;
    if(!header || header_bytes<sizeof(pixel) || !view)return -1;
    memcpy(&pixel,header,sizeof(pixel));
    type=pixel.Common&VITA_D3DCOMMON_TYPE_MASK;
    if(type!=VITA_D3DCOMMON_TYPE_TEXTURE && type!=VITA_D3DCOMMON_TYPE_SURFACE)return -1;
    allowed=VITA_D3DFORMAT_DMACHANNEL_A|VITA_D3DFORMAT_BORDERSOURCE_COLOR|
        VITA_D3DFORMAT_DIMENSION_MASK|VITA_D3DFORMAT_FORMAT_MASK|VITA_D3DFORMAT_MIPMAP_MASK;
    if(pixel.Format&~allowed)return -1;
    mips=(pixel.Format&VITA_D3DFORMAT_MIPMAP_MASK)>>VITA_D3DFORMAT_MIPMAP_SHIFT;
    /* Standalone surfaces may omit the mip count; texture headers require 1. */
    if(((pixel.Format&VITA_D3DFORMAT_DIMENSION_MASK)>>VITA_D3DFORMAT_DIMENSION_SHIFT)!=2 ||
       (mips!=1 && !(type==VITA_D3DCOMMON_TYPE_SURFACE && mips==0)))return -1;
    format=(pixel.Format&VITA_D3DFORMAT_FORMAT_MASK)>>VITA_D3DFORMAT_FORMAT_SHIFT;
    if(depth) {
        if(type!=VITA_D3DCOMMON_TYPE_SURFACE || format!=D3DFMT_LIN_D24S8)return -1;
        candidate.format=HALO_VITA_ALLOCATION_S8D24_TILED;
    }
    else if(format==D3DFMT_LIN_A8B8G8R8)candidate.format=HALO_VITA_ALLOCATION_ABGR_LINEAR;
    else if(format==D3DFMT_LIN_A8R8G8B8)candidate.format=HALO_VITA_ALLOCATION_ARGB_LINEAR;
    else return -1; /* Depth, swizzled and reinterpreted views need separate paths. */
    candidate.width=(pixel.Size&VITA_D3DSIZE_WIDTH_MASK)+1;
    candidate.height=((pixel.Size&VITA_D3DSIZE_HEIGHT_MASK)>>VITA_D3DSIZE_HEIGHT_SHIFT)+1;
    candidate.pitch=(((pixel.Size&VITA_D3DSIZE_PITCH_MASK)>>VITA_D3DSIZE_PITCH_SHIFT)+1)*64;
    if(candidate.pitch<candidate.width*4)return -1;
    candidate.pixels=(const void *)(uintptr_t)pixel.Data;
    /* Dimensions <=4096 and pitch <=16384: product fits size_t on ARM32. */
    if(halo_vita_allocation_resolve(candidate.pixels,candidate.pitch*candidate.height,
        candidate.format,&candidate.allocation)<0)return -1;
    if(halo_vita_allocation_retain(candidate.allocation)<0)return -1;
    *view=candidate;return 0;
}

int halo_vita_d3d_view_acquire(const void *header,size_t bytes,struct halo_vita_d3d_view *view)
{ return acquire_view(header,bytes,view,0); }
int halo_vita_d3d_depth_view_acquire(const void *header,size_t bytes,struct halo_vita_d3d_view *view)
{ return acquire_view(header,bytes,view,1); }
