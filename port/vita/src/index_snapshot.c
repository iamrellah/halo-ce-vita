#include "halo_vita_index_snapshot.h"
#include "halo_vita_buffer.h"
#include "halo_vita_allocation.h"
#include "halo_vita_topology.h"
#include "halo_vita_logical_targets.h"
#define SLOT_BYTES (256u*1024u)
static struct halo_vita_buffer slots[2]={{.uid=-1},{.uid=-1}};
static size_t used[2];
static int ready;
int halo_vita_index_snapshots_destroy(void)
{
    unsigned i;
    for(i=0;i<2;i++)if(slots[i].allocation && halo_vita_allocation_busy(slots[i].allocation)!=0)return -1;
    ready=0;
    for(i=0;i<2;i++)if(halo_vita_buffer_destroy(&slots[i])<0)return -1;
    return 0;
}
int halo_vita_index_snapshots_create(void)
{
    unsigned i;
    if(ready || slots[0].uid>=0 || slots[1].uid>=0)return -1;
    for(i=0;i<2;i++) {
        if(halo_vita_buffer_create_format(&slots[i],SLOT_BYTES,HALO_VITA_ALLOCATION_INDEX_BYTES)<0) {
            halo_vita_index_snapshots_destroy();return -1;
        }
        used[i]=0;
    }
    ready=1;return 0;
}
void halo_vita_index_snapshots_reset(unsigned slot)
{
    if(slot<2)used[slot]=0;
}
int halo_vita_index_snapshot(D3DPRIMITIVETYPE type,size_t first,size_t count,
    struct halo_vita_index_snapshot *out)
{
    int slot=halo_vita_logical_active_index(),result;
    struct halo_vita_index_view view;
    struct halo_vita_index_snapshot snapshot;
    size_t i,offset;
    uint16_t *destination;
    if(!ready || slot<0 || slot>=2 || !out || !count)return -1;
    result=halo_vita_indices_acquire(first,count,&view);if(result<0)return result;
    offset=(used[slot]+3)&~(size_t)3;
    if(offset>SLOT_BYTES){halo_vita_allocation_release(view.allocation);return -1;}
    destination=(uint16_t *)((char *)slots[slot].base+offset);
    snapshot.minimum=65535;snapshot.maximum=0;snapshot.base_vertex=view.base_vertex;
    for(i=0;i<count;i++) {
        if(view.data[i]<snapshot.minimum)snapshot.minimum=view.data[i];
        if(view.data[i]>snapshot.maximum)snapshot.maximum=view.data[i];
    }
    if(snapshot.base_vertex>UINT32_MAX-snapshot.maximum)result=-1;
    else result=halo_vita_topology(type,view.data,count,destination,(SLOT_BYTES-offset)/2,
        &snapshot.primitive,&snapshot.count);
    halo_vita_allocation_release(view.allocation);
    if(result<0)return result;
    result=halo_vita_allocation_retain(slots[slot].allocation);if(result<0)return result;
    /* use_allocation publishes uncached writes, including repeated use. */
    result=halo_vita_logical_use_allocation(slots[slot].allocation);
    halo_vita_allocation_release(slots[slot].allocation);
    if(result)return result;
    snapshot.data=destination;
    used[slot]=offset+snapshot.count*2;
    *out=snapshot;return 0;
}
