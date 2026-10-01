#include "halo_vita_clear_draw.h"
#include "halo_vita_d3d_state.h"
#include "halo_vita_topology.h"
#include "halo_vita_streams.h"
#include "halo_vita_index_snapshot.h"
#include "halo_vita_buffer.h"
#include "halo_vita_allocation.h"
#include "halo_vita_graphics.h"
#include "halo_vita_logical_targets.h"
#include "halo_vita_shader_patcher.h"
#include <psp2/kernel/sysmem.h>
#include <stdint.h>
#include <string.h>
#include "../../../build/vita/clear_gxp.h"

static SceGxmShaderPatcherId vertex_id,fragment_id;
static SceGxmVertexProgram *vertex;
static SceGxmFragmentProgram *fragment;
static SceGxmFragmentProgram *masked_fragments[15];
/* Bounded test/program variants: never evict while queued GPU draws can use
 * one. Production shader caches need their own retirement-aware budget. */
static struct { SceGxmBlendInfo key; SceGxmFragmentProgram *program; } blend_variants[32];
static unsigned clear_counts[2];
#define CLEAR_RING_BYTES 65536
#define CLEAR_RECORD_BYTES 64
static struct halo_vita_buffer geometry_owner={.uid=-1};
#define geometry_uid geometry_owner.uid
#define geometry geometry_owner.base
static int retained,vertex_registered,fragment_registered,ready;
struct clear_vertex { float x,y,z; uint32_t color; };

int halo_vita_clear_destroy(void)
{
    SceGxmShaderPatcher *p=halo_vita_shader_patcher_get();
    int result;
    unsigned i;
    if(halo_vita_graphics_scene_active())return -1;
    ready=0;
    /* Shutdown only, caller has ended scenes and detached presentation. */
    if (halo_vita_graphics_context()) sceGxmFinish(halo_vita_graphics_context());
    for(i=0;i<2;i++)if(halo_vita_logical_poll(i)!=0)return -1;
    for(i=0;i<32;i++)if(blend_variants[i].program) {
        result=sceGxmShaderPatcherReleaseFragmentProgram(p,blend_variants[i].program);
        if(result<0)return result;blend_variants[i].program=NULL;
    }
    for(i=0;i<15;i++)if(masked_fragments[i]) {
        result=sceGxmShaderPatcherReleaseFragmentProgram(p,masked_fragments[i]);
        if(result<0)return result;masked_fragments[i]=NULL;
    }
    if (fragment) { result=sceGxmShaderPatcherReleaseFragmentProgram(p,fragment);if(result<0)return result;fragment=NULL; }
    if (vertex) { result=sceGxmShaderPatcherReleaseVertexProgram(p,vertex);if(result<0)return result;vertex=NULL; }
    if (fragment_registered) { result=sceGxmShaderPatcherUnregisterProgram(p,fragment_id);if(result<0)return result;fragment_registered=0; }
    if (vertex_registered) { result=sceGxmShaderPatcherUnregisterProgram(p,vertex_id);if(result<0)return result;vertex_registered=0; }
    result=halo_vita_buffer_destroy(&geometry_owner);if(result<0)return result;
    if (retained) { result=halo_vita_shader_patcher_release();if(result<0)return result;retained=0; }
    return 0;
}

int halo_vita_clear_create(void)
{
    SceGxmShaderPatcher *p=halo_vita_shader_patcher_get();
    const SceGxmProgram *vs=(const SceGxmProgram *)native_clear_vs;
    const SceGxmProgram *fs=(const SceGxmProgram *)native_clear_fs;
    const SceGxmProgramParameter *position,*color;
    SceGxmVertexAttribute attributes[2];
    SceGxmVertexStream stream;
    struct clear_vertex vertices[3]={{-1,-1,0,0xff204080},{3,-1,0,0xff204080},{-1,3,0,0xff204080}};
    uint16_t indices[4]={0,1,2,3},quad_indices[6];
    size_t quad_count;
    SceGxmPrimitiveType quad_mode;
    int result;
    if (ready || retained || geometry_uid>=0 || !p) return -1;
    if (sceGxmProgramCheck(vs)<0 || sceGxmProgramCheck(fs)<0 ||
        sceGxmProgramGetSize(vs)>sizeof(native_clear_vs) ||
        sceGxmProgramGetSize(fs)>sizeof(native_clear_fs)) return -1;
    position=sceGxmProgramFindParameterByName(vs,"IN.position");
    color=sceGxmProgramFindParameterByName(vs,"IN.color0");
    if (!position || !color) return -1;
    if (halo_vita_shader_patcher_retain()<0) return -1;
    retained=1;
    result=sceGxmShaderPatcherRegisterProgram(p,vs,&vertex_id);if(result<0)goto fail;vertex_registered=1;
    result=sceGxmShaderPatcherRegisterProgram(p,fs,&fragment_id);if(result<0)goto fail;fragment_registered=1;
    memset(attributes,0,sizeof(attributes));
    attributes[0].offset=0;attributes[0].format=SCE_GXM_ATTRIBUTE_FORMAT_F32;
    attributes[0].componentCount=3;attributes[0].regIndex=sceGxmProgramParameterGetResourceIndex(position);
    attributes[1].offset=12;attributes[1].format=SCE_GXM_ATTRIBUTE_FORMAT_U8N;
    attributes[1].componentCount=4;attributes[1].regIndex=sceGxmProgramParameterGetResourceIndex(color);
    memset(&stream,0,sizeof(stream));stream.stride=sizeof(struct clear_vertex);stream.indexSource=SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
    result=sceGxmShaderPatcherCreateVertexProgram(p,vertex_id,attributes,2,&stream,1,&vertex);if(result<0)goto fail;
    result=sceGxmShaderPatcherCreateFragmentProgram(p,fragment_id,SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE,NULL,vs,&fragment);if(result<0)goto fail;
    result=halo_vita_buffer_create(&geometry_owner,4096+2*CLEAR_RING_BYTES);if(result<0)goto fail;
    memcpy(geometry,vertices,sizeof(vertices));
    memcpy((char *)geometry+64,indices,sizeof(indices));
    result=halo_vita_topology(D3DPT_QUADLIST,NULL,4,quad_indices,6,&quad_mode,&quad_count);
    if(result<0 || quad_count!=6 || quad_mode!=SCE_GXM_PRIMITIVE_TRIANGLES)goto fail;
    memcpy((char *)geometry+128,quad_indices,sizeof(quad_indices));
    __asm__ volatile("dsb sy" ::: "memory");
    ready=1;return 0;
fail:
    halo_vita_clear_destroy();return result;
}

int halo_vita_clear_draw_size(unsigned width,unsigned height)
{
    SceGxmContext *context=halo_vita_graphics_context();
    int result;
    if (!ready || !context || !width || !height || width>960 || height>544) return -1;
    sceGxmSetViewport(context,width*0.5f,width*0.5f,height*0.5f,-(height*0.5f),0.5f,0.5f);
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
    result=sceGxmSetVertexStream(context,0,geometry);if(result<0)return result;
    return sceGxmDraw(context,SCE_GXM_PRIMITIVE_TRIANGLES,SCE_GXM_INDEX_FORMAT_U16,(char *)geometry+64,3);
}

int halo_vita_clear_draw(void) { return halo_vita_clear_draw_size(960,544); }

void halo_vita_clear_scene_reset(unsigned slot)
{
    /* Called only after the logical target's previous write fence retired. */
    if(slot<2)clear_counts[slot]=0;
}
int halo_vita_clear_native(unsigned flags,uint32_t color,float z,unsigned stencil)
{
    int slot=halo_vita_logical_active_index(),result;
    unsigned mask=0;
    SceGxmContext *context=halo_vita_graphics_context();
    SceGxmFragmentProgram *program;
    struct clear_vertex *q;
    if(!ready || !context || slot<0 || slot>=2 || flags&~0xf3u || stencil>255 ||
       ((flags&1) && !(z>=0 && z<=1)))return -1;
    if(!flags)return 0;
    if(clear_counts[slot]>=CLEAR_RING_BYTES/CLEAR_RECORD_BYTES)return -1;
    if(flags&0x80)mask|=SCE_GXM_COLOR_MASK_A;
    if(flags&0x10)mask|=SCE_GXM_COLOR_MASK_R;
    if(flags&0x20)mask|=SCE_GXM_COLOR_MASK_G;
    if(flags&0x40)mask|=SCE_GXM_COLOR_MASK_B;
    program=fragment;
    if(mask!=15) {
        if(!masked_fragments[mask]) {
            SceGxmBlendInfo blend;
            memset(&blend,0,sizeof(blend));blend.colorMask=mask;
            result=sceGxmShaderPatcherCreateFragmentProgram(halo_vita_shader_patcher_get(),fragment_id,
                SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,SCE_GXM_MULTISAMPLE_NONE,&blend,
                (const SceGxmProgram *)native_clear_vs,&masked_fragments[mask]);
            if(result<0)return result;
        }
        program=masked_fragments[mask];
    }
    q=(struct clear_vertex *)((char *)geometry+4096+slot*CLEAR_RING_BYTES+
        clear_counts[slot]++*CLEAR_RECORD_BYTES);
    z=(flags&1)?2.0f*z-1.0f:1.0f;
    q[0]=(struct clear_vertex){-1,-1,z,color};q[1]=(struct clear_vertex){1,-1,z,color};
    q[2]=(struct clear_vertex){1,1,z,color};q[3]=(struct clear_vertex){-1,1,z,color};
    __asm__ volatile("dsb sy" ::: "memory");
    sceGxmSetCullMode(context,SCE_GXM_CULL_NONE);
    sceGxmSetTwoSidedEnable(context,SCE_GXM_TWO_SIDED_DISABLED);
    sceGxmSetFrontDepthFunc(context,SCE_GXM_DEPTH_FUNC_ALWAYS);
    sceGxmSetFrontDepthWriteEnable(context,(flags&1)?SCE_GXM_DEPTH_WRITE_ENABLED:SCE_GXM_DEPTH_WRITE_DISABLED);
    sceGxmSetFrontStencilRef(context,stencil);
    sceGxmSetFrontStencilFunc(context,SCE_GXM_STENCIL_FUNC_ALWAYS,SCE_GXM_STENCIL_OP_KEEP,
        SCE_GXM_STENCIL_OP_KEEP,(flags&2)?SCE_GXM_STENCIL_OP_REPLACE:SCE_GXM_STENCIL_OP_KEEP,
        0xff,(flags&2)?0xff:0);
    sceGxmSetVertexProgram(context,vertex);sceGxmSetFragmentProgram(context,program);
    result=sceGxmSetVertexStream(context,0,q);if(result<0)return result;
    return sceGxmDraw(context,SCE_GXM_PRIMITIVE_TRIANGLE_FAN,SCE_GXM_INDEX_FORMAT_U16,(char *)geometry+64,4);
}

/* Qualification quad: exercises captured blend/depth/stencil state with the
 * color shader. This is not an implementation of Halo's general draw API. */
static int color_submit(uint32_t color,float clip_z,const struct halo_vita_stream_view *view,
    const struct halo_vita_index_snapshot *indices)
{
    int slot=halo_vita_logical_active_index(),result;
    unsigned i,free_slot=32;
    SceGxmBlendInfo blend;
    struct halo_vita_depth_stencil depth;
    struct clear_vertex *q;
    SceGxmFragmentProgram *program=NULL;
    SceGxmContext *context=halo_vita_graphics_context();
    if(!ready || !context || slot<0 || slot>=2 || !(clip_z>=-1 && clip_z<=1) ||
       (!view && clear_counts[slot]>=CLEAR_RING_BYTES/CLEAR_RECORD_BYTES) ||
       halo_vita_blend_decode(D3D__RenderState,&blend)<0 ||
       halo_vita_depth_stencil_decode(D3D__RenderState,&depth)<0)return -1;
    for(i=0;i<32;i++) {
        if(!blend_variants[i].program) { if(free_slot==32)free_slot=i; }
        else if(!memcmp(&blend_variants[i].key,&blend,sizeof(blend))) {
            program=blend_variants[i].program;break;
        }
    }
    if(!program) {
        if(free_slot==32)return -1;
        result=sceGxmShaderPatcherCreateFragmentProgram(halo_vita_shader_patcher_get(),fragment_id,
            SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,SCE_GXM_MULTISAMPLE_NONE,&blend,
            (const SceGxmProgram *)native_clear_vs,&program);
        if(result<0)return result;
        blend_variants[free_slot].key=blend;blend_variants[free_slot].program=program;
    }
    if(view) {
        if(view->stride!=sizeof(struct clear_vertex) || view->bytes!=(indices?(size_t)indices->maximum+1:4)*sizeof(struct clear_vertex))return -1;
        result=halo_vita_logical_use_allocation(view->allocation);if(result)return result;
    }
    result=halo_vita_allocation_retain(geometry_owner.allocation);if(result<0)return result;
    result=halo_vita_logical_use_allocation(geometry_owner.allocation);
    halo_vita_allocation_release(geometry_owner.allocation);
    if(result)return result;
    result=halo_vita_depth_stencil_apply();if(result<0)return result;
    if(view)q=(struct clear_vertex *)view->data;
    else {
    q=(struct clear_vertex *)((char *)geometry+4096+slot*CLEAR_RING_BYTES+
        clear_counts[slot]++*CLEAR_RECORD_BYTES);
    q[0]=(struct clear_vertex){-1,-1,clip_z,color};q[1]=(struct clear_vertex){1,-1,clip_z,color};
    q[2]=(struct clear_vertex){1,1,clip_z,color};q[3]=(struct clear_vertex){-1,1,clip_z,color};
    }
    __asm__ volatile("dsb sy" ::: "memory");
    result=halo_vita_cull_apply();if(result<0)return result;
    sceGxmSetVertexProgram(context,vertex);sceGxmSetFragmentProgram(context,program);
    result=sceGxmSetVertexStream(context,0,q);if(result<0)return result;
    return sceGxmDraw(context,indices?indices->primitive:SCE_GXM_PRIMITIVE_TRIANGLES,
        SCE_GXM_INDEX_FORMAT_U16,indices?(const void *)indices->data:(char *)geometry+128,
        indices?indices->count:6);
}

int halo_vita_color_quad(uint32_t color,float clip_z)
{
    return color_submit(color,clip_z,NULL,NULL);
}
int halo_vita_color_stream(unsigned stream,size_t first)
{
    struct halo_vita_stream_view view;
    int result=halo_vita_stream_acquire(stream,first,4,sizeof(struct clear_vertex),&view);
    if(result<0)return result;
    result=color_submit(0,0,&view,NULL);
    if(halo_vita_allocation_release(view.allocation)<0)return -1;
    return result;
}

int halo_vita_color_indexed(unsigned stream,D3DPRIMITIVETYPE type,size_t first,size_t count)
{
    struct halo_vita_index_snapshot indices;
    struct halo_vita_stream_view view;
    int result=halo_vita_index_snapshot(type,first,count,&indices);
    if(result)return result;
    /* Keep 16-bit indices unchanged; apply base vertex to the stream pointer.
     * Snapshot already checked base+maximum overflow. */
    result=halo_vita_stream_acquire(stream,indices.base_vertex,(size_t)indices.maximum+1,
        sizeof(struct clear_vertex),&view);
    if(result<0)return result;
    result=color_submit(0,0,&view,&indices);
    if(halo_vita_allocation_release(view.allocation)<0)return -1;
    return result;
}
