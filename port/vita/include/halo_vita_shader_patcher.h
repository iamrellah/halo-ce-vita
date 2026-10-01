#ifndef HALO_VITA_SHADER_PATCHER_H
#define HALO_VITA_SHADER_PATCHER_H
#include <psp2/gxm.h>
/* Single render owner, GXM already initialized. Every registered program owner
 * retains this pool until GPU retirement and all unregister/releases finish. */
int halo_vita_shader_patcher_create(void);
int halo_vita_shader_patcher_destroy(void);
SceGxmShaderPatcher *halo_vita_shader_patcher_get(void);
int halo_vita_shader_patcher_retain(void);
int halo_vita_shader_patcher_release(void);
#endif
