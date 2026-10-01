#ifndef HALO_VITA_CONSTANTS_H
#define HALO_VITA_CONSTANTS_H
#include "xdk_xbox.h"
#include <psp2/gxm.h>
void halo_vita_constants_reset(void);
void halo_vita_constants_viewport(const D3DVIEWPORT8 *viewport);
int halo_vita_constants_copy(int first,unsigned count,void *out);
/* Caller reserves the vertex uniform buffer once for the draw. */
int halo_vita_constants_upload(void *uniforms,const SceGxmProgramParameter *parameter,int first,unsigned count);
#endif
