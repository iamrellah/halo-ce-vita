#include "halo_vita_texture.h"
#include <stdint.h>

int halo_vita_texture_select(const struct halo_vita_bitmap_layout *b,
    const void *resource, size_t resource_bytes, unsigned mip, unsigned face,
    struct halo_vita_texture_view *view)
{
    static const unsigned char bits[18]={8,8,8,16,0,0,16,0,16,16,32,32,0,0,4,8,8,8};
    static const unsigned char face_map[6]={0,2,1,3,4,5};
    uint64_t total=0, selected_offset=0, selected_size=0, stride, offset;
    unsigned maximum=0, dimension, level, w=0,h=0,sw=0,sh=0,pitch=0;
    unsigned faces,compressed,linear;
    struct halo_vita_texture_view result;
    if (!b || !resource || !view || !b->width || !b->height ||
        b->width>32767 || b->height>32767 || b->depth!=1 ||
        (b->type!=0 && b->type!=2) || b->format>=18 || !bits[b->format] || b->mip_count>15)
        return -1;
    compressed=!!(b->flags&2);linear=!!(b->flags&16);faces=b->type==2?6:1;
    if (compressed!=(b->format>=14 && b->format<=16) || face>=faces ||
        (faces==6 && (linear || b->width!=b->height)) || (linear && compressed)) return -1;
    if (!linear) {
        if (!(b->flags&1) || (b->width&(b->width-1)) || (b->height&(b->height-1))) return -1;
        dimension=b->width>b->height?b->width:b->height;
        if (compressed) dimension/=4;
        while (dimension>1) { maximum++;dimension>>=1; }
        if (maximum>b->mip_count) maximum=b->mip_count;
    }
    if (mip>maximum) return -1;
    for (level=0;level<=maximum;level++) {
        uint64_t size;
        unsigned rw,rh;
        w=b->width>>level;h=b->height>>level;
        if (!w) w=1;
        if (!h) h=1;
        rw=compressed?(w+3)&~3u:w;
        rh=compressed?(h+3)&~3u:h;
        if (linear) {
            pitch=((w*bits[b->format]/8)+63)&~63u;
            size=(uint64_t)pitch*h;
        } else size=(uint64_t)rw*rh*bits[b->format]/8;
        if (level==mip) { selected_offset=total;selected_size=size;sw=w;sh=h; }
        total+=size;
    }
    stride=(total+127)&~(uint64_t)127;
    if (stride*faces>resource_bytes || stride*faces>SIZE_MAX ||
        (uintptr_t)resource>UINTPTR_MAX-(size_t)(stride*faces-1)) return -1;
    offset=stride*face_map[face]+selected_offset;
    result.source=(const unsigned char *)resource+(size_t)offset;
    result.source_bytes=(size_t)selected_size;
    result.width=sw;result.height=sh;result.bitmap_format=b->format;
    result.linear_pitch=pitch;result.linear=linear;
    result.palette=NULL;result.palette_entries=0;
    *view=result;
    return 0;
}
