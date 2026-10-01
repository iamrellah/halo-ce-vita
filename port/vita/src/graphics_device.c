#include "halo_vita_attributes.h"
#include "halo_vita_vertex_program.h"
#include "halo_vita_index_snapshot.h"
#include "halo_vita_vertex_buffer.h"
#include "halo_vita_device.h"
#include "halo_vita_graphics.h"
#include "halo_vita_surfaces.h"
#include "halo_vita_present.h"
#include "halo_vita_shader_patcher.h"
#include "halo_vita_clear_draw.h"
#include "halo_vita_texture_draw.h"
#include "halo_vita_logical_targets.h"
#include "halo_vita_backbuffer.h"
#include <string.h>

enum { STAGE_GXM=1, STAGE_CONTEXT=2, STAGE_SURFACES=4, STAGE_PATCHER=8, STAGE_CLEAR=16, STAGE_PRESENT=32, STAGE_LOGICAL=64, STAGE_BACKBUFFERS=128, STAGE_BLIT=256, STAGE_INDEX_SNAPSHOTS=512, STAGE_ATTRIBUTES=1024 };
static unsigned stages,logical_index;
static int ready;

int halo_vita_device_destroy(void)
{
    int result;
    if(halo_vita_graphics_scene_active())return -1;
    ready=0;
    /* Keep prerequisites alive on cleanup failure. Repeated destroy retries
     * the remaining stages rather than terminating GXM beneath live memory. */
    if (stages&STAGE_PRESENT) {
        result=halo_vita_present_shutdown();if(result<0)return result;
        stages&=~STAGE_PRESENT;
    }
    if(stages&STAGE_BLIT) {
        result=halo_vita_texture_draw_destroy();if(result<0)return result;
        stages&=~STAGE_BLIT;
    }
    if (stages&STAGE_CLEAR) {
        result=halo_vita_clear_destroy();if(result<0)return result;
        stages&=~STAGE_CLEAR;
    }
    if(stages&STAGE_BACKBUFFERS) {
        result=halo_vita_backbuffers_close();if(result<0)return result;
        stages&=~STAGE_BACKBUFFERS;
    }
    if(stages&STAGE_LOGICAL) {
        /* Presentation shutdown/helper teardown has drained the context.
         * Poll still performs the explicit write-pin retirement bookkeeping. */
        result=halo_vita_logical_poll(0);if(result!=0)return -1;
        result=halo_vita_logical_poll(1);if(result!=0)return -1;
        result=halo_vita_logical_targets_destroy();if(result<0)return result;
        stages&=~STAGE_LOGICAL;
    }
    if(stages&STAGE_ATTRIBUTES) {
        result=halo_vita_attribute_snapshots_destroy();if(result<0)return result;
        stages&=~STAGE_ATTRIBUTES;
    }
    if(stages&STAGE_INDEX_SNAPSHOTS) {
        result=halo_vita_index_snapshots_destroy();if(result<0)return result;
        stages&=~STAGE_INDEX_SNAPSHOTS;
    }
    /* Retire/free buffer mappings before GXM termination. Live caller refs
     * deliberately prevent teardown; release them and retry destruction. */
    result=halo_vita_vertex_buffers_collect();if(result<0)return result;
    if(!halo_vita_vertex_buffers_empty())return -1;
    if (stages&STAGE_PATCHER) {
        result=halo_vita_vertex_shaders_shutdown();if(result<0)return result;
        result=halo_vita_shader_patcher_destroy();if(result<0)return result;
        stages&=~STAGE_PATCHER;
    }
    if (stages&STAGE_SURFACES) {
        result=halo_vita_surfaces_destroy();if(result<0)return result;
        stages&=~STAGE_SURFACES;
    }
    if (stages&STAGE_CONTEXT) {
        result=halo_vita_graphics_destroy();if(result<0)return result;
        stages&=~STAGE_CONTEXT;
    }
    if (stages&STAGE_GXM) {
        result=sceGxmTerminate();if(result<0)return result;
        stages&=~STAGE_GXM;
    }
    return 0;
}

int halo_vita_device_create(void)
{
    SceGxmInitializeParams params;
    int result;
    D3DSurface *backbuffer;
    if (ready || stages || halo_vita_graphics_context() ||
        halo_vita_surface_get(0) || halo_vita_logical_target_get(0) || halo_vita_shader_patcher_get()) return -1;
    memset(&params,0,sizeof(params));
    params.parameterBufferSize=SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;
    halo_vita_present_configure(&params);
    result=sceGxmInitialize(&params);if(result<0)return result;
    stages=STAGE_GXM;
    /* Mark an attempted component before creation: it may retain resources
     * after partial-failure cleanup and must be retried by this owner. */
    stages|=STAGE_CONTEXT;result=halo_vita_graphics_create(960,544);if(result<0)goto fail;
    stages|=STAGE_SURFACES;result=halo_vita_surfaces_create(960,544);if(result<0)goto fail;
    stages|=STAGE_LOGICAL;result=halo_vita_logical_targets_create();if(result<0)goto fail;
    stages|=STAGE_INDEX_SNAPSHOTS;result=halo_vita_index_snapshots_create();if(result<0)goto fail;
    stages|=STAGE_ATTRIBUTES;result=halo_vita_attribute_snapshots_create();if(result<0)goto fail;
    stages|=STAGE_BACKBUFFERS;result=halo_vita_backbuffers_open();if(result<0)goto fail;
    stages|=STAGE_PATCHER;result=halo_vita_shader_patcher_create();if(result<0)goto fail;
    stages|=STAGE_CLEAR;result=halo_vita_clear_create();if(result<0)goto fail;
    stages|=STAGE_PRESENT;result=halo_vita_present_initialize();if(result<0)goto fail;
    result=halo_vita_backbuffer_acquire(&backbuffer);if(result<0)goto fail;
    stages|=STAGE_BLIT;result=halo_vita_texture_draw_create_d3d(backbuffer,sizeof(*backbuffer));
    D3DResource_Release((D3DResource *)backbuffer);
    if(result<0)goto fail;
    logical_index=0;ready=1;return 0;
fail:
    halo_vita_device_destroy();return result;
}

int halo_vita_device_test_frame(void)
{
    unsigned slot;
    int result,ended;
    if (!ready) return -1;
    result=halo_vita_present_begin(&slot);
    if (result) return result;
    result=halo_vita_clear_draw();
    /* Even a failed draw must close its scene before orderly teardown. */
    ended=halo_vita_present_end();
    return result<0 ? result : ended;
}
int halo_vita_device_is_ready(void) { return ready; }

int halo_vita_device_present_logical(void)
{
    D3DSurface *backbuffer;
    unsigned slot;
    int result,ended;
    if(!ready)return -1;
    if(halo_vita_graphics_scene_active()) {
        if(halo_vita_graphics_scene_active()!=2 || halo_vita_logical_active_index()!=(int)logical_index)return -1;
        result=halo_vita_logical_end();if(result<0)return result;
    }
    result=halo_vita_logical_poll(logical_index);if(result)return result;
    result=halo_vita_backbuffers_select(logical_index);if(result<0)return result;
    result=halo_vita_backbuffer_acquire(&backbuffer);if(result<0)return result;
    result=halo_vita_texture_draw_bind_d3d(backbuffer,sizeof(*backbuffer));
    D3DResource_Release((D3DResource *)backbuffer);
    if(result<0)return result;
    result=halo_vita_present_begin(&slot);if(result)return result;
    result=halo_vita_texture_draw_submit();
    ended=halo_vita_present_end();
    if(result<0)return result;
    if(ended<0)return ended;
    logical_index^=1;
    return halo_vita_backbuffers_select(logical_index);
}
