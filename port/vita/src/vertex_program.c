/* Attribute binding adapted from Xita xv_shader.c; retain GPL licensing. */
#include "halo_vita_vertex_program.h"
#include "halo_vita_shader_patcher.h"
#include <stdio.h>
#include <string.h>
#include "../../../build/vita/native_vertex_catalog.h"
int halo_vita_vertex_program_destroy(struct halo_vita_vertex_program *v)
{
    SceGxmShaderPatcher *p=halo_vita_shader_patcher_get();int result;
    if(!v)return -1;
    if(v->program){result=sceGxmShaderPatcherReleaseVertexProgram(p,v->program);if(result<0)return result;v->program=NULL;}
    if(v->registered){result=sceGxmShaderPatcherUnregisterProgram(p,v->id);if(result<0)return result;v->registered=0;}
    if(v->retained){result=halo_vita_shader_patcher_release();if(result<0)return result;v->retained=0;}
    memset(v,0,sizeof(*v));return 0;
}
int halo_vita_vertex_catalog_load(unsigned index,struct halo_vita_vertex_program *v)
{
    const xv_vs_desc_t *d;
    const SceGxmProgram *source;
    const SceGxmProgramParameter *constants;
    SceGxmVertexAttribute attrs[16];SceGxmVertexStream streams[5];
    unsigned i,n=0,constant=0,stream_mask=0;int result;
    SceGxmShaderPatcher *p=halo_vita_shader_patcher_get();
    if(!v || !p || index>=67 || v->program || v->retained || v->registered)return -1;
    d=native_vertex_catalog[index].desc;source=(const SceGxmProgram *)native_vertex_catalog[index].data;
    if(sceGxmProgramCheck(source)<0 || sceGxmProgramGetSize(source)>native_vertex_catalog[index].bytes ||
       sceGxmProgramGetType(source)!=SCE_GXM_VERTEX_PROGRAM ||
       !d->nstreams || d->nstreams>4 || d->nattrs>16 || d->c_base < -96 ||
       d->c_base>95 || d->c_count>96-d->c_base)return -1;
    constants=sceGxmProgramFindParameterByName(source,"c");
    /* A missing constant bank is legal when the compiler eliminated it. */
    if(constants && (sceGxmProgramParameterGetCategory(constants)!=SCE_GXM_PARAMETER_CATEGORY_UNIFORM ||
       sceGxmProgramParameterGetType(constants)!=SCE_GXM_PARAMETER_TYPE_F32 ||
       sceGxmProgramParameterGetComponentCount(constants)!=4 ||
       sceGxmProgramParameterGetArraySize(constants)<d->c_count))return -1;
    memset(attrs,0,sizeof(attrs));memset(streams,0,sizeof(streams));
    for(i=0;i<d->nattrs;i++) {
        const xv_attr_desc_t *a=&d->attrs[i];
        const SceGxmProgramParameter *parameter;
        char name[64];unsigned bytes;
        if(!a->name || a->vreg>=16 || !a->components || a->components>4)return -1;
        switch(a->format) {
        case SCE_GXM_ATTRIBUTE_FORMAT_F32:bytes=4;break;
        case SCE_GXM_ATTRIBUTE_FORMAT_S16:case SCE_GXM_ATTRIBUTE_FORMAT_S16N:bytes=2;break;
        case SCE_GXM_ATTRIBUTE_FORMAT_U8:case SCE_GXM_ATTRIBUTE_FORMAT_U8N:bytes=1;break;
        default:return -1;
        }
        if(a->stream!=XV_CONST_STREAM && (a->stream>=d->nstreams ||
           a->offset+bytes*a->components>d->stride[a->stream]))return -1;
        if(snprintf(name,sizeof(name),"IN.%s",a->name)>=(int)sizeof(name))return -1;
        parameter=sceGxmProgramFindParameterByName(source,name);
        if(!parameter)parameter=sceGxmProgramFindParameterByName(source,a->name);
        if(!parameter)continue; /* Compiler may eliminate unused input. */
        if(sceGxmProgramParameterGetCategory(parameter)!=SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE)return -1;
        { unsigned j,reg=sceGxmProgramParameterGetResourceIndex(parameter);
          for(j=0;j<n;j++)if(attrs[j].regIndex==reg)return -1; }
        attrs[n].streamIndex=a->stream==XV_CONST_STREAM?d->nstreams:a->stream;
        attrs[n].offset=a->stream==XV_CONST_STREAM?a->vreg*16:a->offset;
        attrs[n].format=a->format;attrs[n].componentCount=a->components;
        attrs[n].regIndex=sceGxmProgramParameterGetResourceIndex(parameter);n++;
        if(a->stream==XV_CONST_STREAM)constant=1;
        else stream_mask|=1u<<a->stream;
    }
    for(i=0;i<d->nstreams;i++) {
        if(!d->stride[i])return -1;
        streams[i].stride=d->stride[i];streams[i].indexSource=SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
    }
    if(constant){streams[i].stride=256;streams[i].indexSource=SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT;}
    result=halo_vita_shader_patcher_retain();if(result<0)return result;v->retained=1;
    result=sceGxmShaderPatcherRegisterProgram(p,source,&v->id);if(result<0)goto fail;v->registered=1;
    result=sceGxmShaderPatcherCreateVertexProgram(p,v->id,attrs,n,streams,d->nstreams+constant,&v->program);
    if(result<0)goto fail;
    v->desc=d;v->source=source;v->constants=constants;
    v->constant_stream=constant?d->nstreams:255;v->attributes=n;v->stream_mask=stream_mask;return 0;
fail:
    halo_vita_vertex_program_destroy(v);return result;
}

int halo_vita_vertex_catalog_find(const void *program,unsigned program_bytes,
    const void *declaration,unsigned declaration_bytes,unsigned *index)
{
    unsigned i;
    if(!program || !declaration || !index || !program_bytes || !declaration_bytes)return -1;
    for(i=0;i<67;i++) {
        if(program_bytes!=native_vertex_catalog[i].original_bytes ||
           declaration_bytes!=native_vertex_catalog[i].declaration_bytes)continue;
        if(!memcmp(program,native_vertex_catalog[i].original,program_bytes) &&
           !memcmp(declaration,native_vertex_catalog[i].declaration,declaration_bytes)) {
            *index=i;return 0;
        }
    }
    return -1;
}

int halo_vita_vertex_catalog_pair(unsigned program,unsigned declaration,unsigned *out)
{
    if(program>=67 || declaration>=67)return -1;
    return halo_vita_vertex_catalog_find(native_vertex_catalog[program].original,
        native_vertex_catalog[program].original_bytes,native_vertex_catalog[declaration].declaration,
        native_vertex_catalog[declaration].declaration_bytes,out);
}
