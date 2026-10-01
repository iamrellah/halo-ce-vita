#include "halo_vita_backbuffer.h"
#include "halo_vita_graphics.h"
#include <stdio.h>
#include <stdlib.h>
/* Two color headers followed by their corresponding depth headers. */
static D3DSurface *buffers[4];
static unsigned selected;
static int ready;
int halo_vita_backbuffers_close(void)
{
    unsigned i,count;
    if(halo_vita_graphics_scene_active())return -1;
    for(i=0;i<4;i++)if(buffers[i]) {
        if(halo_vita_d3d_resource_count((D3DResource *)buffers[i],&count)<0 || count!=1)return -1;
    }
    ready=0;
    for(i=0;i<4;i++)if(buffers[i]) {
        if(halo_vita_d3d_resource_release((D3DResource *)buffers[i],&count)<0)return -1;
        buffers[i]=NULL;
    }
    selected=0;return 0;
}
int halo_vita_backbuffers_open(void)
{
    unsigned i;
    if(ready || halo_vita_graphics_scene_active())return -1;
    for(i=0;i<4;i++)if(buffers[i])return -1;
    for(i=0;i<4;i++)if((i<2 ? halo_vita_d3d_logical_surface_create(i,&buffers[i]) :
        halo_vita_d3d_logical_depth_create(i-2,&buffers[i]))<0) {
        halo_vita_backbuffers_close();return -1;
    }
    selected=0;ready=1;return 0;
}
int halo_vita_backbuffers_select(unsigned index)
{
    if(!ready || index>=2 || halo_vita_graphics_scene_active())return -1;
    selected=index;return 0;
}
int halo_vita_backbuffer_acquire(D3DSurface **output)
{
    unsigned count;
    if(!ready || !output)return -1;
    if(halo_vita_d3d_resource_retain((D3DResource *)buffers[selected],&count)<0)return -1;
    *output=buffers[selected];return 0;
}
void WINAPI D3DDevice_GetBackBuffer(INT index,D3DBACKBUFFER_TYPE type,D3DSurface **output)
{
    if(index!=0 || type!=0 || halo_vita_backbuffer_acquire(output)<0) {
        fprintf(stderr,"native Vita: invalid or unavailable D3DDevice_GetBackBuffer\n");
        abort();
    }
}

HRESULT WINAPI D3DDevice_GetDepthStencilSurface(D3DSurface **output)
{
    unsigned count;
    if(!output)return E_INVALIDARG;
    *output=NULL;
    if(!ready)return E_FAIL;
    if(halo_vita_d3d_resource_retain((D3DResource *)buffers[selected+2],&count)<0)return E_FAIL;
    *output=buffers[selected+2];return S_OK;
}

int halo_vita_backbuffers_target_index(D3DSurface *color,D3DSurface *depth,unsigned *index)
{
    unsigned i;
    if(!ready || !index)return -1;
    for(i=0;i<2;i++)if(color==buffers[i] && depth==buffers[i+2]) {
        *index=selected;return 0;
    }
    return -1;
}
