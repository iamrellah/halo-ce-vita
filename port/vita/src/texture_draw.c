#include "halo_vita_texture_draw.h"
#include "halo_vita_d3d_view.h"
#include "halo_vita_allocation.h"
#include "halo_vita_graphics.h"
#include "halo_vita_present.h"
#include "halo_vita_texture_upload.h"
#include "halo_vita_shader_patcher.h"
#include <psp2/kernel/sysmem.h>
#include <stdint.h>
#include <string.h>
#include "../../../build/vita/clear_gxp.h"

static SceGxmShaderPatcherId vertex_id,fragment_id;
static SceGxmVertexProgram *vertex;
static SceGxmFragmentProgram *fragment;
static SceUID geometry_uid=-1;
static void *geometry;
static int mapped,retained,vertex_registered,fragment_registered,ready,texture_pinned;
static struct halo_vita_texture_upload texture=HALO_VITA_TEXTURE_UPLOAD_INITIALIZER;
static struct halo_vita_texture_resource *borrowed;
static uint32_t borrowed_token,borrowed_allocation;
static SceGxmTexture bound_texture;
static const SceGxmProgramParameter *matrix_parameter,*alpha_parameter;
static unsigned sampler_index;
struct clear_vertex { float x,y,z,u,v; uint32_t color; };

int halo_vita_texture_draw_destroy(void)
{
    SceGxmShaderPatcher *p=halo_vita_shader_patcher_get();
    int result;
    ready=0;
    /* Shutdown only, caller has ended scenes and detached presentation. */
    if (halo_vita_graphics_context()) sceGxmFinish(halo_vita_graphics_context());
    if (borrowed) { result=halo_vita_texture_resource_retire(borrowed,borrowed_token);if(result<0)return result;borrowed=NULL;borrowed_token=0; }
    if (borrowed_allocation) { result=halo_vita_allocation_release(borrowed_allocation);if(result<0)return result;borrowed_allocation=0; }
    if (texture_pinned) { result=halo_vita_texture_upload_retire(&texture);if(result<0)return result;texture_pinned=0; }
    result=halo_vita_texture_upload_destroy(&texture);if(result<0)return result;
    if (fragment) { result=sceGxmShaderPatcherReleaseFragmentProgram(p,fragment);if(result<0)return result;fragment=NULL; }
    if (vertex) { result=sceGxmShaderPatcherReleaseVertexProgram(p,vertex);if(result<0)return result;vertex=NULL; }
    if (fragment_registered) { result=sceGxmShaderPatcherUnregisterProgram(p,fragment_id);if(result<0)return result;fragment_registered=0; }
    if (vertex_registered) { result=sceGxmShaderPatcherUnregisterProgram(p,vertex_id);if(result<0)return result;vertex_registered=0; }
    if (mapped) { result=sceGxmUnmapMemory(geometry);if(result<0)return result;mapped=0; }
    if (geometry_uid>=0) { result=sceKernelFreeMemBlock(geometry_uid);if(result<0)return result;geometry_uid=-1;geometry=NULL; }
    if (retained) { result=halo_vita_shader_patcher_release();if(result<0)return result;retained=0; }
    return 0;
}

static int create_draw(const struct halo_vita_texture_view *view,
    struct halo_vita_texture_resource *resource,uint32_t token,const void *header,size_t header_bytes)
{
    SceGxmShaderPatcher *p=halo_vita_shader_patcher_get();
    const SceGxmProgram *vs=(const SceGxmProgram *)native_texture_vs;
    const SceGxmProgram *fs=(const SceGxmProgram *)native_texture_fs;
    const SceGxmProgramParameter *position,*color,*uv,*sampler;
    SceGxmVertexAttribute attributes[3];
    SceGxmVertexStream stream;
    struct clear_vertex vertices[3]={{-1,-1,0,0,1,0xffffffff},{3,-1,0,2,1,0xffffffff},{-1,3,0,0,-1,0xffffffff}};
    uint16_t indices[3]={0,1,2};
    int result;
    if ((!view && !resource && !header) || ready || retained || borrowed || borrowed_allocation || geometry_uid>=0 || texture.uid>=0 || !p) return -1;
    if (resource) { result=halo_vita_texture_resource_prepare(resource,token);if(result!=0)return result; }
    if (sceGxmProgramCheck(vs)<0 || sceGxmProgramCheck(fs)<0 ||
        sceGxmProgramGetSize(vs)>sizeof(native_texture_vs) ||
        sceGxmProgramGetSize(fs)>sizeof(native_texture_fs)) return -1;
    position=sceGxmProgramFindParameterByName(vs,"IN.position");
    color=sceGxmProgramFindParameterByName(vs,"IN.color0");
    uv=sceGxmProgramFindParameterByName(vs,"IN.texcoord0");
    matrix_parameter=sceGxmProgramFindParameterByName(vs,"c");
    alpha_parameter=sceGxmProgramFindParameterByName(fs,"xv_atest");
    sampler=sceGxmProgramFindParameterByName(fs,"tex0");
    if (!position || !color || !uv || !matrix_parameter || !alpha_parameter || !sampler) return -1;
    sampler_index=sceGxmProgramParameterGetResourceIndex(sampler);
    if (halo_vita_shader_patcher_retain()<0) return -1;
    retained=1;
    result=sceGxmShaderPatcherRegisterProgram(p,vs,&vertex_id);if(result<0)goto fail;vertex_registered=1;
    result=sceGxmShaderPatcherRegisterProgram(p,fs,&fragment_id);if(result<0)goto fail;fragment_registered=1;
    memset(attributes,0,sizeof(attributes));
    attributes[0].offset=0;attributes[0].format=SCE_GXM_ATTRIBUTE_FORMAT_F32;
    attributes[0].componentCount=3;attributes[0].regIndex=sceGxmProgramParameterGetResourceIndex(position);
    attributes[1].offset=20;attributes[1].format=SCE_GXM_ATTRIBUTE_FORMAT_U8N;
    attributes[1].componentCount=4;attributes[1].regIndex=sceGxmProgramParameterGetResourceIndex(color);
    attributes[2].offset=12;attributes[2].format=SCE_GXM_ATTRIBUTE_FORMAT_F32;
    attributes[2].componentCount=2;attributes[2].regIndex=sceGxmProgramParameterGetResourceIndex(uv);
    memset(&stream,0,sizeof(stream));stream.stride=sizeof(struct clear_vertex);stream.indexSource=SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
    result=sceGxmShaderPatcherCreateVertexProgram(p,vertex_id,attributes,3,&stream,1,&vertex);if(result<0)goto fail;
    result=sceGxmShaderPatcherCreateFragmentProgram(p,fragment_id,SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE,NULL,vs,&fragment);if(result<0)goto fail;
    geometry_uid=sceKernelAllocMemBlock("halo clear geometry",SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,4096,NULL);
    if(geometry_uid<0){result=geometry_uid;goto fail;}
    result=sceKernelGetMemBlockBase(geometry_uid,&geometry);if(result<0)goto fail;
    memset(geometry,0,4096);memcpy(geometry,vertices,sizeof(vertices));
    memcpy((char *)geometry+128,indices,sizeof(indices));
    __asm__ volatile("dsb sy" ::: "memory");
    result=sceGxmMapMemory(geometry,4096,SCE_GXM_MEMORY_ATTRIB_READ);if(result<0)goto fail;
    mapped=1;
    if(header) {
        struct halo_vita_d3d_view acquired;
        result=halo_vita_d3d_texture_acquire(header,header_bytes,&acquired,&bound_texture);
        if(result<0)goto fail;
        borrowed_allocation=acquired.allocation;
    } else if (resource) {
        const SceGxmTexture *descriptor;
        result=halo_vita_texture_resource_pin(resource,token);if(result<0)goto fail;
        borrowed=resource;borrowed_token=token;
        descriptor=halo_vita_texture_resource_get(resource,token);
        if (!descriptor) { result=-1;goto fail; }
        bound_texture=*descriptor;
    } else {
        result=halo_vita_texture_upload_create(&texture,view);if(result<0)goto fail;
        result=halo_vita_texture_upload_pin(&texture);if(result<0)goto fail;texture_pinned=1;
        bound_texture=texture.texture;
    }
    /* Sampler state is a draw-local copy, not a mutation of shared resource. */
    result=sceGxmTextureSetMinFilter(&bound_texture,SCE_GXM_TEXTURE_FILTER_POINT);if(result<0)goto fail;
    result=sceGxmTextureSetMagFilter(&bound_texture,SCE_GXM_TEXTURE_FILTER_POINT);if(result<0)goto fail;
    result=sceGxmTextureSetUAddrMode(&bound_texture,SCE_GXM_TEXTURE_ADDR_CLAMP);if(result<0)goto fail;
    result=sceGxmTextureSetVAddrMode(&bound_texture,SCE_GXM_TEXTURE_ADDR_CLAMP);if(result<0)goto fail;
    ready=1;return 0;
fail:
    halo_vita_texture_draw_destroy();return result;
}

int halo_vita_texture_draw_create(const struct halo_vita_texture_view *view)
{
    return create_draw(view,NULL,0,NULL,0);
}
int halo_vita_texture_draw_create_resource(struct halo_vita_texture_resource *resource,uint32_t token)
{
    return create_draw(NULL,resource,token,NULL,0);
}
int halo_vita_texture_draw_create_d3d(const void *header,size_t bytes)
{
    return create_draw(NULL,NULL,0,header,bytes);
}
int halo_vita_texture_draw_bind_d3d(const void *header,size_t bytes)
{
    struct halo_vita_d3d_view view;
    SceGxmTexture descriptor;
    int result;
    if(!ready || !borrowed_allocation || halo_vita_graphics_scene_active())return -1;
    result=halo_vita_d3d_texture_acquire(header,bytes,&view,&descriptor);
    if(result<0)return result;
    result=halo_vita_allocation_release(borrowed_allocation);
    if(result<0){halo_vita_allocation_release(view.allocation);return result;}
    /* Submitted commands retain GPU pins; changing this CPU descriptor does
     * not retire prior reads or require recreating shader/geometry resources. */
    borrowed_allocation=view.allocation;bound_texture=descriptor;return 0;
}
int halo_vita_texture_draw_submit(void)
{
    SceGxmContext *context=halo_vita_graphics_context();
    int result;
    void *uniforms;
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float alpha[4]={0,7,0,0};
    if (!ready || !context) return -1;
    if(borrowed_allocation) {
        result=halo_vita_present_use_allocation(borrowed_allocation);
        if(result<0)return result;
    }
    if (borrowed) {
        result=halo_vita_present_use_texture(borrowed,borrowed_token);
        if (result<0) return result;
    }
    sceGxmSetViewport(context,480,480,272,-272,0.5f,0.5f);
    sceGxmSetCullMode(context,SCE_GXM_CULL_NONE);
    sceGxmSetFrontDepthFunc(context,SCE_GXM_DEPTH_FUNC_ALWAYS);
    sceGxmSetFrontDepthWriteEnable(context,SCE_GXM_DEPTH_WRITE_DISABLED);
    /* Fullscreen helpers must not inherit a prior scene's stencil rejection
     * or replacement operation. They never modify the destination stencil. */
    sceGxmSetTwoSidedEnable(context,SCE_GXM_TWO_SIDED_DISABLED);
    sceGxmSetFrontStencilFunc(context,SCE_GXM_STENCIL_FUNC_ALWAYS,
        SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
        SCE_GXM_STENCIL_OP_KEEP,0xff,0);
    sceGxmSetVertexProgram(context,vertex);sceGxmSetFragmentProgram(context,fragment);
    result=sceGxmReserveVertexDefaultUniformBuffer(context,&uniforms);if(result<0)return result;
    result=sceGxmSetUniformDataF(uniforms,matrix_parameter,0,16,identity);if(result<0)return result;
    result=sceGxmReserveFragmentDefaultUniformBuffer(context,&uniforms);if(result<0)return result;
    result=sceGxmSetUniformDataF(uniforms,alpha_parameter,0,4,alpha);if(result<0)return result;
    result=sceGxmSetFragmentTexture(context,sampler_index,&bound_texture);if(result<0)return result;
    result=sceGxmSetVertexStream(context,0,geometry);if(result<0)return result;
    return sceGxmDraw(context,SCE_GXM_PRIMITIVE_TRIANGLES,SCE_GXM_INDEX_FORMAT_U16,(char *)geometry+128,3);
}
