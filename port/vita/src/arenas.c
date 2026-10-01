#include "halo_vita_arenas.h"
#include <psp2/kernel/sysmem.h>
#include <stdint.h>
#include <psp2/gxm.h>

struct arena_record { const char *name; size_t size; SceUID uid; void *base; int mapped; };
static struct arena_record arenas[HALO_VITA_ARENA_COUNT] = {
    { "halo game state", 0x345000, -1, NULL, 0 },
    { "halo tag cache", 0x1600000, -1, NULL, 0 },
    { "halo texture cache", 0x1600000, -1, NULL, 0 },
    { "halo sound cache", 0x400000, -1, NULL, 0 }
};
static int ready;

int halo_vita_arenas_dispose(void)
{
    int i, result, first_error = 0;
    /* Refuse the whole dispose before freeing any arena if one is mapped. */
    for (i = 0; i < HALO_VITA_ARENA_COUNT; i++)
        if (arenas[i].mapped) return -1;
    ready = 0;
    for (i = HALO_VITA_ARENA_COUNT - 1; i >= 0; i--) {
        if (arenas[i].uid < 0) continue;
        result = sceKernelFreeMemBlock(arenas[i].uid);
        if (result < 0) { if (!first_error) first_error = result; continue; }
        arenas[i].uid = -1;
        arenas[i].base = NULL;
    }
    return first_error;
}

int halo_vita_arenas_initialize(void)
{
    unsigned i;
    int result;
    if (ready) return -1;
    for (i = 0; i < HALO_VITA_ARENA_COUNT; i++)
        if (arenas[i].uid >= 0) return -1; /* Failed cleanup requires retry. */
    for (i = 0; i < HALO_VITA_ARENA_COUNT; i++) {
        arenas[i].uid = sceKernelAllocMemBlock(arenas[i].name,
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, arenas[i].size, NULL);
        if (arenas[i].uid < 0) { result = arenas[i].uid; goto fail; }
        result = sceKernelGetMemBlockBase(arenas[i].uid, &arenas[i].base);
        if (result < 0) goto fail;
        if (!arenas[i].base || ((uintptr_t)arenas[i].base & 4095)) {
            result = -1; goto fail;
        }
    }
    ready = 1;
    return 0;
fail:
    halo_vita_arenas_dispose(); /* Failed frees retain their UIDs for retry. */
    return result;
}

void *halo_vita_arena_base(enum halo_vita_arena arena)
{
    if (!ready || (unsigned)arena >= HALO_VITA_ARENA_COUNT) return NULL;
    return arenas[arena].base;
}

size_t halo_vita_arena_size(enum halo_vita_arena arena)
{
    if ((unsigned)arena >= HALO_VITA_ARENA_COUNT) return 0;
    return arenas[arena].size;
}

int halo_vita_arena_map_gpu(enum halo_vita_arena arena)
{
    int result;
    if (!ready || (unsigned)arena >= HALO_VITA_ARENA_COUNT ||
        arena == HALO_VITA_SOUND_CACHE || arenas[arena].mapped) return -1;
    result = sceGxmMapMemory(arenas[arena].base, arenas[arena].size,
                            SCE_GXM_MEMORY_ATTRIB_READ);
    if (result < 0) return result;
    arenas[arena].mapped = 1;
    return 0;
}

int halo_vita_arena_unmap_gpu(enum halo_vita_arena arena)
{
    int result;
    if (!ready || (unsigned)arena >= HALO_VITA_ARENA_COUNT ||
        !arenas[arena].mapped) return -1;
    result = sceGxmUnmapMemory(arenas[arena].base);
    if (result < 0) return result; /* Keep ownership if GXM refused unmap. */
    arenas[arena].mapped = 0;
    return 0;
}
