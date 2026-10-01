/* Uniform conventions adapted from Xita xv_d3d.c; retain GPL licensing. */
#include "halo_vita_fragment_uniforms.h"
#include "halo_vita_graphics.h"
#include <string.h>
int halo_vita_fragment_uniforms_capture(const struct halo_vita_pixel_state *pixel,
    const DWORD *states,const float *texscale,const float *border,
    struct halo_vita_fragment_uniforms *out)
{
    struct halo_vita_fragment_uniforms u;DWORD color,func;
    if(!pixel || !states || !out)return -1;
    func=states[D3DRS_ALPHAFUNC];
    if(states[D3DRS_ALPHATESTENABLE] && (func<D3DCMP_NEVER || func>D3DCMP_ALWAYS))return -1;
    memset(&u,0,sizeof(u));u.key=pixel->key;memcpy(u.psc,pixel->constants,sizeof(u.psc));
    color=states[D3DRS_FOGCOLOR];
    u.fog[0]=((color>>16)&255)/255.0f;u.fog[1]=((color>>8)&255)/255.0f;
    u.fog[2]=(color&255)/255.0f;u.fog[3]=(color>>24)/255.0f;
    u.atest[0]=(states[D3DRS_ALPHAREF]&255)/255.0f;
    u.atest[1]=states[D3DRS_ALPHATESTENABLE]?(float)(func-D3DCMP_NEVER):7;
    u.atest[2]=states[D3DRS_ALPHATESTENABLE]?1:0;
    if(texscale){memcpy(u.texscale,texscale,sizeof(u.texscale));u.has_texscale=1;}
    if(border){memcpy(u.border,border,sizeof(u.border));u.has_border=1;}
    *out=u;return 0;
}
int halo_vita_fragment_bind(const struct halo_vita_fragment_program *f,
    const struct halo_vita_fragment_uniforms *u)
{
    const SceGxmProgramParameter *parameters[5];const float *data[5];
    unsigned counts[5],limits[5]={18,1,1,4,2},i,any=0;
    SceGxmContext *context=halo_vita_graphics_context();void *buffer=NULL;int result;
    if(!f || !f->program || !f->entry || !u || !context || halo_vita_graphics_scene_active()!=2 ||
       f->entry->key!=u->key || (f->texscale && !u->has_texscale) || (f->border1 && !u->has_border))return -1;
    parameters[0]=f->psc;parameters[1]=f->fogcolor;parameters[2]=f->atest;
    parameters[3]=f->texscale;parameters[4]=f->border1;
    data[0]=&u->psc[0][0];data[1]=u->fog;data[2]=u->atest;
    data[3]=&u->texscale[0][0];data[4]=&u->border[0][0];
    for(i=0;i<5;i++)if(parameters[i]) {
        unsigned elements=sceGxmProgramParameterGetArraySize(parameters[i]);
        if(sceGxmProgramParameterGetCategory(parameters[i])!=SCE_GXM_PARAMETER_CATEGORY_UNIFORM ||
           sceGxmProgramParameterGetType(parameters[i])!=SCE_GXM_PARAMETER_TYPE_F32 ||
           sceGxmProgramParameterGetComponentCount(parameters[i])!=4 || !elements || elements>limits[i])return -1;
        counts[i]=elements*4;any=1;
    }
    sceGxmSetFragmentProgram(context,f->program);
    if(!any)return 0;
    result=sceGxmReserveFragmentDefaultUniformBuffer(context,&buffer);
    if(result!=0 || !buffer)return result?result:-1;
    for(i=0;i<5;i++)if(parameters[i]) {
        /* Compiler-trimmed array prefixes receive only their declared extent. */
        result=sceGxmSetUniformDataF(buffer,parameters[i],0,counts[i],data[i]);
        if(result!=0)return result;
    }
    return 0;
}
