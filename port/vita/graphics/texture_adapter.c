/* Uses Xita's decoder by explicit build dependency; preserve its license. */
#include "halo_vita_texture.h"
#include "xv_texture_decode.h"
#include <stdint.h>

static int overlaps(const void *a, size_t as, const void *b, size_t bs)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if (!as || !bs) return 0;
    if (x > UINTPTR_MAX-(as-1) || y > UINTPTR_MAX-(bs-1)) return 1;
    return x <= y+bs-1 && y <= x+as-1;
}

int halo_vita_decode_texture(const struct halo_vita_texture_view *v,
    uint32_t *output, size_t output_bytes)
{
    static const unsigned char swizzled[18] = {
        0x19,0x00,0x01,0x1a,255,255,0x05,255,0x02,0x04,0x07,0x06,255,255,0x0c,0x0e,0x0f,0x0b
    };
    static const unsigned char linear[18] = {
        0x1f,0x13,0x1b,0x20,255,255,0x11,255,0x10,0x1d,0x1e,0x12,255,255,255,255,255,255
    };
    static const unsigned char bytes[18] = {1,1,1,2,0,0,2,0,2,2,4,4,0,0,0,0,0,1};
    size_t pixels,required,row;
    unsigned fmt;
    xv_texture_job job;
    if (!v || !v->source || !output || ((uintptr_t)output & 3) ||
        !v->width || !v->height || v->bitmap_format >= 18 ||
        v->width > SIZE_MAX/v->height) return -1;
    pixels=(size_t)v->width*v->height;
    if (pixels > SIZE_MAX/4 || output_bytes < pixels*4) return -1;
    fmt = v->linear ? linear[v->bitmap_format] : swizzled[v->bitmap_format];
    if (fmt == 255) return -1;
    if (v->bitmap_format >= 14 && v->bitmap_format <= 16) {
        size_t bw=(v->width/4)+(v->width%4!=0), bh=(v->height/4)+(v->height%4!=0);
        unsigned bs=v->bitmap_format==14?8:16;
        if (bw > SIZE_MAX/bh || bw*bh > SIZE_MAX/bs) return -1;
        required=bw*bh*bs;
    } else if (v->linear) {
        if (v->width > SIZE_MAX/bytes[v->bitmap_format]) return -1;
        row=(size_t)v->width*bytes[v->bitmap_format];
        if (v->linear_pitch < row || v->height > SIZE_MAX/v->linear_pitch) return -1;
        required=(size_t)v->linear_pitch*(v->height-1)+row;
    } else {
        if ((v->width&(v->width-1)) || (v->height&(v->height-1)) ||
            pixels > SIZE_MAX/bytes[v->bitmap_format]) return -1;
        required=pixels*bytes[v->bitmap_format];
    }
    if (v->source_bytes < required || overlaps(v->source,required,output,pixels*4)) return -1;
    if (v->bitmap_format==17 && (!v->palette || ((uintptr_t)v->palette&3) ||
        v->palette_entries < 256 || overlaps(v->palette,1024,output,pixels*4))) return -1;
    job=(xv_texture_job){v->source,output,v->palette,fmt,v->width,v->height,v->linear_pitch,
                       v->linear,0,0,0};
    return xv_tex_decode_range(&job,0,xv_tex_units(&job));
}

int halo_vita_reorder_bc(const struct halo_vita_texture_view *v,void *output,size_t output_bytes)
{
    size_t blocks,required;
    unsigned block;
    if (!v || !v->source || !output || v->linear || v->bitmap_format<14 || v->bitmap_format>16 ||
        v->width<4 || v->height<4 || v->width>4096 || v->height>4096 ||
        (v->width&(v->width-1)) || (v->height&(v->height-1))) return -1;
    block=v->bitmap_format==14?8:16;
    blocks=(size_t)(v->width/4)*(v->height/4);required=blocks*block;
    if (v->source_bytes<required || output_bytes<required ||
        overlaps(v->source,required,output,required)) return -1;
    return xv_bc_reorder_range(output,v->source,v->width,v->height,block,0,(unsigned)blocks);
}
