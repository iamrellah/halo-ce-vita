/* State capture follows CC0 upstream d3d8_gl.c. Common address modes follow
 * existing Xita GXM bindings. Unsupported modes fail rather than approximate. */
#include "halo_vita_texture_state.h"
#include <string.h>
DWORD D3D__TextureState[4][D3DTSS_MAX];
void halo_vita_texture_state_reset(void)
{
    unsigned i;memset(D3D__TextureState,0,sizeof(D3D__TextureState));
    for(i=0;i<4;i++) {
        D3D__TextureState[i][D3DTSS_ADDRESSU]=D3DTADDRESS_WRAP;
        D3D__TextureState[i][D3DTSS_ADDRESSV]=D3DTADDRESS_WRAP;
        D3D__TextureState[i][D3DTSS_ADDRESSW]=D3DTADDRESS_WRAP;
        D3D__TextureState[i][D3DTSS_MINFILTER]=D3DTEXF_POINT;
        D3D__TextureState[i][D3DTSS_MAGFILTER]=D3DTEXF_POINT;
        D3D__TextureState[i][D3DTSS_MAXANISOTROPY]=1;
    }
}
void __fastcall D3DDevice_SetTextureState_Deferred(DWORD stage,D3DTEXTURESTAGESTATETYPE type,DWORD value)
{ if(stage<4 && (DWORD)type<D3DTSS_MAX)D3D__TextureState[stage][type]=value; }
void WINAPI D3DDevice_SetTextureState_BumpEnv(DWORD stage,D3DTEXTURESTAGESTATETYPE type,DWORD value)
{ D3DDevice_SetTextureState_Deferred(stage,type,value); }
#define SETTER(name,index) void WINAPI D3DDevice_SetTextureState_##name(DWORD stage,DWORD value) \
{ if(stage<4)D3D__TextureState[stage][index]=value; }
SETTER(TexCoordIndex,D3DTSS_TEXCOORDINDEX)
SETTER(BorderColor,D3DTSS_BORDERCOLOR)
SETTER(ColorKeyColor,D3DTSS_COLORKEYCOLOR)
static int address(DWORD mode,DWORD border,SceGxmTextureAddrMode *out)
{
    switch(mode) {
    case D3DTADDRESS_WRAP:*out=SCE_GXM_TEXTURE_ADDR_REPEAT;return 0;
    case D3DTADDRESS_MIRROR:*out=SCE_GXM_TEXTURE_ADDR_MIRROR;return 0;
    case D3DTADDRESS_CLAMP:case D3DTADDRESS_CLAMPTOEDGE:*out=SCE_GXM_TEXTURE_ADDR_CLAMP;return 0;
    case D3DTADDRESS_BORDER:
        if(border)return -1;
        *out=SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER;return 0;
    default:return -1;
    }
}
int halo_vita_sampler_apply(const DWORD *s,SceGxmTexture *texture)
{
    SceGxmTexture t;SceGxmTextureAddrMode u,v;SceGxmTextureType type;
    float bias;int result;
    if(!s || !texture)return -1;
    type=sceGxmTextureGetType(texture);
    if(type==SCE_GXM_TEXTURE_CUBE || type==SCE_GXM_TEXTURE_CUBE_ARBITRARY ||
       sceGxmTextureGetMipmapCount(texture)!=1)return -1;
    memcpy(&bias,&s[D3DTSS_MIPMAPLODBIAS],sizeof(bias));
    if(bias!=0 || s[D3DTSS_MAXMIPLEVEL] || s[D3DTSS_MIPFILTER]>D3DTEXF_LINEAR ||
       (s[D3DTSS_MINFILTER]!=D3DTEXF_POINT && s[D3DTSS_MINFILTER]!=D3DTEXF_LINEAR) ||
       (s[D3DTSS_MAGFILTER]!=D3DTEXF_POINT && s[D3DTSS_MAGFILTER]!=D3DTEXF_LINEAR))return -1;
    if(address(s[D3DTSS_ADDRESSU],s[D3DTSS_BORDERCOLOR],&u)<0 ||
       address(s[D3DTSS_ADDRESSV],s[D3DTSS_BORDERCOLOR],&v)<0)return -1;
    t=*texture;
    result=sceGxmTextureSetUAddrMode(&t,u);if(result<0)return result;
    result=sceGxmTextureSetVAddrMode(&t,v);if(result<0)return result;
    result=sceGxmTextureSetMinFilter(&t,s[D3DTSS_MINFILTER]==D3DTEXF_POINT?SCE_GXM_TEXTURE_FILTER_POINT:SCE_GXM_TEXTURE_FILTER_LINEAR);if(result<0)return result;
    result=sceGxmTextureSetMagFilter(&t,s[D3DTSS_MAGFILTER]==D3DTEXF_POINT?SCE_GXM_TEXTURE_FILTER_POINT:SCE_GXM_TEXTURE_FILTER_LINEAR);if(result<0)return result;
    /* A one-level resource has no neighboring mip regardless of mip filter. */
    result=sceGxmTextureSetMipFilter(&t,SCE_GXM_TEXTURE_MIP_FILTER_DISABLED);if(result<0)return result;
    result=sceGxmTextureValidate(&t);if(result<0)return result;
    *texture=t;return 0;
}
