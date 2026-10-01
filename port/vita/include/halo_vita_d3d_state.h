#ifndef HALO_VITA_D3D_STATE_H
#define HALO_VITA_D3D_STATE_H
#include "xdk_xbox.h"
extern DWORD D3D__RenderState[D3DRS_MAX];
void halo_vita_d3d_state_reset(void);
#include <psp2/gxm.h>
struct halo_vita_depth_stencil {
    SceGxmDepthFunc depth_func;
    SceGxmDepthWriteMode depth_write;
    SceGxmStencilFunc stencil_func;
    SceGxmStencilOp fail,zfail,pass;
    unsigned ref,read_mask,write_mask;
};
/* Decode atomically: invalid enabled states leave output unchanged. */
int halo_vita_depth_stencil_decode(const DWORD *states,struct halo_vita_depth_stencil *out);
int halo_vita_depth_stencil_apply(void);
int halo_vita_blend_decode(const DWORD *states,SceGxmBlendInfo *out);
int halo_vita_cull_decode(DWORD value,SceGxmCullMode *out);
int halo_vita_cull_apply(void);
#endif
