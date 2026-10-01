/*
PHYSICAL_MEMORY_MAP.C

symbols in this file:
001AD660 00f0:
	_physical_memory_allocate (0000)
001AD750 00a0:
	_physical_memory_verify (0000)
001AD7F0 0040:
	_physical_memory_free (0000)
001AD830 0010:
	_physical_memory_get_game_state_base_address (0000)
001AD840 0010:
	_physical_memory_get_tag_cache_base_address (0000)
001AD850 0010:
	_physical_memory_get_texture_cache_base_address (0000)
001AD860 0010:
	_physical_memory_get_sound_cache_base_address (0000)
002A76A0 0035:
	??_C@_0DF@JIMKNOMI@physical_memory_map_globals?4soun@ (0000)
002A76D8 0037:
	??_C@_0DH@HFDEEEDC@physical_memory_map_globals?4text@ (0000)
002A7710 005a:
	??_C@_0FK@BLCIEIND@?$CIunsigned?5long?$CJphysical_memory_m@ (0000)
002A7770 005c:
	??_C@_0FM@EHFFGLDK@?$CIunsigned?5long?$CJphysical_memory_m@ (0000)
002A77CC 002b:
	??_C@_0CL@BKHLOEEB@c?3?2halo?2SOURCE?2cache?2physical_me@ (0000)
002A77F8 001e:
	??_C@_0BO@DKDCAKMK@page_status?5?$DN?$DN?5PAGE_READWRITE?$AA@ (0000)
004D1074 0010:
	_physical_memory_map_globals (0000)
*/

/* ---------- headers */

#ifdef HALO_VITA
#include "halo_vita_arenas.h"
#include <stdlib.h>
#include <string.h>
#else
#include "cseries.h"
#include "cseries_windows.h"
#endif
#include "cache/physical_memory_map.h"

/* ---------- constants */

#ifdef HALO_LINUX
/* the native builds' larger game state, placed above the tag cache
(halo_port_capacity.h); the verified part is the CPU part, as on the Xbox */
#define GAME_STATE_BASE_ADDRESS HALO_PORT_GAME_STATE_BASE_ADDRESS
#define GAME_STATE_SIZE HALO_PORT_GAME_STATE_SIZE
#define GAME_STATE_VERIFY_SIZE HALO_PORT_GAME_STATE_CPU_SIZE
#else
#define GAME_STATE_BASE_ADDRESS 0x80061000
#define GAME_STATE_SIZE 0x345000
#define GAME_STATE_VERIFY_SIZE 0x305000
#endif
#define TAG_CACHE_BASE_ADDRESS 0x803A6000
#define TAG_CACHE_SIZE 0x1600000
#define TEXTURE_CACHE_SIZE 0x1600000
#define SOUND_CACHE_SIZE 0x400000

/* ---------- macros */

/* ---------- structures */

struct physical_memory_map_globals
{
	void *game_state_base_address;
	void *tag_cache_base_address;
	void *texture_cache_base_address;
	void *sound_cache_base_address;
};

/* ---------- prototypes */

/* ---------- globals */

static struct physical_memory_map_globals physical_memory_map_globals;

/* ---------- public code */

void physical_memory_allocate(
	void)
{
#ifdef HALO_VITA
	/* All arenas are acquired before any address is published to the engine.
	 * Xbox virtual addresses are not valid Vita allocation constraints. */
	if (halo_vita_arenas_initialize() != 0) abort();
	physical_memory_map_globals.game_state_base_address = halo_vita_arena_base(HALO_VITA_GAME_STATE);
	physical_memory_map_globals.tag_cache_base_address = halo_vita_arena_base(HALO_VITA_TAG_CACHE);
	physical_memory_map_globals.texture_cache_base_address = halo_vita_arena_base(HALO_VITA_TEXTURE_CACHE);
	physical_memory_map_globals.sound_cache_base_address = halo_vita_arena_base(HALO_VITA_SOUND_CACHE);
	physical_memory_verify();
#else
	physical_memory_map_globals.game_state_base_address = XPhysicalAlloc(GAME_STATE_SIZE, GAME_STATE_BASE_ADDRESS & 0x7FFFFFFF, 0, PAGE_READWRITE);
#line 46 "c:\\halo\\SOURCE\\cache\\physical_memory_map.c"
	match_assert(__FILE__, __LINE__, (unsigned long)physical_memory_map_globals.game_state_base_address==GAME_STATE_BASE_ADDRESS);

	physical_memory_map_globals.tag_cache_base_address = XPhysicalAlloc(TAG_CACHE_SIZE, TAG_CACHE_BASE_ADDRESS & 0x7FFFFFFF, 0, PAGE_READWRITE);
#line 50 "c:\\halo\\SOURCE\\cache\\physical_memory_map.c"
	match_assert(__FILE__, __LINE__, (unsigned long)physical_memory_map_globals.tag_cache_base_address==TAG_CACHE_BASE_ADDRESS);

	physical_memory_map_globals.texture_cache_base_address = XPhysicalAlloc(TEXTURE_CACHE_SIZE, -1, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
#line 55 "c:\\halo\\SOURCE\\cache\\physical_memory_map.c"
	match_assert(__FILE__, __LINE__, physical_memory_map_globals.texture_cache_base_address);

	physical_memory_map_globals.sound_cache_base_address = XPhysicalAlloc(SOUND_CACHE_SIZE, -1, 0, PAGE_READWRITE);
#line 58 "c:\\halo\\SOURCE\\cache\\physical_memory_map.c"
	match_assert(__FILE__, __LINE__, physical_memory_map_globals.sound_cache_base_address);
#endif
	return;
}

void physical_memory_verify(
	void)
{
#ifdef HALO_VITA
	/* Allocator ownership/size checks, not an emulation of Xbox page protection.
	 * USER_RW allocation supplies CPU read/write memory; GXM mapping is separate. */
	if (!physical_memory_map_globals.game_state_base_address ||
		physical_memory_map_globals.game_state_base_address != halo_vita_arena_base(HALO_VITA_GAME_STATE) ||
		physical_memory_map_globals.tag_cache_base_address != halo_vita_arena_base(HALO_VITA_TAG_CACHE) ||
		physical_memory_map_globals.texture_cache_base_address != halo_vita_arena_base(HALO_VITA_TEXTURE_CACHE) ||
		physical_memory_map_globals.sound_cache_base_address != halo_vita_arena_base(HALO_VITA_SOUND_CACHE) ||
		halo_vita_arena_size(HALO_VITA_GAME_STATE) != GAME_STATE_SIZE ||
		halo_vita_arena_size(HALO_VITA_TAG_CACHE) != TAG_CACHE_SIZE ||
		halo_vita_arena_size(HALO_VITA_TEXTURE_CACHE) != TEXTURE_CACHE_SIZE ||
		halo_vita_arena_size(HALO_VITA_SOUND_CACHE) != SOUND_CACHE_SIZE)
		abort();
#else
	byte *address;
	unsigned long page_status;

	for (address = physical_memory_map_globals.tag_cache_base_address;
		address < (byte *)physical_memory_map_globals.tag_cache_base_address + TAG_CACHE_SIZE;
		address += 0x1000)
	{
		page_status = XQueryMemoryProtect(address);
#line 77 "c:\\halo\\SOURCE\\cache\\physical_memory_map.c"
		match_assert(__FILE__, __LINE__, page_status == PAGE_READWRITE);
	}

	for (address = physical_memory_map_globals.game_state_base_address;
		address < (byte *)physical_memory_map_globals.game_state_base_address + GAME_STATE_VERIFY_SIZE;
		address += 0x1000)
	{
		page_status = XQueryMemoryProtect(address);
#line 86 "c:\\halo\\SOURCE\\cache\\physical_memory_map.c"
		match_assert(__FILE__, __LINE__, page_status == PAGE_READWRITE);
	}
#endif
	return;
}

void physical_memory_free(
	void)
{
#ifdef HALO_VITA
	/* Workers and GPU references must already be retired by the caller.
	 * The arena owner rejects disposal while any GXM mapping remains. */
	if (halo_vita_arenas_dispose() != 0) abort();
	memset(&physical_memory_map_globals, 0, sizeof(physical_memory_map_globals));
#else
	if (physical_memory_map_globals.game_state_base_address)
		XPhysicalFree(physical_memory_map_globals.game_state_base_address);
	if (physical_memory_map_globals.tag_cache_base_address)
		XPhysicalFree(physical_memory_map_globals.tag_cache_base_address);
	if (physical_memory_map_globals.texture_cache_base_address)
		XPhysicalFree(physical_memory_map_globals.texture_cache_base_address);
	if (physical_memory_map_globals.sound_cache_base_address)
		XPhysicalFree(physical_memory_map_globals.sound_cache_base_address);
#endif
	return;
}

void *physical_memory_get_game_state_base_address(
	void)
{
	return physical_memory_map_globals.game_state_base_address;
}

void *physical_memory_get_tag_cache_base_address(
	void)
{
	return physical_memory_map_globals.tag_cache_base_address;
}

void *physical_memory_get_texture_cache_base_address(
	void)
{
	return physical_memory_map_globals.texture_cache_base_address;
}

void *physical_memory_get_sound_cache_base_address(
	void)
{
	return physical_memory_map_globals.sound_cache_base_address;
}

/* ---------- private code */
