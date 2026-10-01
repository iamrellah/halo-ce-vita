#include "halo_vita_attributes.h"
#include "halo_vita_vertex_program.h"
#include "halo_vita_streams.h"
#include "halo_vita_allocation.h"
#include "halo_vita_logical_targets.h"
#include "halo_vita_graphics.h"
#include "halo_vita_constants.h"
#include <string.h>

int halo_vita_vertex_shader_bind(size_t first,size_t count,
    const struct halo_vita_stream_view *persistent)
{
    const struct halo_vita_vertex_program *v;
    struct halo_vita_stream_view views[4],owned_snapshot={0};
    SceGxmContext *context=halo_vita_graphics_context();
    uint32_t persistent_id=0;unsigned i;int result;void *uniforms;
    if(!context || halo_vita_graphics_scene_active()!=2 || !count)return -1;
    result=halo_vita_vertex_shader_current(&v);if(result<0)return result;
    memset(views,0,sizeof(views));
    for(i=0;i<v->desc->nstreams;i++)if(v->stream_mask&(1u<<i)) {
        result=halo_vita_stream_acquire(i,first,count,v->desc->stride[i],&views[i]);
        if(result<0)goto done;
        /* GXM stride is baked into the patched vertex program. */
        if(views[i].stride!=v->desc->stride[i]){result=-1;goto done;}
    }
    if(v->constant_stream!=255) {
        if(!persistent) {
            result=halo_vita_attributes_snapshot(&owned_snapshot);if(result<0)goto done;
            persistent=&owned_snapshot;
        }
        /* Caller provides an immutable 16-register snapshot for this draw.
         * Validate it independently of caller-supplied allocation metadata. */
        if(!persistent || persistent->bytes<256 || persistent->stride!=256 ||
           halo_vita_allocation_resolve(persistent->data,256,HALO_VITA_ALLOCATION_VERTEX_BYTES,&persistent_id)<0 ||
           persistent_id!=persistent->allocation) {persistent_id=0;result=-1;goto done;}
        result=halo_vita_allocation_retain(persistent_id);
        if(result<0){persistent_id=0;goto done;}
    }
    for(i=0;i<4;i++)if(views[i].allocation) {
        result=halo_vita_logical_use_allocation(views[i].allocation);if(result)goto done;
    }
    if(persistent_id) {
        result=halo_vita_logical_use_allocation(persistent_id);if(result)goto done;
    }
    sceGxmSetVertexProgram(context,v->program);
    if(v->constants && v->desc->c_count) {
        result=sceGxmReserveVertexDefaultUniformBuffer(context,&uniforms);if(result<0)goto done;
        result=halo_vita_constants_upload(uniforms,v->constants,v->desc->c_base,v->desc->c_count);
        if(result<0)goto done;
    }
    for(i=0;i<4;i++)if(views[i].allocation) {
        result=sceGxmSetVertexStream(context,i,views[i].data);if(result<0)goto done;
    }
    if(persistent_id) {
        result=sceGxmSetVertexStream(context,v->constant_stream,persistent->data);if(result<0)goto done;
    }
    result=0;
done:
    for(i=0;i<4;i++)if(views[i].allocation)halo_vita_allocation_release(views[i].allocation);
    if(persistent_id)halo_vita_allocation_release(persistent_id);
    if(owned_snapshot.allocation)halo_vita_allocation_release(owned_snapshot.allocation);
    /* Any pins already acquired remain owned by this scene until retirement,
     * even when later state setup fails. No failed setup authorizes a draw. */
    return result;
}
