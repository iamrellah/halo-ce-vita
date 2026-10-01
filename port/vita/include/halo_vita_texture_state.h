#ifndef HALO_VITA_TEXTURE_STATE_H
#define HALO_VITA_TEXTURE_STATE_H
#include "xdk_xbox.h"
#include <psp2/gxm.h>
extern DWORD D3D__TextureState[4][D3DTSS_MAX];
void halo_vita_texture_state_reset(void);
/* Applies a copied stage state to a single-mip 2D descriptor. Output is unchanged
 * on unsupported state or validation failure. Colored borders require a shader
 * implementation and currently fail; no silent clamp substitution. */
int halo_vita_sampler_apply(const DWORD *state,SceGxmTexture *texture);
#endif
