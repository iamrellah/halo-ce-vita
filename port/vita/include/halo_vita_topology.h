#ifndef HALO_VITA_TOPOLOGY_H
#define HALO_VITA_TOPOLOGY_H
#include "xdk_xbox.h"
#include <psp2/gxm.h>
#include <stddef.h>
#include <stdint.h>
/* Input/output may not overlap. NULL input generates sequential indices.
 * All size/range checks precede output writes. No allocation or submission. */
int halo_vita_topology(D3DPRIMITIVETYPE type,const uint16_t *input,size_t count,
    uint16_t *output,size_t capacity,SceGxmPrimitiveType *primitive,size_t *written);
#endif
