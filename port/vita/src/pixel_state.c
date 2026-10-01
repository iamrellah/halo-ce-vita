/* Render-state layout follows CC0 upstream d3d8_gl.c. Identity helpers copied
 * from Xita runtime/xv_ps_key.h and xv_ps_identity.h retain Xita GPL licensing. */
#include "halo_vita_pixel_state.h"
#include "halo_vita_d3d_state.h"
#include "xv_ps_identity.h"
#include <stddef.h>
#include <string.h>
#define PS_WORDS 57u
_Static_assert(sizeof(DWORD)==4 && sizeof(D3DPIXELSHADERDEF)==240,"pixel shader ABI");
_Static_assert(offsetof(D3DPIXELSHADERDEF,PSC0Mapping)==PS_WORDS*4,"state prefix");
#define CHECK_FIELD(field,state) _Static_assert(offsetof(D3DPIXELSHADERDEF,field)==4*(state),"pixel state " #field)
CHECK_FIELD(PSAlphaInputs,D3DRS_PSALPHAINPUTS0);
CHECK_FIELD(PSFinalCombinerInputsABCD,D3DRS_PSFINALCOMBINERINPUTSABCD);
CHECK_FIELD(PSFinalCombinerInputsEFG,D3DRS_PSFINALCOMBINERINPUTSEFG);
CHECK_FIELD(PSConstant0,D3DRS_PSCONSTANT0_0);
CHECK_FIELD(PSConstant1,D3DRS_PSCONSTANT1_0);
CHECK_FIELD(PSAlphaOutputs,D3DRS_PSALPHAOUTPUTS0);
CHECK_FIELD(PSRGBInputs,D3DRS_PSRGBINPUTS0);
CHECK_FIELD(PSCompareMode,D3DRS_PSCOMPAREMODE);
CHECK_FIELD(PSFinalCombinerConstant0,D3DRS_PSFINALCOMBINERCONSTANT0);
CHECK_FIELD(PSFinalCombinerConstant1,D3DRS_PSFINALCOMBINERCONSTANT1);
CHECK_FIELD(PSRGBOutputs,D3DRS_PSRGBOUTPUTS0);
CHECK_FIELD(PSCombinerCount,D3DRS_PSCOMBINERCOUNT);
/* PSTextureModes is special render state 116, not prefix word 54. */
CHECK_FIELD(PSDotMapping,D3DRS_PSDOTMAPPING);
CHECK_FIELD(PSInputTexture,D3DRS_PSINPUTTEXTURE);
static DWORD mappings[3],packed[18];
static float colors[18][4];
static int colors_valid;
static xv_ps_identity_cache identities;
void halo_vita_pixel_state_reset(void)
{
    memset(mappings,0,sizeof(mappings));memset(&identities,0,sizeof(identities));colors_valid=0;
}
void WINAPI D3DDevice_SetPixelShaderProgram(D3DPIXELSHADERDEF *definition)
{
    if(!definition)return;
    /* Copy immediately: Halo reuses mutable shader-definition scratch data. */
    memmove(D3D__RenderState,definition,54*4);
    D3D__RenderState[D3DRS_PSTEXTUREMODES]=definition->PSTextureModes;
    D3D__RenderState[D3DRS_PSDOTMAPPING]=definition->PSDotMapping;
    D3D__RenderState[D3DRS_PSINPUTTEXTURE]=definition->PSInputTexture;
    memcpy(mappings,(const char *)definition+PS_WORDS*4,sizeof(mappings));
}
int halo_vita_pixel_state_capture(struct halo_vita_pixel_state *out)
{
    struct halo_vita_pixel_state state;DWORD words[18];unsigned i;
    if(!out)return -1;
    memcpy(&state.definition,D3D__RenderState,PS_WORDS*4);
    state.definition.PSTextureModes=D3D__RenderState[D3DRS_PSTEXTUREMODES];
    memcpy((char *)&state.definition+PS_WORDS*4,mappings,sizeof(mappings));
    if((state.definition.PSCombinerCount&255)>8)return -1;
    xv_ps_identity_lookup(&identities,&state.definition,&state.hash,&state.key);
    memcpy(words,state.definition.PSConstant0,32);
    memcpy(words+8,state.definition.PSConstant1,32);
    words[16]=state.definition.PSFinalCombinerConstant0;
    words[17]=state.definition.PSFinalCombinerConstant1;
    for(i=0;i<18;i++)if(!colors_valid || packed[i]!=words[i]) {
        DWORD color=words[i];packed[i]=color;
        colors[i][0]=((color>>16)&255)/255.0f;colors[i][1]=((color>>8)&255)/255.0f;
        colors[i][2]=(color&255)/255.0f;colors[i][3]=(color>>24)/255.0f;
    }
    colors_valid=1;memcpy(state.constants,colors,sizeof(colors));*out=state;return 0;
}
