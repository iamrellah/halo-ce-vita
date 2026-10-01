#ifndef HALO_VITA_FRAGMENT_UNIFORMS_H
#define HALO_VITA_FRAGMENT_UNIFORMS_H
#include "halo_vita_pixel_state.h"
#include "halo_vita_fragment_program.h"
struct halo_vita_fragment_uniforms {
    uint32_t key;
    float psc[18][4],fog[4],atest[4],texscale[4][4],border[2][4];
    unsigned has_texscale,has_border;
};
/* Texture-derived values are explicit: no guessed scale/border defaults. */
int halo_vita_fragment_uniforms_capture(const struct halo_vita_pixel_state *pixel,
    const DWORD *states,const float *texscale,const float *border,
    struct halo_vita_fragment_uniforms *out);
/* Inside a logical scene. One reservation for all uniforms; failure forbids draw. */
int halo_vita_fragment_bind(const struct halo_vita_fragment_program *fragment,
    const struct halo_vita_fragment_uniforms *uniforms);
#endif
