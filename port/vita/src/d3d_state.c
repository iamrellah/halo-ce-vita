#include "halo_vita_pixel_state.h"
/* Render-state capture adapted from the CC0 upstream port/linux/src/d3d8_gl.c.
 * This table is CPU state, NOT an implementation of GPU draw state application.
 * Xbox inline wrappers also write it directly. Draws must inspect the table;
 * a dirty generation based only on these setters would miss those writes.
 */
#include "halo_vita_d3d_state.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
DWORD D3D__RenderState[D3DRS_MAX];
void halo_vita_d3d_state_reset(void)
{
    memset(D3D__RenderState,0,sizeof(D3D__RenderState));
    halo_vita_pixel_state_reset();
    D3D__RenderState[D3DRS_ZENABLE] = TRUE;
		D3D__RenderState[D3DRS_ZWRITEENABLE] = TRUE;
		D3D__RenderState[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
		D3D__RenderState[D3DRS_COLORWRITEENABLE] = 0x01010101u;
		D3D__RenderState[D3DRS_SRCBLEND] = D3DBLEND_ONE;
		D3D__RenderState[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
		D3D__RenderState[D3DRS_BLENDOP] = D3DBLENDOP_ADD;
		D3D__RenderState[D3DRS_CULLMODE] = D3DCULL_CCW;
		D3D__RenderState[D3DRS_FRONTFACE] = D3DFRONT_CW;
		D3D__RenderState[D3DRS_FILLMODE] = D3DFILL_SOLID;
		D3D__RenderState[D3DRS_ALPHAFUNC] = D3DCMP_ALWAYS;
		D3D__RenderState[D3DRS_STENCILFUNC] = D3DCMP_ALWAYS;
		D3D__RenderState[D3DRS_STENCILMASK] = 0xff;
		D3D__RenderState[D3DRS_STENCILWRITEMASK] = 0xff;
		D3D__RenderState[D3DRS_STENCILFAIL] = D3DSTENCILOP_KEEP;
		D3D__RenderState[D3DRS_STENCILZFAIL] = D3DSTENCILOP_KEEP;
		D3D__RenderState[D3DRS_STENCILPASS] = D3DSTENCILOP_KEEP;
}
void __fastcall D3DDevice_SetRenderState_Simple(DWORD method,DWORD value)
{
    /* January SDK method mapping, from xdk_d3d8.h. Also capture direct callers;
     * the inline wrapper's subsequent identical write is harmless. */
    switch(method) {
    case 0x40260: D3D__RenderState[0]=value;return;
    case 0x40264: D3D__RenderState[1]=value;return;
    case 0x40268: D3D__RenderState[2]=value;return;
    case 0x4026c: D3D__RenderState[3]=value;return;
    case 0x40270: D3D__RenderState[4]=value;return;
    case 0x40274: D3D__RenderState[5]=value;return;
    case 0x40278: D3D__RenderState[6]=value;return;
    case 0x4027c: D3D__RenderState[7]=value;return;
    case 0x40288: D3D__RenderState[8]=value;return;
    case 0x4028c: D3D__RenderState[9]=value;return;
    case 0x40a60: D3D__RenderState[10]=value;return;
    case 0x40a64: D3D__RenderState[11]=value;return;
    case 0x40a68: D3D__RenderState[12]=value;return;
    case 0x40a6c: D3D__RenderState[13]=value;return;
    case 0x40a70: D3D__RenderState[14]=value;return;
    case 0x40a74: D3D__RenderState[15]=value;return;
    case 0x40a78: D3D__RenderState[16]=value;return;
    case 0x40a7c: D3D__RenderState[17]=value;return;
    case 0x40a80: D3D__RenderState[18]=value;return;
    case 0x40a84: D3D__RenderState[19]=value;return;
    case 0x40a88: D3D__RenderState[20]=value;return;
    case 0x40a8c: D3D__RenderState[21]=value;return;
    case 0x40a90: D3D__RenderState[22]=value;return;
    case 0x40a94: D3D__RenderState[23]=value;return;
    case 0x40a98: D3D__RenderState[24]=value;return;
    case 0x40a9c: D3D__RenderState[25]=value;return;
    case 0x40aa0: D3D__RenderState[26]=value;return;
    case 0x40aa4: D3D__RenderState[27]=value;return;
    case 0x40aa8: D3D__RenderState[28]=value;return;
    case 0x40aac: D3D__RenderState[29]=value;return;
    case 0x40ab0: D3D__RenderState[30]=value;return;
    case 0x40ab4: D3D__RenderState[31]=value;return;
    case 0x40ab8: D3D__RenderState[32]=value;return;
    case 0x40abc: D3D__RenderState[33]=value;return;
    case 0x40ac0: D3D__RenderState[34]=value;return;
    case 0x40ac4: D3D__RenderState[35]=value;return;
    case 0x40ac8: D3D__RenderState[36]=value;return;
    case 0x40acc: D3D__RenderState[37]=value;return;
    case 0x40ad0: D3D__RenderState[38]=value;return;
    case 0x40ad4: D3D__RenderState[39]=value;return;
    case 0x40ad8: D3D__RenderState[40]=value;return;
    case 0x40adc: D3D__RenderState[41]=value;return;
    case 0x417f8: D3D__RenderState[42]=value;return;
    case 0x41e20: D3D__RenderState[43]=value;return;
    case 0x41e24: D3D__RenderState[44]=value;return;
    case 0x41e40: D3D__RenderState[45]=value;return;
    case 0x41e44: D3D__RenderState[46]=value;return;
    case 0x41e48: D3D__RenderState[47]=value;return;
    case 0x41e4c: D3D__RenderState[48]=value;return;
    case 0x41e50: D3D__RenderState[49]=value;return;
    case 0x41e54: D3D__RenderState[50]=value;return;
    case 0x41e58: D3D__RenderState[51]=value;return;
    case 0x41e5c: D3D__RenderState[52]=value;return;
    case 0x41e60: D3D__RenderState[53]=value;return;
    case 0x41d90: D3D__RenderState[54]=value;return;
    case 0x41e74: D3D__RenderState[55]=value;return;
    case 0x41e78: D3D__RenderState[56]=value;return;
    case 0x40354: D3D__RenderState[57]=value;return;
    case 0x4033c: D3D__RenderState[58]=value;return;
    case 0x40304: D3D__RenderState[59]=value;return;
    case 0x40300: D3D__RenderState[60]=value;return;
    case 0x40340: D3D__RenderState[61]=value;return;
    case 0x40344: D3D__RenderState[62]=value;return;
    case 0x40348: D3D__RenderState[63]=value;return;
    case 0x4035c: D3D__RenderState[64]=value;return;
    case 0x40310: D3D__RenderState[65]=value;return;
    case 0x4037c: D3D__RenderState[66]=value;return;
    case 0x40358: D3D__RenderState[67]=value;return;
    case 0x40374: D3D__RenderState[68]=value;return;
    case 0x40378: D3D__RenderState[69]=value;return;
    case 0x40364: D3D__RenderState[70]=value;return;
    case 0x40368: D3D__RenderState[71]=value;return;
    case 0x4036c: D3D__RenderState[72]=value;return;
    case 0x40360: D3D__RenderState[73]=value;return;
    case 0x40350: D3D__RenderState[74]=value;return;
    case 0x4034c: D3D__RenderState[75]=value;return;
    case 0x409f8: D3D__RenderState[76]=value;return;
    case 0x40384: D3D__RenderState[77]=value;return;
    case 0x40388: D3D__RenderState[78]=value;return;
    case 0x40330: D3D__RenderState[79]=value;return;
    case 0x40334: D3D__RenderState[80]=value;return;
    case 0x40338: D3D__RenderState[81]=value;return;
    default: fprintf(stderr,"native Vita: unknown simple render method %08lx\n",method);abort();
    }
}
void __fastcall D3DDevice_SetRenderState_Deferred(D3DRENDERSTATETYPE state, DWORD value)
{
	if ((unsigned long)state < D3DRS_MAX)
		D3D__RenderState[state] = value;
}

void WINAPI D3DDevice_SetRenderState_ZBias(DWORD value);

void WINAPI D3DDevice_SetRenderStateNotInline(D3DRENDERSTATETYPE state, DWORD value)
{
	if (state == D3DRS_ZBIAS)
		D3DDevice_SetRenderState_ZBias(value);
	else if ((unsigned long)state < D3DRS_MAX)
		D3D__RenderState[state] = value;
}

/* As the Xbox's D3D8 does it: a z bias is a polygon offset of -bias depth
units plus -bias/4 times the polygon's depth slope, enabled for every fill
mode. Without the slope term, decals (biased by 8) fight with the surface
under them wherever it is seen at an angle. */
void WINAPI D3DDevice_SetRenderState_ZBias(DWORD value)
{
	float offset = -(float)value;
	float slope = offset * 0.25f;
	DWORD enable = value != 0;

	memcpy(&D3D__RenderState[D3DRS_POLYGONOFFSETZSLOPESCALE], &slope, sizeof(slope));
	memcpy(&D3D__RenderState[D3DRS_POLYGONOFFSETZOFFSET], &offset, sizeof(offset));
	D3D__RenderState[D3DRS_POINTOFFSETENABLE] = enable;
	D3D__RenderState[D3DRS_WIREFRAMEOFFSETENABLE] = enable;
	D3D__RenderState[D3DRS_SOLIDOFFSETENABLE] = enable;
	D3D__RenderState[D3DRS_ZBIAS] = value;
}

#define COMPLEX_RENDER_STATE(name, state) \
	void WINAPI D3DDevice_SetRenderState_##name(DWORD value) { D3D__RenderState[state] = value; }

COMPLEX_RENDER_STATE(PSTextureModes, D3DRS_PSTEXTUREMODES)
COMPLEX_RENDER_STATE(VertexBlend, D3DRS_VERTEXBLEND)
COMPLEX_RENDER_STATE(FogColor, D3DRS_FOGCOLOR)
COMPLEX_RENDER_STATE(FillMode, D3DRS_FILLMODE)
COMPLEX_RENDER_STATE(BackFillMode, D3DRS_BACKFILLMODE)
COMPLEX_RENDER_STATE(TwoSidedLighting, D3DRS_TWOSIDEDLIGHTING)
COMPLEX_RENDER_STATE(NormalizeNormals, D3DRS_NORMALIZENORMALS)
COMPLEX_RENDER_STATE(ZEnable, D3DRS_ZENABLE)
COMPLEX_RENDER_STATE(StencilEnable, D3DRS_STENCILENABLE)
COMPLEX_RENDER_STATE(StencilFail, D3DRS_STENCILFAIL)
COMPLEX_RENDER_STATE(FrontFace, D3DRS_FRONTFACE)
COMPLEX_RENDER_STATE(CullMode, D3DRS_CULLMODE)
COMPLEX_RENDER_STATE(TextureFactor, D3DRS_TEXTUREFACTOR)
COMPLEX_RENDER_STATE(LogicOp, D3DRS_LOGICOP)
COMPLEX_RENDER_STATE(EdgeAntiAlias, D3DRS_EDGEANTIALIAS)
COMPLEX_RENDER_STATE(MultiSampleAntiAlias, D3DRS_MULTISAMPLEANTIALIAS)
COMPLEX_RENDER_STATE(MultiSampleMask, D3DRS_MULTISAMPLEMASK)
COMPLEX_RENDER_STATE(MultiSampleType, D3DRS_MULTISAMPLETYPE)
COMPLEX_RENDER_STATE(ShadowFunc, D3DRS_SHADOWFUNC)
COMPLEX_RENDER_STATE(LineWidth, D3DRS_LINEWIDTH)
COMPLEX_RENDER_STATE(Dxt1NoiseEnable, D3DRS_DXT1NOISEENABLE)
COMPLEX_RENDER_STATE(YuvEnable, D3DRS_YUVENABLE)
COMPLEX_RENDER_STATE(OcclusionCullEnable, D3DRS_OCCLUSIONCULLENABLE)
COMPLEX_RENDER_STATE(StencilCullEnable, D3DRS_STENCILCULLENABLE)
COMPLEX_RENDER_STATE(RopZCmpAlwaysRead, D3DRS_ROPZCMPALWAYSREAD)
COMPLEX_RENDER_STATE(RopZRead, D3DRS_ROPZREAD)
COMPLEX_RENDER_STATE(DoNotCullUncompressed, D3DRS_DONOTCULLUNCOMPRESSED)


/* No integer casts between NV2A and GXM enums: their encodings differ. */
static int stencil_op(DWORD v,SceGxmStencilOp *out)
{
    switch(v) {
    case D3DSTENCILOP_KEEP:*out=SCE_GXM_STENCIL_OP_KEEP;break;
    case D3DSTENCILOP_ZERO:*out=SCE_GXM_STENCIL_OP_ZERO;break;
    case D3DSTENCILOP_REPLACE:*out=SCE_GXM_STENCIL_OP_REPLACE;break;
    case D3DSTENCILOP_INCRSAT:*out=SCE_GXM_STENCIL_OP_INCR;break;
    case D3DSTENCILOP_DECRSAT:*out=SCE_GXM_STENCIL_OP_DECR;break;
    case D3DSTENCILOP_INVERT:*out=SCE_GXM_STENCIL_OP_INVERT;break;
    case D3DSTENCILOP_INCR:*out=SCE_GXM_STENCIL_OP_INCR_WRAP;break;
    case D3DSTENCILOP_DECR:*out=SCE_GXM_STENCIL_OP_DECR_WRAP;break;
    default:return -1;
    }
    return 0;
}
int halo_vita_depth_stencil_decode(const DWORD *s,struct halo_vita_depth_stencil *out)
{
    static const SceGxmDepthFunc depth[8]={SCE_GXM_DEPTH_FUNC_NEVER,SCE_GXM_DEPTH_FUNC_LESS,
        SCE_GXM_DEPTH_FUNC_EQUAL,SCE_GXM_DEPTH_FUNC_LESS_EQUAL,SCE_GXM_DEPTH_FUNC_GREATER,
        SCE_GXM_DEPTH_FUNC_NOT_EQUAL,SCE_GXM_DEPTH_FUNC_GREATER_EQUAL,SCE_GXM_DEPTH_FUNC_ALWAYS};
    static const SceGxmStencilFunc stencil[8]={SCE_GXM_STENCIL_FUNC_NEVER,SCE_GXM_STENCIL_FUNC_LESS,
        SCE_GXM_STENCIL_FUNC_EQUAL,SCE_GXM_STENCIL_FUNC_LESS_EQUAL,SCE_GXM_STENCIL_FUNC_GREATER,
        SCE_GXM_STENCIL_FUNC_NOT_EQUAL,SCE_GXM_STENCIL_FUNC_GREATER_EQUAL,SCE_GXM_STENCIL_FUNC_ALWAYS};
    struct halo_vita_depth_stencil d;
    if(!s || !out || s[D3DRS_ZENABLE]>1)return -1; /* W buffering unsupported. */
    memset(&d,0,sizeof(d));
    d.depth_func=SCE_GXM_DEPTH_FUNC_ALWAYS;d.depth_write=SCE_GXM_DEPTH_WRITE_DISABLED;
    d.stencil_func=SCE_GXM_STENCIL_FUNC_ALWAYS;
    d.fail=d.zfail=d.pass=SCE_GXM_STENCIL_OP_KEEP;
    if(s[D3DRS_ZENABLE]) {
        if(s[D3DRS_ZFUNC]<D3DCMP_NEVER || s[D3DRS_ZFUNC]>D3DCMP_ALWAYS)return -1;
        d.depth_func=depth[s[D3DRS_ZFUNC]-D3DCMP_NEVER];
        d.depth_write=s[D3DRS_ZWRITEENABLE]?SCE_GXM_DEPTH_WRITE_ENABLED:SCE_GXM_DEPTH_WRITE_DISABLED;
    }
    if(s[D3DRS_STENCILENABLE]) {
        if(s[D3DRS_STENCILFUNC]<D3DCMP_NEVER || s[D3DRS_STENCILFUNC]>D3DCMP_ALWAYS ||
           stencil_op(s[D3DRS_STENCILFAIL],&d.fail)<0 ||
           stencil_op(s[D3DRS_STENCILZFAIL],&d.zfail)<0 ||
           stencil_op(s[D3DRS_STENCILPASS],&d.pass)<0)return -1;
        d.stencil_func=stencil[s[D3DRS_STENCILFUNC]-D3DCMP_NEVER];
        d.ref=s[D3DRS_STENCILREF]&255;d.read_mask=s[D3DRS_STENCILMASK]&255;
        d.write_mask=s[D3DRS_STENCILWRITEMASK]&255;
    }
    *out=d;return 0;
}
#include "halo_vita_graphics.h"
int halo_vita_depth_stencil_apply(void)
{
    struct halo_vita_depth_stencil d;
    SceGxmContext *context=halo_vita_graphics_context();
    if(!context || halo_vita_graphics_scene_active()!=2 ||
       halo_vita_depth_stencil_decode(D3D__RenderState,&d)<0)return -1;
    sceGxmSetTwoSidedEnable(context,SCE_GXM_TWO_SIDED_DISABLED);
    sceGxmSetFrontDepthFunc(context,d.depth_func);
    sceGxmSetFrontDepthWriteEnable(context,d.depth_write);
    sceGxmSetFrontStencilRef(context,d.ref);
    sceGxmSetFrontStencilFunc(context,d.stencil_func,d.fail,d.zfail,d.pass,d.read_mask,d.write_mask);
    return 0;
}

static int blend_factor(DWORD value,int alpha)
{
    switch(value) {
    case D3DBLEND_ZERO:return SCE_GXM_BLEND_FACTOR_ZERO;
    case D3DBLEND_ONE:return SCE_GXM_BLEND_FACTOR_ONE;
    case D3DBLEND_SRCCOLOR:return alpha?SCE_GXM_BLEND_FACTOR_SRC_ALPHA:SCE_GXM_BLEND_FACTOR_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:return alpha?SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case D3DBLEND_SRCALPHA:return SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:return SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:return SCE_GXM_BLEND_FACTOR_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA:return SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case D3DBLEND_DESTCOLOR:return alpha?SCE_GXM_BLEND_FACTOR_DST_ALPHA:SCE_GXM_BLEND_FACTOR_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR:return alpha?SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case D3DBLEND_SRCALPHASAT:return alpha?SCE_GXM_BLEND_FACTOR_ONE:SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    default:return -1; /* Constant factors need a shader path, not an approximation. */
    }
}
int halo_vita_blend_decode(const DWORD *s,SceGxmBlendInfo *out)
{
    SceGxmBlendInfo b;
    int src,dst,asrc,adst,op;
    DWORD mask;
    if(!s || !out)return -1;
    mask=s[D3DRS_COLORWRITEENABLE];
    if(mask&~0x01010101u)return -1;
    memset(&b,0,sizeof(b));
    if(mask&0x00000001u)b.colorMask|=SCE_GXM_COLOR_MASK_B;
    if(mask&0x00000100u)b.colorMask|=SCE_GXM_COLOR_MASK_G;
    if(mask&0x00010000u)b.colorMask|=SCE_GXM_COLOR_MASK_R;
    if(mask&0x01000000u)b.colorMask|=SCE_GXM_COLOR_MASK_A;
    if(s[D3DRS_ALPHABLENDENABLE]) {
        switch(s[D3DRS_BLENDOP]) {
        case D3DBLENDOP_ADD:op=SCE_GXM_BLEND_FUNC_ADD;break;
        case D3DBLENDOP_SUBTRACT:op=SCE_GXM_BLEND_FUNC_SUBTRACT;break;
        case D3DBLENDOP_REVSUBTRACT:op=SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT;break;
        case D3DBLENDOP_MIN:op=SCE_GXM_BLEND_FUNC_MIN;break;
        case D3DBLENDOP_MAX:op=SCE_GXM_BLEND_FUNC_MAX;break;
        default:return -1; /* Signed Xbox equations require separate implementation. */
        }
        if(op==SCE_GXM_BLEND_FUNC_MIN || op==SCE_GXM_BLEND_FUNC_MAX) {
            /* Min/max compares unscaled source/destination components. */
            src=dst=asrc=adst=SCE_GXM_BLEND_FACTOR_ONE;
        } else {
            src=blend_factor(s[D3DRS_SRCBLEND],0);dst=blend_factor(s[D3DRS_DESTBLEND],0);
            asrc=blend_factor(s[D3DRS_SRCBLEND],1);adst=blend_factor(s[D3DRS_DESTBLEND],1);
            if(src<0 || dst<0 || asrc<0 || adst<0)return -1;
        }
        b.colorFunc=b.alphaFunc=op;
        b.colorSrc=src;b.colorDst=dst;b.alphaSrc=asrc;b.alphaDst=adst;
    }
    *out=b;return 0;
}

int halo_vita_cull_decode(DWORD value,SceGxmCullMode *out)
{
    SceGxmCullMode mode;
    if(!out)return -1;
    /* Xbox CULL names the winding discarded, not a FRONT/BACK face index.
     * Match the existing Xita GXM path; viewport/shader winding still needs
     * physical qualification before relying on it for game geometry. */
    switch(value) {
    case D3DCULL_NONE:mode=SCE_GXM_CULL_NONE;break;
    case D3DCULL_CW:mode=SCE_GXM_CULL_CW;break;
    case D3DCULL_CCW:mode=SCE_GXM_CULL_CCW;break;
    default:return -1;
    }
    *out=mode;return 0;
}
int halo_vita_cull_apply(void)
{
    SceGxmCullMode mode;
    SceGxmContext *context=halo_vita_graphics_context();
    if(!context || halo_vita_graphics_scene_active()!=2 ||
       halo_vita_cull_decode(D3D__RenderState[D3DRS_CULLMODE],&mode)<0)return -1;
    sceGxmSetCullMode(context,mode);return 0;
}
