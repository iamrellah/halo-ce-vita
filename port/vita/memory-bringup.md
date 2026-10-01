# Native memory bring-up audit

## Current integration status (supersedes the initial fixed-address findings)

The Vita physical-memory path now owns four dynamic USER_RW arenas. Game-state
initialization passes the actual pool address, and game_state_allocate_buffer
validates pool identity, nonzero/page-aligned CPU/GPU sizes, and a subtraction-based
bounds check before publishing its borrowed reference. It no longer applies Xbox
write-combine flags on Vita. This does NOT make cached memory GPU-ready: explicit
GXM mapping, cache publication and GPU retirement remain graphics-bridge duties.
Game-state buffer disposal now clears its borrowed reference without calling
XPhysicalFree. Actual allocation ownership stays with physical_memory_free.

Cross compilation passes for physical_memory_map.c, game_state.c and
game_state_xbox.c; nm confirms the latter no longer imports XPhysicalProtect or
XPhysicalFree. Evidence: build/vita/game-state-arena-build.log. Runtime execution
is pending. Saves and map pointers still require relocation/version validation.
The standalone staged v14 package tests physical arena ownership but predates
this game-state consumer change and does not test that module.

Remaining lifecycle gap: inspected shell_dispose does not call game_state_dispose
or physical_memory_free. Do not insert frees before auditing cache workers,
async file requests and renderer retirement; startup-failure unwind also needs
ownership tracking. Other cache modules still depend on Xbox protection policies.

## Original audit

This port is not ready to replace Xita. Source inspection identifies fixed
address dependencies that a malloc-backed XPhysicalAlloc would violate.

- `source/cache/physical_memory_map.c`: Vita currently takes the Xbox branch.
  Game state must equal 0x80061000 (0x345000 bytes), and tags must equal
  0x803a6000 (0x1600000 bytes). Texture cache is another 0x1600000 bytes;
  sound cache is 0x400000 bytes. These are original engine pools, before port
  runtime, graphics resources, stacks and other allocations.
- `source/saved games/game_state.c`: initialization passes literal 0x80061000
  to game_state_allocate_buffer. The Xbox implementation returns the physical
  game-state pool. Save layout and pointer-bearing state need a relocation
  design before changing its address.
- `source/cache/cache_files.c`: loaded tag_header->tag_instances is consumed
  directly. BSP loading writes to reference->base_address, then installs that
  pointer as structure_bsp_header. Relocating only the allocation base would
  not relocate these serialized pointers.
- `port/linux/src/xbox_memory.c`: reserves the Xbox-address window with mmap
  and allocates within it. This is Linux virtual-memory behavior, not a Vita
  memory allocation implementation. It cannot simply be compiled for Vita.

Next steps: inventory all pointer-bearing tag/BSP/save structures and the
existing Xita address translation; determine whether a supported Vita fixed
arena can preserve the required addresses without colliding with executable
or reserved memory. If not, design validated load/save relocation and native
resource address conversion together. Do not remove fixed-address assertions
or return arbitrary allocations as a purported implementation. Keep prototype
saves isolated and require compatible 2342 assets; existing 2276 data is not
validated by this build.

## Comparison with current Xita

The working recompiled runtime (`recomp/xv_x86rt.h` in the authoritative Xita
source) uses g_xram and g_xpt: 4 KiB virtual pages map Xbox guest addresses to
an arena. The 0x80000000 and 0xf0000000 aliases are software translations.
Native C pointers bypass those helpers, so linking that arena implementation
alone would not fix native tag or save pointers.

The installed Vita SDK's public SceKernelAllocMemBlockOpt provides alignment,
base-block references and flags, but no documented requested virtual-address
field. This inspection does not prove that every alternative mapping technique
is impossible; it does mean the Linux mmap implementation has no demonstrated
Vita equivalent. Do not assume one or use undocumented flags as a deployment
strategy without hardware validation.

`python3 port/vita/check_assets.py path/to/b30.map` now performs read-only
header preflight: signatures, version, build, termination and declared tag
bounds. Exit zero means header-compatible only, not playable or fully valid.
Compressed source length can differ from declared uncompressed length; the
checker never follows raw tag pointers or seeks using uncompressed offsets.
Synthetic parser checks covered compatible/mismatched builds and truncation.
No real map or runtime validation occurred in this step.

## Typed relocation coverage

The shared header defines tag_block (count/address/definition), tag_reference
(group/name/name_length/index), and tag_data (size/file_offset/address/definition).
These are useful structure descriptions, but a complete runtime schema registry
is not present in the inspected source. In particular scenario_definitions.c
contains no symbols. Some bitmap, scripting and leaf-map block definitions do
exist. A universal recursive relocation pass cannot assume every definition
pointer in a shipped cache points to usable native metadata.

The read-only checker now offers --tag-index for a compatible, unpacked cache.
It validates the tags signature and directory count/bounds and inventories the
outer header tag_instances plus each entry's name/base_address pointer offsets.
Out-of-region targets are reported, never rewritten. This is explicitly NOT a
complete relocation plan: nested structures, BSPs, GPU resource fields and save
state still require schemas and lifetime treatment. Five synthetic parser tests
pass (test_asset_inspection.py), including build mismatch, count overflow,
compressed-input refusal and preserving unknown pointer targets. No real map
was validated and no original file was modified.

## Installed hardware data verified

Read exactly the first 2048 bytes of ux0:data/xita/haloce/maps/b30.map via FTP,
without modifying it. Header build is 01.10.12.2276, version 5, map name b30.
Physical length is 162,877,440 bytes versus declared uncompressed 224,887,296;
tag region offset 209,169,920 and size 15,716,920. The length difference is
consistent with compressed distribution data; the header alone does not verify
the compressed payload. The native source explicitly requires 01.01.14.2342.
Evidence is build/vita/b30-installed-header-report.json; its header digest
identifies the exact captured bytes. Do not treat the installed map as compatible
or follow its uncompressed tag offset directly in the compressed file.

The standalone test report was also checked over FTP and is absent (550).
That establishes no hardware pass; it does not distinguish never launched from
an early startup failure. Separate-app test execution remains pending user
installation/launch. Existing perf307 and saves were not changed.

## Cross-reference compatibility findings

Checked the private upstream port README and port/linux/README.md: both
explicitly require PAL data build 01.01.14.2342. The checked cache loaders keep
that exact build string; no 2276 selector/converter was found in that runtime
path. The separately cloned bnunu and punpckhdq READMEs also identify 2342.
This is a scoped source inspection, not a claim about every branch online.

The local stianeklund-halo retail cache_files.c corroborates these outer header
facts for 2276: magic at 0/0x7fc, version 5, length at +8, name at +0x20 and build
at +0x40; it explicitly compares against 01.10.12.2276. It also documents a
0x20-byte tag-instance stride and tag count at +0xc. These observations make
header/directory comparison feasible; they do NOT establish nested tag ABI,
resource payload, BSP, script or save compatibility. That reference uses fixed
absolute guest addresses and is not itself a drop-in native ARM module.

Required compatibility evidence before allowing retail assets: inventory all
loaded tag groups from unpacked maps; validate per-group layout/pointer fields,
script opcode/type interpretation, BSP/resource records and save relocation
against the matching retail executable. Keep header build rejection intact
until those consumers are validated or an explicit conversion is implemented.

## Real retail-map outer inventory

Used the existing authoritative Xita recompiler/halo_map.py as an offline data
reader with disk caching disabled. No game code was executed. The local owned
b30.map has the same 2048-byte header digest as the Vita capture (this does not
prove its entire payload matches the Vita). Inflation produced exactly the
224,887,296-byte declared length. The directory contains 3,325 tags in 65 groups
and two BSPs, with no non-null tag-entry base addresses outside the tag region.
Nested pointers are not covered by that statement.

Largest groups: sound 1435, bitmap 467, effect 288, model shader 142, particle
102, model 86. Both BSP file spans fit the inflated image. Complete metadata
and source/reader hashes are in build/vita/b30-retail-inventory.json. Reproduce
with inventory_retail_map.py, an explicit --xita-source and --output. Original
maps are opened read-only; no native compatibility gate changes or conversion
are performed. This replaces speculation about the required tag coverage with
an actual level inventory; per-group pointer/ABI comparison remains required.

## Bitmap layout and serialized-address audit

ARM compile-only bitmap_layout_check.c checks the 108-byte group, 64-byte
sequence, 32-byte sprite and 48-byte bitmap records and relevant field offsets.
The retail b30 inventory (--bitmap-layout) finds 467 bounded groups, 798 sequences,
960 sprites and 663 bitmap records with matching signatures; no typed-span errors.
All 663 candidate pixel spans (group pixel_data.file_offset + pixels_offset,
pixels_size) fit the inflated map. This does not verify mip sizes or pixel content.

Every serialized bitmap base_address is 0x02270040, while every hardware_format
is zero. These are not 663 validated CPU pointers. Upstream
source/cache/xbox_texture_cache.c texture_cache_bitmap_new explicitly clears
base_address/hardware_format, adjusts pixels_offset by the group's file offset,
and recomputes pixels_size. The later cache allocation supplies a new base address
before cache_file_read. Do not relocate the serialized base_address as a tag pointer.
The exact provenance of the repeated serialized value is not established.

Evidence: build/vita/b30-bitmap-layout.json. Six synthetic bitmap-auditor tests
exercise valid layout, invalid nested/block spans, counts, signature, payload
bounds and preservation of unclassified serialized addresses. They test the
inspector only. Next: verify payload sizes and format/mipmap interpretation
against retail, then implement an explicit typed relocation/load path. Native
2342/retail 2276 compatibility remains unproven; build rejection stays enabled.

## Retail mip-size mismatch (offline evidence)

Extended bitmap_inspection.py with the upstream bitmap_get_pixel_data_size
calculation: format bits-per-pixel, compressed 4x4 rounding per mip, volume depth
and six cube faces. This is an offline metadata calculation, not a native runtime
test. All 663 records have valid dimensions/types, supported format IDs and
matching compressed flags. Raw calculated size equals 251 records, is smaller
than 406, and exceeds six by 16 bytes. Rounding each face's complete mip chain
to 128 bytes matches 657/663 recorded sizes. Alignment is an observed candidate
rule, not yet a verified decoder contract.

Exceptions: tag 188 bitmap 0 (128x32 DXT3, 7 mips), and tags 2185, 2206, 3229,
3236, 3248 bitmap 0 (1024x256 DXT3/5, 10 mips). Upstream predicts 5520 vs 5504
and 349584 vs 349568 respectively. All are rectangular compressed 2D images.
The upstream initialization recomputes pixels_size; blindly applying it to these
retail payloads therefore requests 16 bytes beyond their recorded span. This
is not evidence of corrupt maps, nor proof of a runtime fault: retail mip-tail
packing/termination needs investigation. Do not solve this by blindly truncating
reads or rewriting mip counts. Full per-record metadata is in the inventory.

Twelve inspector tests pass, including compressed small mip levels, cube faces,
volume depth and flag/format disagreement. No native compatibility gate changed,
no game deployed and no FPS claim. Next inspect the retail resource-size/mip
address implementation and its smallest rectangular compressed levels.


## Hardware mip layout resolves the six exceptions

Follow-up source inspection found the separate Xbox resource-size contract in
source/rasterizer/rasterizer_swizzle.c, rasterizer_xbox_bitmap_get_pixel_data_size
and rasterizer_xbox_bitmap_get_max_mipmap_count. Compressed hardware mip chains
stop at floor_log2(max(width/4,height/4,depth)), capped by stored mip count;
linear resources have one level with 64-byte row alignment; final per-face size
is rounded to 128 bytes. The inspector now computes this separately from the
generic bitmap size. **All 663 serialized sizes match the Xbox hardware formula.**
This supersedes any interpretation of the previous six generic-size differences
as proof of an incompatible retail layout. No corruption or conversion failure
has been demonstrated by those differences.

The separately reviewed retail texture_cache_bitmap_new source also recomputes
the generic size, like upstream. Its relationship to cache-file reads still
needs tracing; do not patch it just because generic and hardware sizes differ.
Neither matching resource size nor the synthetic tests prove texel ordering,
cube-face ordering, palette handling or Vita-native texture compatibility.

The offline inspector now has 14 passing tests, adding the two actual rectangular
DXT size cases, cube-face alignment and linear-row padding. Next step is a typed
bitmap relocation/load plan that distinguishes tag pointers, file offsets and
transient runtime fields and retains separate generic/hardware size metadata.

## Typed bitmap relocation inventory

bitmap_relocation.py generates a read-only, non-executable plan. It uses explicit
bitmap/sequence/sprite offsets, validates target spans and rejects resident
serialized tag-data pointers or non-null schema definitions rather than guessing.
It preserves file/pixel offsets, identifies tag-block pointer rebases and clears
only classified empty-block/runtime pointer fields. Cache indices, owning tag
indices and cached flags remain the loader's initialization responsibility.

Retail b30: all 467 bitmap groups yield plans without errors: 1,239 nonempty
block pointer rebases, 493 empty block addresses, 1,326 runtime pointer clears,
934 preserved tag-data file offsets and 663 preserved bitmap pixel offsets.
Evidence: build/vita/b30-bitmap-relocation.json, generated with
inventory_retail_map.py --bitmap-layout --bitmap-relocation. No source map bytes
are modified. The plan excludes tag-directory pointers and the other 64 groups;
it must not be used as a whole-map relocation plan. Four synthetic planner tests
cover field classification/no mutation, unknown schema pointers, resident data
and out-of-range targets. Next implement the remaining group schemas and the
transactional native loader, with hardware validation before enabling maps.

## Sound structure inventory

sound_layout_check.c cross-compiles ARM assertions for 164-byte sound groups,
72-byte pitch ranges and 124-byte permutations, including reference/block/data
offsets. inventory_retail_map.py --sound-layout now checks those typed spans,
permutation next indices, and resident tag-data ranges without executing game
code. Retail b30 contains 1,435 bounded sound groups, 1,441 pitch ranges and
7,624 permutations. All next links fit their ranges. Every permutation uses
compression value 1. All 7,624 sample spans fit the inflated file under the
candidate file-offset interpretation. Mouth data: 4,954 resident bounded tag
spans and 2,670 empty records. All subtitles are empty. No non-null schema
definition pointers observed. Report: build/vita/b30-sound-layout.json.

This adds typed coverage but not codec/playback or retail engine compatibility.
Resident mouth data must be relocated; sample file offsets must be preserved.
Four synthetic sound-inspector tests cover these distinctions, next-link errors,
resident overruns and file overruns. Next verify promotion tag references and
cache read semantics before adding sound relocation to the native loader.

## Sound references and cache-read semantics

source/cache/xbox_sound_cache.c sound_cache_start_loading_sound passes
samples.file_offset and samples.size directly to cache_file_read and allocates
a separate cache buffer; these tag_data fields must not be rebased as pointers.
Promotion references were checked by full salted tag ID and snd! group, not just
the low index. All 11 non-null references resolve; 1,424 are null. All 11 retain
valid bounded NUL-terminated names but have name_length zero. The initial strict
length checker rejected these; it now recognizes cleared cache name lengths
while still validating any nonzero length and bounded termination. This is an
observed retail representation, not grounds to strip or rewrite names. Six
synthetic sound tests pass, including full-ID resolution and cached zero-length
names. No game runtime or native asset compatibility gate changed.

## Native relocation primitive (not wired to game loading)

Added halo_vita_relocate_tags in port/vita/src/tag_relocation.c with a typed
rebase/clear plan. It validates the entire sorted plan before writing: 32-bit
address and arithmetic bounds, aligned/nonoverlapping fields, expected original
values, target spans, legal action kinds and disjoint plan/data storage. The
caller must own the buffer exclusively through validation and application; this
is not synchronization for a live shared tag region. Schema correctness remains
a separate requirement. Expected values also reject accidental repeat application.

Standalone platform test v5 includes a valid rebase/clear case, late validation
failure without partial writes, target overrun, duplicate field, overlapping plan
storage and repeated application rejection. It cross-compiles, links and packages;
Clang static analysis reports no diagnostics. These checks have NOT run on the
Vita. The v5 VPK is only built locally, not staged/installed. Existing staged v4
is unchanged. Native map acceptance and installed Halo runtime remain unchanged.

Platform v5 staging follow-up: added explicit unaligned-plan rejection to the
native helper/test, rebuilt successfully and staged the VPK over FTP with full
SHA-256 readback. Receipt: build/vita/platform-test-v5-staged.json. Separate title
XITATST01; installed Xita and saves untouched. Poll of the previous hardware report
returned FTP 550 File not found, so no hardware pass is claimed. Manual VitaShell
install/launch remains required with the current remote capabilities. Awake lease
renewal completed. Prior v4 staged package remains preserved.

## Sound relocation plans

sound_relocation.py now consumes the sound structural/reference audit before
producing per-tag actions. It preserves sample/mouth/subtitle file offsets and
promotion tag IDs, rebases resident data/name/block pointers and clears the
transient cache_base_address field (unknown1, permutation +48). Cache indices
and flags remain initialization responsibilities. Non-null definition pointers
and invalid file spans reject the affected plan. This remains an offline plan,
not wired into the native relocation primitive or map acceptance.

All 1,435 b30 sound groups plan successfully: 7,841 pointer rebases, 7,624 runtime
pointer clears, 22,872 preserved file-offset fields and 1,435 preserved tag IDs.
Report: build/vita/b30-sound-relocation.json via --sound-relocation. Three new
synthetic tests check classification/no mutation, schema rejection and file
span rejection. Seven combined bitmap/sound relocation tests pass. Next combine
and validate patch plans against the C helper's sorted nonoverlapping contract;
partial group coverage must still never enable the entire map.

## Combined partial patch contract

relocation_plan.py normalizes sound/bitmap plans into the native helper's five
uint32 fields. It checks expected bytes against the actual inflated source,
4-byte aligned bounded fields, rebased target identity/span, known actions and
conflicts between preserved fields and writes. Identical shared actions merge;
conflicting actions fail. Source data is not modified and incomplete group plans
are rejected. Both --sound-relocation and --bitmap-relocation emit the normalized
report. Actual b30: 18,523 patches and 25,904 preserved fields, no conflicts.
Artifact: build/vita/b30-partial-relocation.json; 370,460 bytes of patch entries
if serialized, though no binary plan is currently emitted or installed.

Six normalizer tests pass for native field order/deduplication, preserve/write
conflicts, stale expected data, incorrect target identity, overrun and unknown
actions. Coverage is still only two of 65 groups; loadable and
complete_map_relocation remain explicitly false. No hardware/FPS claim.

## Allocator identity propagated to startup consumers

HALO_VITA game_state_initialize now passes the allocated game-state base getter
to game_state_allocate_buffer instead of the literal 0x80061000. The latter
already checks equality with that getter and uses the supplied address for buffer
bookkeeping and GPU subrange protection. Cache write-enable/disable now protect
the tag-cache getter rather than literal 0x803a6000. Both units cross-compile.
This does not yet allocate movable arenas: physical_memory_map.c still owns the
fixed allocation policy, and XPhysicalProtect/GPU mapping require implementation.
Do not remove those calls or claim protection succeeds without implementing it.

The search also confirms serialized tag_header pointers and BSP destinations
remain separate relocation work. Existing raw game-state save files may embed
addresses; isolated native saves must not be loaded across changed bases without
validated relocation. Startup/error cleanup and GPU ownership remain prerequisites
before wiring new allocations into a runnable entry point. No deployment.

## Vita-owned arena implementation (not yet connected)

Added port/vita/src/arenas.c using sceKernelAllocMemBlock(USER_RW) and
sceKernelGetMemBlockBase for the four existing engine arena capacities. It
publishes bases only after all allocations succeed, unwinds in reverse order
on failure, retains failed-free UIDs for retry, rejects duplicate initialization
and hides bases during disposal. It is explicitly single-owner startup/shutdown
code. No live worker/GPU access may overlap disposal. These are cached CPU arenas;
GXM mapping/cache management and Xbox page-protection semantics are NOT supplied.

Local platform test v7 checks allocation/alignment, edge writes, duplicate-init
rejection and repeated disposal. Build/link/package and Clang static analysis
pass; none of these runtime tests has executed. v7 remains local and the staged
v5 package is unchanged. physical_memory_map.c does not yet use these arenas,
so partial map relocation cannot accidentally enter normal game loading.

## Arena GXM mapping ownership

Arena support now exposes explicit GPU READ map/unmap, rejects duplicate mapping
and sound-cache mapping, retains mapped ownership on unmap failure, and refuses
all disposal before freeing anything if any arena remains mapped. It inserts no
GPU finish/wait: caller must retire all GPU references before unmapping. Build,
link and static analysis pass. Standalone tests currently exercise CPU allocation
only; GPU mapping has not run or been qualified. Mapped cached memory still needs
an established CPU publication/cache policy before submissions. This is not a
replacement for resource fences, cache coherency or Xbox page protection. No
startup connection, VPK staging or gameplay deployment in this change.

## Consolidated physical platform test v8 staged

Platform test v8 / SFO 01.08 adds actual GXM initialization and maps the three
render-related arenas READ-only, checks duplicate-map rejection, checks disposal
refuses mapped memory without hiding live bases, unmaps and terminates GXM.
No scenes/draws/display entries are submitted, so no pending GPU work exists in
this lifecycle test. This does not test cached CPU-write visibility or rendering.
Compilation/link/package succeed; VPK copied to ux0:data/xita-platform-test-v8.vpk
with complete SHA-256 readback (build/vita/platform-test-v8-staged.json). Previous
hardware report still returned 550 File not found. Runtime test is pending manual
installation/launch. Installed Halo and save files unchanged. Older packages are
preserved; v8 is the current requested standalone test.

## Decompressor shutdown implementation

The copy worker was an infinite event loop with no exit path. Added Vita-only
cache_copy_dispose_vita, called by cache_files_dispose: with producers already
stopped, wait for copy_complete, publish an atomic shutdown flag, wake the idle
worker, join its thread, then close all five owned thread/event handles. The
existing copy-completion boundary follows cache_copy_wait_for_async_io and source
file closure. The I/O service must stay alive through this join so issuer callbacks
can drain. No serialized structure layout changes. Initialization resets the
atomic flag and rejects a still-owned thread handle.

Initial cancellation approach was corrected during source review: raw header
read/write helpers assert completion rather than tolerate a stop event. Shutdown
therefore waits for the active copy naturally; it does not claim prompt cancellation
or a timeout guarantee. This is only decompressor retirement, not complete cache
teardown. The separate cache_file_windows_thread_proc remains an infinite loop,
and cache_files_dispose still frees its request array without joining that worker;
that path must be fixed before adding physical_memory_free to engine shutdown.
Cross-build of both affected units passes (cache-shutdown-build.log). No engine
or standalone hardware execution covers this yet; staged platform v14 unchanged.

## Cache-read worker retirement

Vita cache_files_dispose now enforces closed-map precondition, joins the copy
worker, then atomically requests cache-reader exit and wakes it. Reader checks
that all request pending/running flags are clear at the exit boundary; violations
abort instead of freeing live buffers. It is joined before its thread/event
handles close and before the request array is freed/nullified. A typed WINAPI
entry wrapper replaces the cast from a void(void) callback on Vita. Init rejects
still-owned state and explicitly zeroes requests before starting the reader.
source/cache/cache_files_windows.c cross-compiles (cache-read-shutdown-build.log).

Draining belongs to the existing cache_file_close -> cache_requests_flush path
while the map handle remains valid; shutdown does not silently cancel queued
reads or close a still-open map. Producers must stop before lifecycle disposal.
Remaining issues: existing request handoff/pending flags use volatile/plain
shared fields rather than a fully audited ARM publication protocol; map-close
flush busy-spins. These need explicit publication and wait behavior, including
failure paths, before runtime qualification. Cached map file handle closure and
partial-initialization unwind also remain open. No whole-engine shutdown or
hardware pass is implied. Staged v14 does not execute these cache workers.

## Cache request publication and non-spinning drain

Vita request pending and priority flags now use release stores/acquire loads.
Submission fills payload, priority and running before publishing pending=true;
completion clears running before publishing pending=false, after which it does
not touch the reusable request record. Slot scans, worker selection, drain checks
and shutdown checks consume pending with acquire semantics. The existing single
engine-side producer assumption remains; this is not a multi-producer queue.

A separate auto-reset completion event replaces full-pool and map-close spinning,
and blocks the busy scan when outstanding reads remain. Callbacks release the
slot then signal the event. Waiters recheck predicates after wakes (including
issuer callbacks); sticky auto-reset signaling avoids the check/wait lost-wakeup
window for one waiter. The event remains alive through worker join and is closed
afterward. No polling timeout or per-frame GPU synchronization was added.

Cross compilation passes; disassembly contains ARM dmb barriers for generated
atomic operations. Static analysis exits 0 but reports TWO existing uninitialized
value paths after match_vassert defaults (precache status and map-selection range).
Those defaults assume non-returning assertion failure; they remain explicit
qualification concerns, not a clean static-analysis pass. Receipts:
cache-publication-build.log, cache-publication-analysis.log and
cache-publication-disassembly.txt. Runtime stress testing is pending. Completion
flags owned by texture/sound callers still use their old volatile contract and
require a separate cross-module audit. Staged platform v14 unchanged.

## Completion flags across cache consumers

Added cache/cache_flags.h with Vita release-store/acquire-load operations and
unchanged plain-access fallback for other targets. Both cache-read completion
callbacks now publish payload readiness with release stores. Audited all four
cache_file_read call sites in this source tree: texture loaded flags (six reads),
sound loaded flags (two reads), tag-load and BSP-load stack completion flags now
use acquire loads. Local async-header completion checks and resets use the same
contract. Existing structure sizes and flag addresses are unchanged.

All four affected engine units cross-compile; evidence:
build/vita/cache-consumer-publication-build.log. This closes the scoped CPU-side
ready-flag handoff, not a complete lifetime/concurrency audit. Objects/flags must
remain allocated until their completion callback publishes; GPU/audio-device
cache visibility is a separate bridge responsibility. BSP/tag waits still call
SwitchToThread (not implemented in Vita platform layer yet), and streaming error
handling still relies on assertion behavior. No native runtime test executed;
staged platform test v14 and installed perf307 remain unchanged.
