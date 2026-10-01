# Native texture integration audit — 2026-09-28

This concerns the PAL2342 native source prototype, not installed perf307.
No hardware speedup or retail-map engine compatibility is established.

## Actual cached bitmap path

`source/cache/xbox_texture_cache.c`:

1. `texture_cache_bitmap_new` clears runtime pointers, converts pixel file
   offsets, and marks the bitmap cached. Serialized base_address is not usable.
2. `texture_cache_start_loading_bitmap` reserves an LRU block and assigns its
   CPU address. It initializes/registers the Xbox descriptor BEFORE submitting
   `cache_file_read`. Registration therefore cannot decode/upload its pixels.
3. `_texture_cache_bitmap_get_hardware_format` touches the LRU entry and waits
   for `CACHE_FLAG_LOAD(&texture->loaded)` before returning the descriptor.
   This is the earliest existing render-owner point for lazy Vita conversion.
4. `rasterizer_set_texture_bitmap_data` passes that descriptor to SetTexture.
5. `texture_cache_locked_block_proc` treats pending reads and resource IsBusy
   as eviction locks. `texture_cache_delete_block_proc` waits for both before
   deleting the datum and clearing the bitmap's cache address.

## Required bridge ownership

- Descriptor registration records layout/address only; it must not read pixels.
- Publish a decoded/uploaded resource only after acquire-load of loaded, and
  validate its complete source span against the allocated cache block.
- Cache conversion once per cache-entry lifetime. Pointer-only lookup is unsafe:
  datum slots/arena addresses are reused. Use full datum identity/generation or
  explicit unregister/invalidation on deletion before reuse.
- Pin converted resources for every submitted scene that references them; retire
  from GPU completion, not SetTexture(NULL), CPU end-of-frame, or display enqueue.
- IsBusy must include outstanding native GPU references. An always-false stub
  permits premature source/destination reuse. An always-true stub deadlocks LRU.
- Delete must dispose the native sidecar before recycling the cache entry, after
  both read completion and GPU retirement. Preserve failed cleanup ownership.
- SetTexture stage changes do not imply upload or conversion on every draw.

## Other paths that cannot be silently treated as cached 2D bitmaps

`rasterizer_xbox_hardware_bitmaps.c` creates managed 2D, cube and volume resources
and updates them through LockRect/UnlockRect. Render-target textures in
`rasterizer_xbox.c` include water, shadows, sun glow, primary/secondary surfaces,
and copies. Code directly edits Xbox Format mip bits for water. These paths
need explicit resource kinds and update/invalidation rules; a raw GXM pointer
cannot substitute for an Xbox D3D descriptor with field accesses.

## Next implementation gate

Current native upload is single-mip decoded ABGR. It is adequate for the isolated
texture test, not the engine's full mip/cube/render-target contract. Build a
render-owned resource sidecar with registration, readiness, identity, and GPU
retirement before connecting cached bitmap binding. Keep volume and unsupported
formats explicit failures until implemented; do not report blanket D3D support.

Hardware test v23-private is staged at
ux0:data/xita-platform-test-v23-private.vpk, full readback verified. It contains
owned bitmap data and must remain private. Remote tools cannot install/launch
this separate title. No platform-test report exists at the expected path yet;
this does not distinguish not launched from an early failure. Installed Halo
perf307 was not interrupted or replaced.
