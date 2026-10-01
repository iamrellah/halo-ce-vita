#ifndef HALO_VITA_FRAGMENT_PROGRAM_H
#define HALO_VITA_FRAGMENT_PROGRAM_H
#include "halo_vita_vertex_program.h"
struct halo_vita_fragment_entry {
    uint32_t vs_hash,key,raw_hash;
    unsigned char cube_mask,cube_modes,c2d_mask;
    const unsigned char *data;unsigned bytes;
};
struct halo_vita_fragment_program {
    const struct halo_vita_fragment_entry *entry;
    const SceGxmProgram *source;
    SceGxmShaderPatcherId id;SceGxmFragmentProgram *program;
    const SceGxmProgramParameter *psc,*fogcolor,*atest,*texscale,*border1;
    int textures[4];unsigned discard,depth_replace;
    int retained,registered;
};
unsigned halo_vita_fragment_count(void);
const struct halo_vita_fragment_entry *halo_vita_fragment_entry(unsigned index);
int halo_vita_fragment_find(uint32_t vs_hash,uint32_t key,unsigned c2d,unsigned *out);
/* Zero-initialize output. Owner must retire GPU uses before destruction. */
int halo_vita_fragment_load(unsigned index,const struct halo_vita_vertex_program *vs,
    const SceGxmBlendInfo *blend,struct halo_vita_fragment_program *out);
int halo_vita_fragment_destroy(struct halo_vita_fragment_program *program);
struct halo_vita_texture_resource;
/* 0 bound/unused, 1 still loading, negative failure. Prepared single-mip 2D
 * uploads only; cube resources remain unsupported. Copies sampler state per
 * draw and pins upload until logical-scene completion. Optional snapshot is
 * written only for a successful used sampler, for texture-derived uniforms. */
int halo_vita_fragment_texture_bind(const struct halo_vita_fragment_program *,unsigned stage,
    struct halo_vita_texture_resource *,uint32_t token,const unsigned long *state,SceGxmTexture *snapshot);
#endif
