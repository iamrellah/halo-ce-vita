#include "halo_vita_attributes.h"
#include "halo_vita_buffer.h"
#include "halo_vita_allocation.h"
#include "halo_vita_logical_targets.h"
#include <string.h>
#define SLOT_BYTES (64u*1024u)
static float attributes[16][4],last[2][16][4];
static struct halo_vita_buffer slots[2]={{.uid=-1},{.uid=-1}};
static size_t used[2],last_offset[2];
static int ready,valid[2];
void halo_vita_attributes_reset(void)
{
    unsigned i;memset(attributes,0,sizeof(attributes));
    for(i=0;i<16;i++)attributes[i][3]=1.0f;
}
static void set(INT reg,float x,float y,float z,float w)
{
    float value[4]={x,y,z,w};
    if(reg==-1)reg=0; /* D3DVSDE_VERTEX in xdk_d3d8.h */
    if(reg<0 || reg>=16)return;
    memcpy(attributes[reg],value,sizeof(value));
    /* Immediate Begin/End is not implemented yet. Its vertex-emission hook
     * must consume this bank when writing register zero, as upstream does. */
}
void WINAPI D3DDevice_SetVertexData4f(INT reg,FLOAT x,FLOAT y,FLOAT z,FLOAT w)
{ set(reg,x,y,z,w); }
void WINAPI D3DDevice_SetVertexData2f(INT reg,FLOAT x,FLOAT y)
{ set(reg,x,y,0,1); }
void WINAPI D3DDevice_SetVertexData2s(INT reg,SHORT x,SHORT y)
{ set(reg,(float)x,(float)y,0,1); }
void WINAPI D3DDevice_SetVertexData4ub(INT reg,BYTE x,BYTE y,BYTE z,BYTE w)
{ set(reg,x/255.0f,y/255.0f,z/255.0f,w/255.0f); }
void WINAPI D3DDevice_SetVertexDataColor(INT reg,D3DCOLOR color)
{ set(reg,((color>>16)&255)/255.0f,((color>>8)&255)/255.0f,(color&255)/255.0f,((color>>24)&255)/255.0f); }
int halo_vita_attribute_snapshots_destroy(void)
{
    unsigned i;
    for(i=0;i<2;i++)if(slots[i].allocation && halo_vita_allocation_busy(slots[i].allocation)!=0)return -1;
    ready=0;
    for(i=0;i<2;i++)if(halo_vita_buffer_destroy(&slots[i])<0)return -1;
    return 0;
}
int halo_vita_attribute_snapshots_create(void)
{
    unsigned i;
    if(ready || slots[0].uid>=0 || slots[1].uid>=0)return -1;
    for(i=0;i<2;i++) {
        if(halo_vita_buffer_create(&slots[i],SLOT_BYTES)<0) {
            halo_vita_attribute_snapshots_destroy();return -1;
        }
        used[i]=0;valid[i]=0;
    }
    halo_vita_attributes_reset();ready=1;return 0;
}
int halo_vita_attribute_snapshots_reset(unsigned slot)
{
    if(slot>=2)return -1;
    if(!ready)return 0;
    if(halo_vita_allocation_busy(slots[slot].allocation)!=0)return -1;
    used[slot]=0;valid[slot]=0;return 0;
}
int halo_vita_attributes_snapshot(struct halo_vita_stream_view *out)
{
    int slot=halo_vita_logical_active_index();size_t offset;
    struct halo_vita_stream_view view;
    if(!ready || !out || slot<0 || slot>=2)return -1;
    if(valid[slot] && !memcmp(last[slot],attributes,sizeof(attributes)))offset=last_offset[slot];
    else {
        if(used[slot]>SLOT_BYTES-sizeof(attributes))return -1;
        offset=used[slot];
        memcpy((char *)slots[slot].base+offset,attributes,sizeof(attributes));
        memcpy(last[slot],attributes,sizeof(attributes));
        last_offset[slot]=offset;valid[slot]=1;used[slot]+=sizeof(attributes);
    }
    if(halo_vita_allocation_retain(slots[slot].allocation)<0)return -1;
    view.data=(char *)slots[slot].base+offset;view.bytes=256;view.stride=256;
    view.allocation=slots[slot].allocation;*out=view;return 0;
}
