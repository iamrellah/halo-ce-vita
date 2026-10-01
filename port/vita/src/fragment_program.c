/* Link/parameter discovery follows Xita xv_shader.c; retain GPL licensing. */
#include "halo_vita_fragment_program.h"
#include "halo_vita_shader_patcher.h"
#include <string.h>
#include "../../../build/vita/native_fragment_catalog.h"
unsigned halo_vita_fragment_count(void)
{ return sizeof(native_fragments)/sizeof(native_fragments[0]); }
const struct halo_vita_fragment_entry *halo_vita_fragment_entry(unsigned i)
{ return i<halo_vita_fragment_count()?&native_fragments[i]:NULL; }
int halo_vita_fragment_find(uint32_t vs,uint32_t key,unsigned c2d,unsigned *out)
{
    unsigned lo=0,hi=halo_vita_fragment_count();
    if(!out || c2d>15)return -1;
    while(lo<hi) {
        unsigned mid=lo+(hi-lo)/2;
        const struct halo_vita_fragment_entry *e=&native_fragments[mid];
        if(e->vs_hash<vs || (e->vs_hash==vs && (e->key<key || (e->key==key && e->c2d_mask<c2d))))lo=mid+1;
        else hi=mid;
    }
    if(lo==halo_vita_fragment_count() || native_fragments[lo].vs_hash!=vs ||
       native_fragments[lo].key!=key || native_fragments[lo].c2d_mask!=c2d)return -1;
    *out=lo;return 0;
}
int halo_vita_fragment_destroy(struct halo_vita_fragment_program *f)
{
    SceGxmShaderPatcher *p=halo_vita_shader_patcher_get();int result;
    if(!f)return -1;
    if(f->program){result=sceGxmShaderPatcherReleaseFragmentProgram(p,f->program);if(result<0)return result;f->program=NULL;}
    if(f->registered){result=sceGxmShaderPatcherUnregisterProgram(p,f->id);if(result<0)return result;f->registered=0;}
    if(f->retained){result=halo_vita_shader_patcher_release();if(result<0)return result;f->retained=0;}
    memset(f,0,sizeof(*f));return 0;
}
int halo_vita_fragment_load(unsigned index,const struct halo_vita_vertex_program *vs,
    const SceGxmBlendInfo *blend,struct halo_vita_fragment_program *f)
{
    const struct halo_vita_fragment_entry *e=halo_vita_fragment_entry(index);
    SceGxmShaderPatcher *p=halo_vita_shader_patcher_get();const SceGxmProgram *source;
    static const char *names[4]={"tex0","tex1","tex2","tex3"};unsigned i;int result;
    if(!e || !p || !f || !vs || !vs->program || !vs->desc || !vs->source ||
       e->vs_hash!=vs->desc->func_hash || f->program || f->registered || f->retained)return -1;
    source=(const SceGxmProgram *)e->data;
    if(sceGxmProgramCheck(source)<0 || sceGxmProgramGetSize(source)>e->bytes ||
       sceGxmProgramGetType(source)!=SCE_GXM_FRAGMENT_PROGRAM)return -1;
    result=halo_vita_shader_patcher_retain();if(result<0)return result;f->retained=1;
    result=sceGxmShaderPatcherRegisterProgram(p,source,&f->id);if(result<0)goto fail;f->registered=1;
    result=sceGxmShaderPatcherCreateFragmentProgram(p,f->id,SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE,blend,vs->source,&f->program);if(result<0)goto fail;
    f->entry=e;f->source=source;
    f->psc=sceGxmProgramFindParameterByName(source,"psc");
    f->fogcolor=sceGxmProgramFindParameterByName(source,"xv_fogcolor");
    f->atest=sceGxmProgramFindParameterByName(source,"xv_atest");
    f->texscale=sceGxmProgramFindParameterByName(source,"xv_texscale");
    f->border1=sceGxmProgramFindParameterByName(source,"xv_border1");
    f->discard=sceGxmProgramIsDiscardUsed(source)!=0;f->depth_replace=sceGxmProgramIsDepthReplaceUsed(source)!=0;
    for(i=0;i<4;i++) {
        const SceGxmProgramParameter *parameter=sceGxmProgramFindParameterByName(source,names[i]);
        f->textures[i]=-1;
        if(parameter) {
            if(sceGxmProgramParameterGetCategory(parameter)!=SCE_GXM_PARAMETER_CATEGORY_SAMPLER){result=-1;goto fail;}
            f->textures[i]=sceGxmProgramParameterGetResourceIndex(parameter);
        }
    }
    return 0;
fail:
    halo_vita_fragment_destroy(f);return result;
}
