#ifndef HALO_VITA_ARENAS_H
#define HALO_VITA_ARENAS_H
#include <stddef.h>
enum halo_vita_arena { HALO_VITA_GAME_STATE, HALO_VITA_TAG_CACHE,
    HALO_VITA_TEXTURE_CACHE, HALO_VITA_SOUND_CACHE, HALO_VITA_ARENA_COUNT };
/* Single-owner startup/shutdown only. No workers or GPU may access at shutdown.
 * CPU memory only: allocation does not perform GXM mapping or save relocation. */
int halo_vita_arenas_initialize(void);
int halo_vita_arenas_dispose(void);
/* Caller initializes GXM first. READ-only GPU mappings, no cache publication.
 * Caller must retire ALL GPU references before unmap. No implicit GPU waits. */
int halo_vita_arena_map_gpu(enum halo_vita_arena arena);
int halo_vita_arena_unmap_gpu(enum halo_vita_arena arena);
void *halo_vita_arena_base(enum halo_vita_arena arena);
size_t halo_vita_arena_size(enum halo_vita_arena arena);
#endif
