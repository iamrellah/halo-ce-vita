#ifndef HALO_VITA_PIXEL_STATE_H
#define HALO_VITA_PIXEL_STATE_H
#include "xdk_xbox.h"
#include <stdint.h>
struct halo_vita_pixel_state {
    D3DPIXELSHADERDEF definition;
    uint32_t hash,key;
    float constants[18][4];
};
/* Single render owner; capture includes direct writes through Xbox inline APIs. */
void halo_vita_pixel_state_reset(void);
int halo_vita_pixel_state_capture(struct halo_vita_pixel_state *out);
#endif
