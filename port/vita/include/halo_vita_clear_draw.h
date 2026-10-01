#ifndef HALO_VITA_CLEAR_DRAW_H
#define HALO_VITA_CLEAR_DRAW_H
#include <stdint.h>
#include "xdk_xbox.h"
#include <stddef.h>
/* Bring-up color draw, not full D3D Clear semantics. Owns immutable geometry.
 * Create with live patcher; draw only inside a scene on the native context.
 * Destroy only after presentation shutdown, before patcher/context destruction. */
int halo_vita_clear_create(void);
int halo_vita_clear_draw(void);
int halo_vita_clear_draw_size(unsigned width,unsigned height);
int halo_vita_clear_destroy(void);
void halo_vita_clear_scene_reset(unsigned slot);
int halo_vita_clear_native(unsigned xbox_flags,uint32_t color,float z,unsigned stencil);
/* Test/bring-up quad using captured blend/depth/stencil; current viewport. */
int halo_vita_color_quad(uint32_t color,float clip_z);
/* Qualification layout only: float3 clip position + packed D3DCOLOR, stride 16. */
int halo_vita_color_stream(unsigned stream,size_t first);
int halo_vita_color_indexed(unsigned stream,D3DPRIMITIVETYPE type,size_t first,size_t count);
#endif
