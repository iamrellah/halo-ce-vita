#include "halo_vita_texture_resource.h"
#include "halo_vita_attributes.h"
#include "halo_vita_index_snapshot.h"
#include "halo_vita_logical_targets.h"
#include "halo_vita_graphics.h"
#include "halo_vita_allocation.h"
#include "halo_vita_present.h"
#include "halo_vita_clear_draw.h"
#include <stdint.h>

/* Exclusive native notification words 0 and 1; no other native module uses
 * this region. Never reset a word while its previous submission is pending. */
static SceGxmNotification fences[2];
static uint32_t pending[2],color_ids[2],depth_ids[2],serial;
static int active=-1,failed;
#define READ_LIMIT 128
static uint32_t read_ids[2][READ_LIMIT];
static unsigned read_count[2];
#define TEXTURE_LIMIT 512
static struct {struct halo_vita_texture_resource *resource;uint32_t token;} textures[2][TEXTURE_LIMIT];
static unsigned texture_count[2];
static int pin_write(uint32_t id)
{
    int result=halo_vita_allocation_retain(id);
    if(result<0)return result;
    result=halo_vita_allocation_pin(id);
    halo_vita_allocation_release(id);return result;
}
int halo_vita_logical_poll(unsigned index)
{
    unsigned i;
    if(index>=2 || failed)return -1;
    if(active==(int)index)return 1;
    if(!pending[index])return 0;
    if(__atomic_load_n(fences[index].address,__ATOMIC_ACQUIRE)!=pending[index])return 1;
    /* Fragment completion covers earlier vertex/fragment reads too. */
    for(i=0;i<read_count[index];i++) {
        if(halo_vita_allocation_retire(read_ids[index][i])<0){failed=1;return -1;}
        read_ids[index][i]=0;
    }
    read_count[index]=0;
    while(texture_count[index]) {
        unsigned last=texture_count[index]-1;
        if(halo_vita_texture_resource_retire(textures[index][last].resource,textures[index][last].token)<0) {
            failed=1;return -1;
        }
        textures[index][last].resource=NULL;textures[index][last].token=0;texture_count[index]--;
    }
    /* Only now release render-target write pins. */
    if(halo_vita_allocation_retire(color_ids[index])<0 ||
       halo_vita_allocation_retire(depth_ids[index])<0){failed=1;return -1;}
    pending[index]=0;color_ids[index]=0;depth_ids[index]=0;return 0;
}
int halo_vita_logical_read_ready(uint32_t id)
{
    unsigned i;
    if(failed)return -1;
    for(i=0;i<2;i++) {
        const struct halo_vita_surface_slot *s=halo_vita_logical_target_get(i);
        if(s && (id==s->color_allocation || id==s->depth_allocation))
            return halo_vita_logical_poll(i);
    }
    return 0;
}
int halo_vita_logical_begin(unsigned index)
{
    const struct halo_vita_surface_slot *s=halo_vita_logical_target_get(index);
    volatile unsigned int *words;
    int result;
    if(!s || active>=0 || failed || serial==UINT32_MAX)return -1;
    result=halo_vita_present_reap();if(result<0)return result;
    result=halo_vita_logical_poll(index);if(result)return result;
    result=halo_vita_allocation_gpu_busy(s->color_allocation);if(result)return result;
    result=halo_vita_allocation_gpu_busy(s->depth_allocation);if(result)return result;
    words=sceGxmGetNotificationRegion();if(!words)return -1;
    result=halo_vita_attribute_snapshots_reset(index);if(result<0)return result;
    if(halo_vita_graphics_scene_claim(2)<0)return -1;
    result=pin_write(s->color_allocation);if(result<0)goto claim_fail;
    result=pin_write(s->depth_allocation);if(result<0)goto color_fail;
    fences[index].address=words+index;fences[index].value=++serial;
    __atomic_store_n(fences[index].address,0,__ATOMIC_RELEASE);
    result=sceGxmBeginScene(halo_vita_graphics_context(),0,
        halo_vita_logical_render_target(),NULL,NULL,s->sync,&s->color,&s->depth);
    if(result<0){halo_vita_allocation_retire(s->depth_allocation);goto color_fail;}
    color_ids[index]=s->color_allocation;depth_ids[index]=s->depth_allocation;
    active=(int)index;halo_vita_clear_scene_reset(index);halo_vita_index_snapshots_reset(index);return 0;
color_fail:
    halo_vita_allocation_retire(s->color_allocation);
claim_fail:
    halo_vita_graphics_scene_release(2);return result;
}
int halo_vita_logical_end(void)
{
    unsigned index;
    int result;
    if(active<0 || failed)return -1;
    index=(unsigned)active;
    result=sceGxmEndScene(halo_vita_graphics_context(),NULL,&fences[index]);
    if(result<0){failed=1;return result;} /* Unknown GPU state retains ownership. */
    pending[index]=fences[index].value;active=-1;
    return halo_vita_graphics_scene_release(2);
}

int halo_vita_logical_active_index(void) { return active; }

int halo_vita_logical_use_allocation(uint32_t id)
{
    unsigned i,index;
    int result;
    if(active<0 || failed || !id)return -1;
    index=(unsigned)active;
    if(id==color_ids[index] || id==depth_ids[index])return -1;
    /* Native vertex owners are uncached. Publish completed CPU writes before
     * commands can consume them; cached external owners must clean separately. */
    __asm__ volatile("dsb sy" ::: "memory");
    for(i=0;i<read_count[index];i++)if(read_ids[index][i]==id)return 0;
    if(read_count[index]==READ_LIMIT)return -1;
    result=halo_vita_logical_read_ready(id);if(result)return result;
    result=halo_vita_allocation_pin(id);if(result<0)return result;
    read_ids[index][read_count[index]++]=id;return 0;
}

int halo_vita_logical_use_texture(struct halo_vita_texture_resource *resource,uint32_t token)
{
    unsigned i,slot;int result;
    if(active<0 || failed)return -1;
    slot=(unsigned)active;
    for(i=0;i<texture_count[slot];i++)
        if(textures[slot][i].resource==resource && textures[slot][i].token==token)return 0;
    if(texture_count[slot]>=TEXTURE_LIMIT)return -1;
    result=halo_vita_texture_resource_pin(resource,token);if(result<0)return result;
    textures[slot][texture_count[slot]].resource=resource;
    textures[slot][texture_count[slot]].token=token;texture_count[slot]++;return 0;
}
