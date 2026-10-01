#include "halo_vita_fragment_program.h"
#include "halo_vita_texture_resource.h"
#include "halo_vita_texture_state.h"
#include "halo_vita_logical_targets.h"
#include "halo_vita_graphics.h"
int halo_vita_fragment_texture_bind(const struct halo_vita_fragment_program *f,unsigned stage,
    struct halo_vita_texture_resource *resource,uint32_t token,const unsigned long *state,
    SceGxmTexture *snapshot)
{
    const SceGxmTexture *source;SceGxmTexture texture;int result;
    if(!f || !f->program || !f->entry || stage>=4 || !state ||
       halo_vita_graphics_scene_active()!=2)return -1;
    if(f->textures[stage]<0)return 0; /* Compiler eliminated this sampler. */
    if(f->textures[stage]>=16 || (f->entry->cube_mask&(1u<<stage)))return -1;
    result=halo_vita_texture_resource_prepare(resource,token);if(result)return result;
    /* Temporary CPU ownership while constructing a copied descriptor. */
    result=halo_vita_texture_resource_pin(resource,token);if(result<0)return result;
    source=halo_vita_texture_resource_get(resource,token);
    if(!source){result=-1;goto done;}
    texture=*source;
    result=halo_vita_sampler_apply(state,&texture);if(result<0)goto done;
    result=halo_vita_logical_use_texture(resource,token);if(result<0)goto done;
    result=sceGxmSetFragmentTexture(halo_vita_graphics_context(),f->textures[stage],&texture);
    if(result==0 && snapshot)*snapshot=texture;
done:
    halo_vita_texture_resource_retire(resource,token);
    /* Scene ownership is retained even if the final GXM state call fails. */
    return result;
}
