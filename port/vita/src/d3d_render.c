#include "halo_vita_texture_state.h"
#include "halo_vita_attributes.h"
#include "halo_vita_d3d_render.h"
#include "halo_vita_d3d_state.h"
#include "halo_vita_constants.h"
#include "halo_vita_backbuffer.h"
#include "halo_vita_logical_targets.h"
#include "halo_vita_graphics.h"
#include "halo_vita_device.h"
#include "halo_vita_clear_draw.h"
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
static D3DVIEWPORT8 viewport={0,0,640,480,0.0f,1.0f};
void halo_vita_d3d_render_reset(void)
{
    halo_vita_d3d_state_reset();
    halo_vita_texture_state_reset();
    halo_vita_constants_reset();
    halo_vita_attributes_reset();
    viewport=(D3DVIEWPORT8){0,0,640,480,0.0f,1.0f};
    halo_vita_constants_viewport(&viewport);
}
static void apply_viewport(void)
{
    float half_width=viewport.Width*0.5f,half_height=viewport.Height*0.5f;
    sceGxmSetViewport(halo_vita_graphics_context(),viewport.X+half_width,half_width,
        viewport.Y+half_height,-half_height,(viewport.MinZ+viewport.MaxZ)*0.5f,
        (viewport.MaxZ-viewport.MinZ)*0.5f);
}
int halo_vita_d3d_viewport_set(const D3DVIEWPORT8 *v)
{
    if(!v || !halo_vita_device_is_ready() || v->X>=640 || v->Y>=480 ||
       !v->Width || !v->Height || v->Width>640-v->X || v->Height>480-v->Y ||
       !(v->MinZ>=0 && v->MaxZ<=1 && v->MinZ<=v->MaxZ) || halo_vita_graphics_scene_active()==1)return -1;
    viewport=*v;
    halo_vita_constants_viewport(&viewport);
    if(halo_vita_graphics_scene_active()==2)apply_viewport();
    return 0;
}
int halo_vita_d3d_target_set(D3DSurface *color,D3DSurface *depth)
{
    unsigned index;
    int result;
    if(!halo_vita_device_is_ready() || halo_vita_backbuffers_target_index(color,depth,&index)<0)return -1;
    if(halo_vita_graphics_scene_active()) {
        if(halo_vita_graphics_scene_active()!=2 || halo_vita_logical_active_index()!=(int)index)return -1;
    } else {
        result=halo_vita_logical_begin(index);if(result)return result;
    }
    apply_viewport();return 0;
}
void WINAPI D3DDevice_SetViewport(const D3DVIEWPORT8 *v)
{
    if(halo_vita_d3d_viewport_set(v)<0) {
        fprintf(stderr,"native Vita: invalid/unsupported SetViewport\n");abort();
    }
}
void WINAPI D3DDevice_SetRenderTarget(D3DSurface *color,D3DSurface *depth)
{
    unsigned attempt;
    for(attempt=0;attempt<10000;attempt++) {
        int result=halo_vita_d3d_target_set(color,depth);
        if(!result)return;
        if(result<0)break;
        sceKernelDelayThread(1000);
    }
    fprintf(stderr,"native Vita: unsupported or stalled SetRenderTarget\n");abort();
}

void WINAPI D3DDevice_Clear(DWORD count,const D3DRECT *rectangles,DWORD flags,
    D3DCOLOR color,float z,DWORD stencil)
{
    int result;
    /* All inspected Halo clear sites use count=0, including alpha-only fog.
     * Rectangle lists require clipping semantics and are not silently ignored. */
    if(count || rectangles || halo_vita_graphics_scene_active()!=2) {
        fprintf(stderr,"native Vita: unsupported Clear rectangles/scene\n");abort();
    }
    /* Clear Z is independent of the scene's MinZ/MaxZ viewport depth range. */
    sceGxmSetViewport(halo_vita_graphics_context(),viewport.X+viewport.Width*0.5f,
        viewport.Width*0.5f,viewport.Y+viewport.Height*0.5f,-(viewport.Height*0.5f),0.5f,0.5f);
    result=halo_vita_clear_native(flags,color,z,stencil);
    apply_viewport();
    if(result>=0)result=halo_vita_depth_stencil_apply();
    if(result>=0)result=halo_vita_cull_apply();
    if(result<0){fprintf(stderr,"native Vita: Clear failed (%08x)\n",(unsigned)result);abort();}
}
