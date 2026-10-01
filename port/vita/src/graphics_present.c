#include "halo_vita_present.h"
#include "halo_vita_graphics.h"
#include "halo_vita_surfaces.h"
#include "halo_vita_allocation.h"
#include "halo_vita_logical_targets.h"
#include <psp2/display.h>
#include <string.h>

struct display_entry { SceDisplayFrameBuf frame; unsigned old_slot; };
enum { FREE, HELD, QUEUED, RETIRED };
static unsigned states[3], pinned[3], tail, cursor;
static int active=-1, ready, failure;
#define SCENE_TEXTURE_LIMIT 512
struct texture_reference { struct halo_vita_texture_resource *resource; uint32_t token; };
static struct texture_reference textures[3][SCENE_TEXTURE_LIMIT];
static unsigned texture_counts[3];
#define SCENE_ALLOCATION_LIMIT 128
static uint32_t allocations[3][SCENE_ALLOCATION_LIMIT];
static unsigned allocation_counts[3];

int halo_vita_present_use_allocation(uint32_t id)
{
    const struct halo_vita_surface_slot *surface;
    unsigned i,slot;
    int result;
    if(!ready || active<0 || __atomic_load_n(&failure,__ATOMIC_ACQUIRE))return -1;
    slot=(unsigned)active;surface=halo_vita_surface_get(slot);
    if(!surface || id==surface->color_allocation || id==surface->depth_allocation)return -1;
    for(i=0;i<allocation_counts[slot];i++)if(allocations[slot][i]==id)return 0;
    if(allocation_counts[slot]==SCENE_ALLOCATION_LIMIT)return -1;
    result=halo_vita_logical_read_ready(id);if(result!=0)return -1;
    result=halo_vita_allocation_pin(id);if(result<0)return result;
    allocations[slot][allocation_counts[slot]++]=id;return 0;
}

static int retire_textures(unsigned slot)
{
    while(allocation_counts[slot]) {
        unsigned index=allocation_counts[slot]-1;
        int result=halo_vita_allocation_retire(allocations[slot][index]);
        if(result<0)return result;
        allocations[slot][index]=0;allocation_counts[slot]--;
    }
    while (texture_counts[slot]) {
        struct texture_reference *ref=&textures[slot][texture_counts[slot]-1];
        int result=halo_vita_texture_resource_retire(ref->resource,ref->token);
        if (result<0) return result; /* preserve failed ownership for diagnosis */
        ref->resource=NULL;ref->token=0;texture_counts[slot]--;
    }
    return 0;
}
int halo_vita_present_use_texture(struct halo_vita_texture_resource *resource,uint32_t token)
{
    unsigned i,slot;
    int result;
    if (!ready || active<0 || __atomic_load_n(&failure,__ATOMIC_ACQUIRE)) return -1;
    slot=(unsigned)active;
    for (i=0;i<texture_counts[slot];i++)
        if (textures[slot][i].resource==resource && textures[slot][i].token==token) return 0;
    if (texture_counts[slot]>=SCENE_TEXTURE_LIMIT) return -1;
    result=halo_vita_texture_resource_pin(resource,token);
    if (result<0) return result;
    textures[slot][texture_counts[slot]].resource=resource;
    textures[slot][texture_counts[slot]].token=token;
    texture_counts[slot]++;
    return 0;
}

static void display_callback(const void *data)
{
    const struct display_entry *entry=data;
    int result;
    if (__atomic_load_n(&failure,__ATOMIC_ACQUIRE)) return;
    result=sceDisplaySetFrameBuf(&entry->frame,SCE_DISPLAY_SETBUF_NEXTFRAME);
    if (result>=0) result=sceDisplayWaitVblankStart();
    if (result<0) { __atomic_store_n(&failure,result,__ATOMIC_RELEASE);return; }
    /* Old buffer is no longer scanned out. The callback is also gated by
     * GXM's old/new sync objects. CPU owner performs the actual unpin. */
    __atomic_store_n(&states[entry->old_slot],RETIRED,__ATOMIC_RELEASE);
}

void halo_vita_present_configure(SceGxmInitializeParams *params)
{
    params->displayQueueMaxPendingCount=2;
    params->displayQueueCallback=display_callback;
    params->displayQueueCallbackDataSize=sizeof(struct display_entry);
}

int halo_vita_present_initialize(void)
{
    unsigned i;
    if (ready || active>=0 || !halo_vita_graphics_context()) return -1;
    for (i=0;i<3;i++) {
        const struct halo_vita_surface_slot *s=halo_vita_surface_get(i);
        if (pinned[i] || texture_counts[i] || allocation_counts[i] || !s || s->width!=960 || s->height!=544) return -1;
    }
    for (i=0;i<3;i++) __atomic_store_n(&states[i],FREE,__ATOMIC_RELEASE);
    __atomic_store_n(&failure,0,__ATOMIC_RELEASE);
    tail=2;cursor=0;
    if (halo_vita_surface_pin(tail)<0) return -1;
    pinned[tail]=1;
    __atomic_store_n(&states[tail],QUEUED,__ATOMIC_RELEASE);
    ready=1;
    return 0;
}

int halo_vita_present_reap(void)
{
    unsigned i;
    int result;
    if(!ready)return 0;
    if(active>=0 || halo_vita_graphics_scene_active())return -1;
    result=__atomic_load_n(&failure,__ATOMIC_ACQUIRE);if(result)return result;
    for(i=0;i<3;i++)if(__atomic_load_n(&states[i],__ATOMIC_ACQUIRE)==RETIRED) {
        result=retire_textures(i);if(result<0)return result;
        if(!pinned[i] || halo_vita_surface_retire(i)<0)return -1;
        pinned[i]=0;__atomic_store_n(&states[i],FREE,__ATOMIC_RELEASE);
    }
    return 0;
}

int halo_vita_present_begin(unsigned *slot)
{
    unsigned i,attempt;
    int result;
    if (!ready || active>=0 || !slot) return -1;
    result=halo_vita_present_reap();if(result<0)return result;
    for (attempt=0;attempt<3;attempt++) {
        unsigned state;
        i=(cursor+attempt)%3;
        state=__atomic_load_n(&states[i],__ATOMIC_ACQUIRE);
        const struct halo_vita_surface_slot *s;
        if (state==RETIRED) {
            result=retire_textures(i);if (result<0) return result;
            if (!pinned[i] || halo_vita_surface_retire(i)<0) return -1;
            pinned[i]=0;
            __atomic_store_n(&states[i],FREE,__ATOMIC_RELEASE);
            state=FREE;
        }
        if (state!=FREE) continue;
        s=halo_vita_surface_get(i);
        if (!s) return -1;
        /* Display retirement does not retire reads by a later scene using
         * this allocation through a texture alias. Never overwrite those. */
        result=halo_vita_allocation_gpu_busy(s->color_allocation);
        if(result<0)return result;
        if(result)continue;
        result=halo_vita_allocation_gpu_busy(s->depth_allocation);
        if(result<0)return result;
        if(result)continue;
        if(halo_vita_graphics_scene_claim(1)<0)return -1;
        if (halo_vita_surface_pin(i)<0) { halo_vita_graphics_scene_release(1);return -1; }
        pinned[i]=1;
        __atomic_store_n(&states[i],HELD,__ATOMIC_RELEASE);
        result=sceGxmBeginScene(halo_vita_graphics_context(),0,
            halo_vita_graphics_target(),NULL,NULL,s->sync,&s->color,&s->depth);
        if (result<0) {
            halo_vita_surface_retire(i);pinned[i]=0;
            halo_vita_graphics_scene_release(1);
            __atomic_store_n(&states[i],FREE,__ATOMIC_RELEASE);
            return result;
        }
        active=(int)i;*slot=i;cursor=(i+1)%3;return 0;
    }
    return 1;
}

int halo_vita_present_end(void)
{
    const struct halo_vita_surface_slot *s,*old;
    struct display_entry entry;
    int result;
    unsigned index;
    if (!ready || active<0) return -1;
    index=(unsigned)active;
    s=halo_vita_surface_get(index);old=halo_vita_surface_get(tail);
    if (!s || !old) return -1;
    result=sceGxmEndScene(halo_vita_graphics_context(),NULL,NULL);
    if (result<0) { __atomic_store_n(&failure,result,__ATOMIC_RELEASE);return result; }
    active=-1;
    result=halo_vita_graphics_scene_release(1);if(result<0)return result;
    result=sceGxmPadHeartbeat(&s->color,s->sync);
    if (result<0) { __atomic_store_n(&failure,result,__ATOMIC_RELEASE);return result; }
    memset(&entry,0,sizeof(entry));
    entry.frame.size=sizeof(entry.frame);entry.frame.base=s->pixels;
    entry.frame.pitch=s->stride;entry.frame.width=s->width;entry.frame.height=s->height;
    entry.frame.pixelformat=SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    entry.old_slot=tail;
    __atomic_store_n(&states[index],QUEUED,__ATOMIC_RELEASE);
    result=sceGxmDisplayQueueAddEntry(old->sync,s->sync,&entry);
    if (result<0) {
        /* Submitted GPU work may still reference this slot. Keep every pin
         * until shutdown drains the context and display, never guess reuse. */
        __atomic_store_n(&failure,result,__ATOMIC_RELEASE);return result;
    }
    tail=index;
    return 0;
}

int halo_vita_present_shutdown(void)
{
    unsigned i;
    int result;
    if (!ready) return 0;
    if (active>=0 || halo_vita_graphics_scene_active()) return -1;
    sceGxmFinish(halo_vita_graphics_context());
    result=sceGxmDisplayQueueFinish();
    if (result<0) return result;
    result=sceDisplaySetFrameBuf(NULL,SCE_DISPLAY_SETBUF_NEXTFRAME);
    if (result<0) return result;
    result=sceDisplayWaitVblankStart();if (result<0) return result;
    result=sceDisplayWaitVblankStart();if (result<0) return result;
    for (i=0;i<3;i++) {
        result=retire_textures(i);if (result<0) return result;
    }
    for (i=0;i<3;i++) if (pinned[i]) {
        result=halo_vita_surface_retire(i);if (result<0) return result;
        pinned[i]=0;
    }
    ready=0;
    return 0;
}
