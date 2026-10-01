# Native D3D/GXM device integration audit

Audited 2026-09-28 against this worktree. This is an integration contract, not
proof of a working native engine or an FPS improvement. Physical Vita remains
on perf307. No game execution on a PC, Pi, or emulator was used for this audit.

## Startup has two device lifetimes

`source/rasterizer/xbox/rasterizer_xbox.c` preinitialize (around line 1845)
creates Direct3D, creates a 640x480 A8R8G8B8 device with D24S8 automatic depth,
reads caps, presents once without drawing, and releases both interfaces.
`_rasterizer_initialize` (around line 2916) creates a second device, installs
palette/state/resources, and eventually `_rasterizer_dispose` releases it.
The Vita shell already calls this preinitialize before allocating game memory.

The Linux `D3DDevice_Release` returns 1 without teardown. Its retained device
cannot be used as the Vita lifetime implementation. Vita's component owner
already supports create/destroy cycles and partial-cleanup retry, but is not
called by the engine D3D entry points. Do not clear ownership after a failed
cleanup or terminate GXM with retained resources.

## Surface aliases are the next integration prerequisite

The engine requests `GetBackBuffer` and `GetDepthStencilSurface`, copies their
D3D headers, and constructs texture aliases by assigning `Data`, `Format`, and
`Size` directly (around lines 3126-3219). One copy aliases the depth allocation
as a color resource. It also creates secondary render-target textures.
A bridge that only connects CreateDevice/Present to the clear-frame prototype
will therefore fail immediately after the capability probe.

Implement a native surface/allocation registry before exposing these calls:

- Preserve the D3D header ABI and distinguish allocation identity from header
  identity. Copied headers must resolve the same allocation.
- Track CPU ownership and GPU/display retirement separately. A Release on a
  header must not free storage still referenced by an alias or queued frame.
- Explicitly validate format reinterpretation, depth/color alias access, and
  render-target transitions. A depth allocation is not automatically safe to
  sample as ABGR. Avoid silently accepting unsupported Xbox aliases.
- Bind the current render slot when consuming backbuffer aliases; never keep
  a descriptor pointing at a slot already recycled for a later frame.

## Resolution and errors cannot be silently ignored

`graphics_device.c` creates 960x544 surfaces; `graphics_present.c` rejects any
other dimensions. Both bring-up draw helpers use a fixed 960x544 viewport.
Halo requests 640x480 and uses Xbox packed surface sizes. Choose and implement
an actual render-target-to-display scaling pass, or deliberately adapt the
engine's screen/target layout together. Merely reporting 640x480 while drawing
at 960x544 does not preserve viewport, HUD, or resource address semantics.

The COM wrapper `IDirect3DDevice8_Present` calls a void Xbox function and always
returns S_OK. `Clear` and GetDeviceCaps have similar wrappers. The temporary
probe also ignores Present's return. Native submission failure must latch a
backend failure that startup/frame code checks; returning an error from a
lower-level GXM helper alone is insufficient.

`global_d3d_caps` is only populated at the two creation sites in the current
engine source; no field reads were found. Do not copy Linux's advertised
volume/lighting/shader capabilities as evidence of Vita implementation.

## Ordered next work and physical verification

1. Surface/allocation identity and alias ownership bridge, including explicit
   rejection of unsupported format/view combinations.
2. Resolution-correct display path and D3D lifecycle adapter; preserve the
   temporary create/present/release followed by main create sequence.
3. Palette, render-state and render-target APIs reached by initialization,
   followed by actual shaders/geometry submission. Never replace real draws
   with the prototype clear frame and call startup complete.
4. Extend the standalone physical test with alias/release/retirement and
   create/fail/cleanup/recreate cases. Its existing tests have compiled but
   have not been verified executing on the Vita.
5. Only then qualify native Halo with owned-map compatibility checks, cold
   launches and real b30 gameplay. Native compilation/import counts do not
   establish successful loading, rendering, or the 20 FPS goal.

## Allocation registry implemented (platform test v34)

`src/allocation.c` now provides a render-owner-only, 128-entry borrowed-storage
registry. Non-recycled IDs, overlap/range checks, exact-format resolution,
separate retained-view/GPU counts, and checked retirement protect reused
addresses and copied views. Exhaustion or ID wrap fails without registration.
Storage owners alone register/unregister; this layer never frees allocations.

`graphics_surfaces.c` registers all six color/depth allocations and exports IDs
through each surface slot. Destroy prechecks every allocation before any
teardown and refuses live aliases or outstanding GPU pins, in addition to its
existing presentation references. Registry extent excludes allocator padding.
Format resolution rejects color/depth reinterpretation; such reinterpretation
still needs a correct conversion/view implementation before Halo can use it.

The v34 standalone package cross-compiles and links without unresolved symbols.
Tests cover copied-range resolution, overlap/format/overflow rejection,
independent CPU/GPU retirement, stale tokens after address reuse, and refusal
to destroy real GXM surfaces while an alias is retained. These are compiled
test cases, NOT recorded hardware passes. No D3D header decoder or automatic
framebuffer alias rebinding exists yet. Next: connect actual D3D surface views
and resolve their packed fields without treating Xbox addresses as Vita GXM
addresses. perf307 is unchanged; no native Halo or performance claim follows.

## D3D header view decoder implemented (platform test v35)

`d3d_view.c` reads the real 20-byte pixel-container prefix (compile-time size
and surface-prefix offset checks), validates 2D/single-mip linear 32-bit color,
decodes width/height/64-byte pitch, bounds-checks the full backing span, and
retains the resolved allocation. Output is unchanged on rejection. Copied
surface and texture headers resolve by storage address rather than header
pointer. Callers must release the returned allocation reference.

ARGB (Xbox linear format 18) and ABGR (63) remain distinct allocation types.
The existing prototype color surfaces use ABGR, whereas Halo's primary target
header is ARGB (`Format=0x11229`, `Size=0x271df27f`: 640x480, pitch2560).
This is an additional reason not to silently expose the current prototype
surface as Halo's requested framebuffer. Depth/swizzled/cube/multimip views
remain rejected here and need their own implementation. Data must already be
a registered native address; no Xbox physical-address translation is guessed.

Built v35 standalone tests for surface/texture copied prefixes, span/pitch
bounds, short headers, output preservation, incompatible channel formats and
unsupported layouts. Link/package succeeded with no unresolved symbols;
no tests were executed on the host, Pi, emulator, or Vita. The decoder is not
yet wired to engine SetTexture/GetBackBuffer; it establishes the checked view
path needed by those entry points. Next bridge work includes format-correct
backbuffer publication and presentation scaling, not reporting probe success
without actual supporting resources.

## Zero-copy color descriptor path (platform test v36)

`d3d_texture.c` acquires a checked D3D view and constructs/validates a GXM
LinearStrided texture descriptor over its existing memory. ARGB and ABGR use
the corresponding GXM swizzles; no pixel allocation, decode, or copy occurs.
Default point/clamp sampler state is local to the descriptor. Failure releases
the temporary view reference and leaves both caller outputs unchanged.

The allocation owner must supply GPU-readable mapped memory with published
writes. Callers must separately track GPU reads and avoid sampling the active
render target. Descriptor construction does not establish synchronization,
perform a render-target transition, or implement SetTexture itself.

`graphics_present.c` now checks outstanding allocation GPU references before
reusing a display-retired slot for writing. CPU view references alone do not
block rewriting. This closes the prerequisite slot-reuse gap for future
framebuffer sampling; automatic per-scene allocation-pin retirement still
needs to be connected before such draws are submitted.

v36 compiles/links/packages with no unresolved imports. Added descriptor tests
over actual native surface storage (pointer, format, dimensions, pitch, and
retained cleanup guard). Tests are compiled only, not physical GPU sampling
proof. perf307 remains installed and no performance result has changed.

## Scene retirement and zero-copy draw (platform test v37)

Presentation now deduplicates allocation IDs per scene and retires their GPU
pins through the same display/GXM retirement path as texture resources.
Shutdown drains before dropping pins; failed retirement preserves ownership.
Active color/depth target IDs are rejected to prevent feedback sampling.
`texture_draw_create_d3d` retains a decoded D3D view and uses its descriptor
in actual GXM texture draw submission, recording the allocation before drawing.
The helper releases its CPU reference on teardown; scene GPU pins remain
independent. No pixel upload/copy is added to this path.

The new physical test allocates/maps an uncached ARGB color fixture, describes
it with an Xbox texture header, draws six frames, tests duplicate scene use
and active-target rejection, drains presentation, checks ABGR pixels in all
three slots, and verifies ownership before freeing the fixture. Compiled and
linked without unresolved imports; NOT executed or passed on physical hardware.

Standalone v37 was staged at ux0:data/xita-platform-test-v37.vpk with full
FTP readback equality. SHA256 c890718dde3cb21474a3616305b1c75e7842135f13d55160420456f1bafdeb6c,
213648 bytes. Manual install/launch is still required. Existing Halo perf307,
saves and settings are unchanged. This is a native texture submission test,
not the full Halo D3D runtime, and establishes no Halo FPS gain.

## Surface publication and engine metadata calls (v38)

`d3d_surface.c` exports a native color slot into a real D3DSurface header,
including its native pointer, ABGR format and packed dimensions/pitch. Export
retains an allocation reference returned separately to the owner; header
release/reference integration is still required before GetBackBuffer can
publish it directly to the engine. Depth headers are not fabricated.

Implemented strong D3DSurface_GetDesc and D3DTexture_GetLevelDesc definitions
using checked view decoding. Current supported case is one-level linear
ARGB/ABGR backed by registered storage. Unsupported calls terminate with an
explicit diagnostic because these Xbox entry points return void, rather than
silently returning a fictitious descriptor. The checked internal API returns
an error without touching output. Usage remains zero as upstream; native
usage metadata is not tracked yet. Current engine GetDesc callers consume
Width/Height for viewport setup and Size for screenshot readback checks.

Audit also found standalone surface headers omit mip count; decoder now
accepts zero only for surface type and still requires one for texture type.
Tests cover exported packed size, retained ownership, exact metadata, and
unsupported mip rejection/output preservation. Standalone v38 links/packages;
not staged or run. v37 remains the latest staged package.

Relocatable engine audit succeeded with all 466 engine units, 514 objects and
527 raw imports remaining. Both descriptor functions are strong T definitions
in partial-engine.o. This remains a partial link, not an executable or a Halo
runtime/FPS result. Existing perf307 remains untouched.

## Managed header ownership (v39) and logical buffer constraint

`d3d_resource.c` owns up to 64 explicitly allocated surface headers, with
checked reference counts and a separate backing-allocation reference. Final
header release frees only the header and drops its CPU reference; copied-view
references and outstanding scene GPU pins independently keep storage alive.
The real D3DResource_Release symbol dispatches only these managed headers for
now. Unsupported/unmanaged resources produce a diagnostic rather than freeing
unknown memory. Other resource classes and engine-created aliases still need
explicit registration/release semantics. No GetBackBuffer endpoint yet.

v39 tests exercise a managed header with two references, an independently
retained copy, and a GPU pin; each is released separately and surface cleanup
must remain refused until ownership is drained. Standalone cross-build and
link succeeded. Physical execution is still unverified; v37 remains staged.

Further engine audit: rasterizer_set_target_as_texture selects one of TWO
persistent primary texture aliases using fps_accumulation_frame_index & 1.
It calls GetBackBuffer only while that alias Data is zero and caches its
address afterward. Therefore publishing a rotating triple-display slot does
not satisfy the engine's logical buffer identity. The next integration must
provide stable logical render-buffer identities (or explicitly update every
engine alias with verified frame semantics) separately from triple-buffered
presentation. The preferred initial path is two logical 640x480 ARGB targets
and a GPU presentation conversion/scale to three 960x544 ABGR display slots.
This costs an additional pass and must be measured; it is not an asserted
performance optimization. Depth-as-color aliases remain a separate unresolved
format/transition requirement. Do not claim managed display-slot headers alone
make the real device adapter complete.

## Stable logical targets (v40)

Added logical_targets.c: two separately owned 640x480 linear ARGB color / tiled
S8D24 depth allocations, sync objects, and a 640x480 GXM render target. Addresses
remain stable and independent of all three 960x544 display slots. Registry
references guard cleanup; partial allocation failures retain cleanup ownership.
No per-frame full GPU wait is introduced. The module does not submit scenes;
callers must pin accesses and wait for actual completion before destroying.

v40 standalone tests allocate both pools concurrently, verify independence and
reference-protected cleanup, recreate the logical pool, and sample a CPU-filled
four-quadrant logical ARGB target through the D3D-header draw path into six
960x544 display frames. Readback checks all four corners of every display slot
for the expected ABGR color/order. This exercises scaling/sampling/lifetimes;
it does NOT yet test an offscreen GPU-write-to-texture-read transition.

Standalone build/link/package succeeded; hardware execution remains pending.
v40 is local only, v37 remains staged, and installed Halo stays perf307.
Next: explicit offscreen scene completion and sampling transitions, then
logical-target-backed D3D device publication. The 20 FPS goal is unmet.

## Offscreen completion and render-to-texture test (v41)

`logical_scene.c` submits logical scenes with fragment notifications using
native-exclusive notification words 0 and 1. Per-target write pins remain
until an acquire-load observes the expected completion value. Busy polls return
1; no frame-loop sceGxmFinish was added. Existing consumer GPU pins block
rewriting. Serial exhaustion fails; uncertain EndScene failure retains pins
and poisons further operations rather than guessing completion.

The shared context now has an explicit display/logical scene owner; nested
begin, context destroy, and presentation shutdown while another scene is
active are rejected. Presentation allocation use refuses an unfinished logical
write. Once ready, its per-scene sampling pin protects subsequent GPU reads.

v41 standalone test draws into both 640x480 logical ARGB buffers, polls actual
fragment notifications, checks CPU readback, then samples the GPU-produced
first buffer into six display frames and checks ABGR output in all slots. It
also tests nested-scene and active-scene shutdown rejection. Cross-build/link
succeeded; NO physical execution/pass is recorded. The test validates this
transition only once per logical target; repeated write/sample coherency and
real engine effects still require hardware qualification. Startup integration,
full D3D compatibility, and the sustained 20 FPS goal remain incomplete.

## Reuse stall correction and repeated transitions (v42)

Added present_reap so logical_begin can retire completed display consumers
without first opening a new display scene. Previously a logical writer could
wait on sampling pins whose release only occurred inside present_begin, which
comes later in the frame sequence. Reaping checks real callback retirement;
it never guesses completion or releases the currently scanned-out buffer.

Added draw_bind_d3d to switch the retained CPU texture view between scenes,
keeping immutable shaders/geometry and previously submitted GPU pins. No
per-frame helper destruction or full-GPU wait is needed to alternate sources.
The compiled physical test now alternates both logical buffers through twelve
GPU render/notification/sample/display cycles before draining and checking
output. It rejects rebind during an active scene. Hardware execution/pass is
still unverified, so this is an implementation/test addition, not a measured
performance improvement.

v42 built/linked and staged with exact FTP readback verification:
ux0:data/xita-platform-test-v42.vpk, 229098 bytes,
SHA256 5e2148605d95234dae881d6431f740131361b211c5ef109ccac1ae77659b9caa.
It supersedes staged v37 for manual platform testing, without replacing Halo
perf307. Installation/launch of this separate title remains manual.

## Logical backbuffer API (v43)

Added a device-owner pair of managed logical color headers and the real
D3DDevice_GetBackBuffer entry point. Open retains one header reference for
each logical allocation; GetBackBuffer retains the selected header for the
caller. Switching selection never changes either header's Data address.
Close refuses outstanding caller references before releasing any owner refs.
Creation/selection/close reject active shared-context scenes. Index 0/mono is
the currently supported Xbox request, matching inspected engine call sites.

Surface export now handles display ABGR and logical ARGB explicitly. v43 tests
exercise A/B/A selection, stable identities and addresses, caller/owner counts,
GetDesc metadata, busy close, final release and unavailable-after-close calls.
Standalone build/link succeeded, with strong GetBackBuffer/Release/GetDesc
symbols. No physical runtime result is claimed; v43 is local, v42 staged.

Device initialization still must open this owner pair and Present must select
the correct next logical target. SetRenderTarget must also preserve the
engine's cached primary-surface semantics rather than blindly honoring an
old physical header. Depth API, depth/color reinterpretation, and the complete
D3D lifecycle are unfinished. Existing Halo perf307 remains unchanged.

## Integrated device ownership and presentation (v44)

The native device owner now creates/destroys logical targets, managed
backbuffer headers and the reusable texture scaling draw alongside display,
context and shader resources. Cleanup drains presentation first, releases the
scaling view, closes backbuffer ownership, retires completed logical writes,
and only then destroys target storage and GXM prerequisites. Retained caller
headers prevent final cleanup; retry remains possible after caller release.

Added device_present_logical: it polls the selected target, binds its actual
surface header, submits scaling into an available display slot, and advances
the two-buffer selection only after successful submission. Pending target or
display work returns 1 without advancing. Active scenes and other failures
return errors. No per-frame full-GPU wait is added.

v44 standalone tests extend both device create/destroy cycles with six logical
presentations, header/address alternation and cleanup refusal/retry while a
caller retains a backbuffer. Build/link/package succeeded. These are compiled
checks, not hardware results. v44 remains local; v42 is staged. The real
Direct3D_CreateDevice/Present/Release lifecycle adapter remains to be wired,
along with depth APIs, game draw/state/shader handling and asset compatibility.
Installed Halo perf307 and its measurements are unchanged.

## Engine D3D lifecycle adapter (v45)

Added real Direct3DCreate8, Direct3D_CreateDevice, GetDeviceCaps, Present and
Release definitions. Creation accepts the inspected 640x480 ARGB/D24S8 probe
configuration and creates the integrated GXM owner. Unsupported dimensions,
multisampling, windowing and interval TWO/IMMEDIATE are rejected rather than
silently mapped to another policy. Default/ONE are supported. Caps remain
conservative (HAL type, no claimed unimplemented Xbox shader/volume limits).
Opaque singleton interface identities match Xbox wrappers, not desktop COM.

Present samples the selected logical target through the implemented GPU path;
it retries target/slot pending states with a 1 ms yield, no full-GPU Finish,
and diagnoses a prolonged stall. Void API errors terminate with diagnostics;
Release refuses outstanding ownership rather than freeing live resources.

Standalone v45 builds/links and contains probe-configuration validation,
duplicate-create rejection, and two create/caps/present/release cycles. Not
executed on hardware. Engine aggregate and startup reachability audits also
succeeded. Startup-engine.o now retains Direct3D_CreateDevice,
D3DDevice_Present, halo_vita_device_create and device_present_logical: the
native GXM device is connected to real engine startup paths, not just tests.
This is still an unresolved partial engine link. logical_begin is not yet
reachable from engine draws: game SetRenderTarget/state/shader/draw APIs and
depth handling remain unfinished. Do not confuse startup reachability with
running or rendering Halo. Local v45; staged v42; installed Halo perf307.

## Managed depth surfaces (v46)

Backbuffer ownership now includes two corresponding managed depth headers.
Implemented D3DDevice_GetDepthStencilSurface with caller reference retention,
NULL/error handling, and selected-buffer identity. GetDesc understands their
640x480 LIN_D24S8 API metadata. Native backing remains GXM tiled S8D24 and is
registered as that distinct layout; pitch here is logical row size, not a CPU
mapping guarantee. Lock/readback and depth-as-color conversion are not yet
implemented. The color texture descriptor path rejects these depth headers.

Tests acquire depth, verify metadata/address/registered allocation identity,
check color-sampling rejection and release the caller reference. Device cleanup
owns all four color/depth headers and refuses any retained caller reference.
v46 standalone builds/links/packages; no physical test execution. It remains
local; staged v42 and installed Halo perf307 are unchanged. Next engine-facing
work is render-target selection and real viewport/clear/state/draw semantics,
including the cached-primary-header behavior across presentation flips.

## Primary render target and viewport entry points (v47)

Implemented SetRenderTarget for managed matching primary color/depth headers,
with Halo-specific cached-primary semantics: the pair selects the CURRENT
logical buffer, while copied Data values retain their physical identity for
sampling. Secondary targets, null-depth mode and unmanaged/mismatched pairs
are explicitly unsupported. Changing to an unsupported target never silently
renders into the primary buffer.

Implemented checked SetViewport caching and GXM application on logical scene
begin or viewport change. Bounds/depth-range checks reject invalid or NaN
values without modifying state. Present ends the active matching logical
scene, then follows notification polling and scaling submission. Cached
headers can be reused across flips without rendering repeatedly into buffer 0.

v47 standalone build/link passes with cached-header reuse across four flips in
each D3D device cycle and checked invalid viewport/target cases. Aggregate and
startup-link audits pass; real startup now retains SetRenderTarget,
SetViewport AND halo_vita_logical_begin. Hardware execution remains pending.
The test still uses the bring-up fixed-color draw inside those scenes; general
D3D Clear, render states, shaders, geometry and secondary targets remain
unfinished. v47 local, v42 staged, existing Halo perf307 unchanged.

## Native Clear path (v48)

Implemented count=0/no-rectangle D3DDevice_Clear, matching all inspected engine
clear call sites. Xbox per-channel bits map to cached GXM fragment color masks,
including alpha-only clears in fog/frame handling. Depth/stencil flags control
their respective writes, with stencil replacement and depth independent of the
cached viewport MinZ/MaxZ. Unsupported rectangle lists fail explicitly.

Clear geometry now has two bounded 64 KiB regions, one per logical buffer,
with immutable records per call (1024 maximum before explicit failure). A
region resets only after the prior logical write fence/consumer ownership
allows a new scene. CPU writes are uncached and published before submission;
no per-clear allocation, overwrite of pending records or full-GPU wait occurs.
Shader mask variants are created lazily and released at shutdown.

v48 adds a full clear followed by alpha-only clear and expected ARGB readback,
repeated across cached-header flips. Standalone cross-build/link succeeds, not
a hardware pass. The compiled readback covers color preservation; arbitrary Z
values and stencil values still need independent depth/stencil qualification.
Viewport state is restored after clear. The future game draw state cache must
rebind pipeline/depth/stencil state after clears; general draw/state support is
still unfinished. Local v48; staged v42; Halo perf307/FPS unchanged.

## Fullscreen stencil isolation (v49)

Both the fixed-color fullscreen helper and display texture scaling helper now
explicitly disable two-sided stencil and set front stencil ALWAYS/KEEP with a
zero write mask. Previously they inherited prior stencil state: a rejecting
stencil function could hide the output, and a preceding stencil clear could
leave replacement enabled during presentation. Depth writes remain disabled.

The standalone presentation tests now seed front AND back stencil NEVER before
each fullscreen submission; existing color readbacks must still match after
GPU retirement. These are compiled regression tests, not hardware results.
v49 cross-build/link/package succeeds. No native hardware execution or FPS gain
is established. v49 remains local, staged test remains v42, and installed Halo
remains perf307. General game render-state rebinding remains unfinished.

## Engine render-state capture (v50)

Added the 144-entry D3D__RenderState table and native capture entry points for
simple SDK methods, deferred states, specialized setters and NotInline calls.
The simple mapping follows all 82 January SDK method encodings. Direct simple
calls also store their value; inline wrappers can safely repeat that write.
Unknown simple methods fail explicitly. Deferred/NotInline out-of-range states
retain upstream ignore behavior. Defaults and ZBias-derived slope/constant
values follow the CC0 upstream d3d8_gl.c implementation, without OpenGL calls.
Device recreation resets the table.

This is CPU state preservation only. No general game draw API yet applies the
table to GXM, and shader/blend/texture behavior is NOT implemented by storing
these values. Draw-time state inspection must account for inline wrappers and
shader setup writing the table directly: setter-only dirty flags are unsafe.
Clear and presentation helpers must not mutate the saved game state table.

v50 standalone compilation/link/package succeeds. Added compiled assertions
for default depth state, SDK method decoding, ZEnable, and the decal ZBias=8
values (-2 slope, -8 constant) plus reset. Hardware execution pending. Complete
466-unit engine aggregate links relocatably with 521 objects, 489 unresolved
imports, zero unresolved weak symbols; this remains an incomplete executable.
v50 local, staged test v42, installed Halo perf307; no measured FPS change.

## Depth/stencil application (v51)

Added checked decoding of the saved Xbox depth/stencil table to explicit GXM
enums. All eight comparison functions and stencil operations are mapped;
saturating INCRSAT/DECRSAT are distinct from wrapping INCR/DECR. Disabled depth
testing uses ALWAYS with writes disabled even if ZWRITEENABLE is set. Disabled
stencil uses ALWAYS/KEEP and zero write mask. Reference/read/write masks are
restricted to the native eight-bit stencil. W buffering and invalid enabled
states fail; invalid disabled substate is ignored. Failed decoding leaves the
output untouched and emits no partial GXM state changes.

D3D Clear now restores saved game depth/stencil state after its own writes and
viewport restoration. This is the first application of the captured table;
general geometry submission still needs to call it before draws. Culling,
fill/depth bias, blending, shaders and texture state remain separate unfinished
work. The helper applies state every time for now: direct inline table writes
and clear/presentation state changes preclude setter-only dirty tracking.

v51 standalone cross-build/link/package passes. Compiled tests cover default
state, disabled depth writes, saturating versus wrapping increment, eight-bit
reference, transactional rejection, disabled invalid state and unsupported W
buffering. Existing repeated Clear tests exercise restoration calls. None are
claimed as physical test passes; arbitrary depth/stencil pixel behavior is
still unverified. v51 local, v42 staged, Halo perf307 unchanged; no FPS gain.

## Blend descriptor translation (v52)

Added checked Xbox-state to SceGxmBlendInfo decoding: per-byte Xbox channel
write bits map explicitly to GXM masks; disabled blending uses NONE; add,
subtract, reverse subtract, min and max map to native equations. Source/dest
color factors use corresponding alpha factors for the alpha equation, and
SRCALPHASAT uses ONE for alpha. Min/max ignore scale factors. Output remains
unchanged for unsupported values, including signed equations and constant
color/alpha factors. This decoder is ready for fragment-program variant keys;
general game fragment creation/submission is not yet wired to it.

Static source audit confirms constants are REQUIRED: transparent_geometry.c
around 3385/3390, screen_effect.c 1294/1315/1333/1342 and dynavobgeom.c 438.
GXM BlendFactor exposes no direct constant-color/constant-alpha counterpart.
These cases require shader/multipass design that preserves source and
destination terms; do not approximate them as ONE or disable blending.

v52 standalone builds/links. Compiled tests cover alpha masks, conventional
alpha blending, saturate's alpha behavior, min semantics and transactional
failure for constants/signed equations. Hardware pixels remain unverified.
v52 local, test v42 staged, Halo perf307 installed; no FPS claim.

## Blend/depth pixel qualification path (v53)

Connected the blend decoder to a bounded 32-entry fragment variant cache in
the color bring-up helper. Keys contain the complete four-byte GXM blend
descriptor. Repeated descriptors reuse programs; variants are never evicted
while submitted draws may reference them, and are released after shutdown GPU
retirement. Capacity exhaustion fails explicitly. This is a qualification
helper, not a production general-shader cache or Halo DrawPrimitive API.

The color quad uses captured depth/stencil and blend state with immutable
per-logical-scene ring geometry. Its viewport remains caller-controlled and
culling is explicitly disabled for this fullscreen test. The device test now
clears to ARGB 7f204080, adds RGB 010203 while preserving alpha, then issues a
white additive draw under depth NEVER. Expected retired readback is 7f214283.
This exercises actual GXM program variants, channel masking and depth rejection
rather than only enum decoding; four alternating cached-header flips repeat
it. The test is COMPILED, NOT RUN on hardware.

v53 standalone build/link/package succeeds. Native Halo geometry, shaders and
constant-factor blend cases remain unfinished. v53 local; v42 staged; installed
Halo perf307 unchanged. No native runtime pass or FPS improvement is claimed.

## Physical staging of v53

Copied standalone XITATST01 version 01.53 to
ux0:data/xita-platform-test-v53.vpk and verified the complete FTP readback.
259819 bytes; SHA256
733567cd488a7dd70bff4459a12eb01cabff02b2b0d147460ca4fb8d0ef8303b.
Receipt: build/vita/platform-v53-staged.json. Installation/execution still
requires VitaShell/LiveArea interaction; the existing Xita remote endpoint
cannot install or launch this separate title. The report lookup returned FTP
550 File not found, so there is no native hardware pass to report. Existing
Halo installation/saves remain untouched. Keep-awake lease renewed 3600s.

## Cull state restoration and qualification (v54)

Native cull decoding explicitly maps Xbox NONE/CW/CCW to GXM, matching the
existing source/runtime/xv_d3d.c submission mapping. Xbox cull names winding
to discard; FRONTFACE is not another inversion of CULL. Clear restores saved
culling, and the color qualification quad now consumes saved culling rather
than forcing NONE. Unknown cull values fail without changing decode output.

The compiled device test draws an extra RGB 010101 under each complementary
CW/CCW mode after the blend/depth checks. Exactly one must affect a pixel,
yielding ARGB 7f224384. This checks complementary rejection only: absolute
orientation with the eventual game vertex shader/viewport remains a separate
hardware requirement. Two-sided shading/fill modes are not implemented here.

v54 standalone build/link passes; hardware execution remains pending. v54 is
local, v53 remains staged for installation, Halo perf307 remains installed.
No new runtime or frame-rate result.

## Geometry topology bridge (v55)

Audited real Halo stream/index calls in rasterizer_xbox_draw_primitives.c and
QUADLIST detail-object submission in rasterizer_xbox_detail_objects.c. Added
bounded caller-owned index conversion: quads use upstream diagonal 0,1,2 / 
0,2,3; strips/loops expand to lines; supported triangles, fans and points keep
order. Quad strips follow upstream's triangle-strip representation. No vertex
transforms, per-call allocation, material sorting or submission reordering.

Capacity, integer range, malformed primitive count, sequential 16-bit overflow
and overlapping input/output fail before writes. Callers still must prove
input readability and vertex-stream extents; no arbitrary pointer validity is
claimed. Output must be allocated/published in GPU-readable frame storage and
kept alive to retirement before general engine draw submission is connected.

The color qualification quad now actually draws the six converted indices
from immutable mapped geometry, exercising the converter's topology in the
existing blend/depth/cull pixel test. Compiled additional cases check indexed
quad order, line-loop closure, output preservation on insufficient capacity,
alias rejection and sequential range. v55 standalone build/link passes; no
hardware execution yet. v55 local, v53 staged, installed Halo perf307 unchanged.

## Retained vertex stream bindings (v56)

Implemented engine D3DDevice_SetStreamSource for 16 slots backed by registered
native VERTEX_BYTES allocations. Binding retains storage and copies the data
address/stride, so later header reuse cannot invalidate the binding. Rebinding
retains the new allocation before releasing old ownership; failed binding
preserves old state. Unbind requires NULL/zero stride. Device Release unbinds
streams before destroying the native device.

Added draw-view acquisition checking first vertex, count, element width,
stride, arithmetic overflow and exact registered backing range. Success owns
an additional CPU reference; failed acquisition leaves output unchanged. It
does not claim that CPU refs alone protect GPU reads: draw submission must pin
and attach retirement to the logical scene. Generic vertex allocation,
Register/Lock, cache publication and those submission pins are still needed.
Unregistered Xbox addresses fail explicitly rather than being passed to GXM.

v56 standalone cross-build/link succeeds. Compiled tests check stream range,
overflow, rejected wrong resource type, and references surviving unbind.
No hardware execution/FPS gain established. v56 local, v53 staged, Halo perf307.

## Logical-scene read ownership (v57)

Added bounded per-logical-scene allocation read sets (128 entries each).
Submission users must retain a CPU view, then call logical_use_allocation
before GPU commands. Duplicate uses share one scene GPU pin. Current color or
depth target feedback is rejected; source logical writes must complete before
sampling. Capacity/invalid-state failures acquire no pin. Reads retire only
when the scene fragment notification matches, along with target write pins.
Failed EndScene or retirement retains/poisons ownership rather than guessing
that GPU use stopped. No full-GPU wait or lock was added to submission.

Compiled lifetime test attaches a metadata-only stack allocation to a real
logical scene, drops its CPU reference, verifies unregistration fails, ends
and polls the scene, then verifies unregistration succeeds. Repeated use checks
deduplication; current-target feedback and use outside a scene are rejected.
The GPU does NOT fetch that stack data: this tests ownership accounting, not
vertex content/cache visibility. Real vertex draw wiring remains unfinished.

v57 standalone build/link passes, physical tests still pending. v57 local,
v53 staged, Halo perf307 unchanged; no FPS improvement claimed.

## Owned GPU-readable buffers (v58)

Added USER_RW_UNCACHE buffer ownership with page-aligned allocation, GXM READ
mapping and registry extent equal to requested bytes (allocator padding is
not readable vertex data). Destruction first requires registry references to
be gone, then unmaps/frees; failures retain remaining ownership for retry.
Writes use uncached memory and explicit publication barriers. Callers remain
responsible for not overwriting in-flight ranges.

The clear/color geometry allocation now uses this owner. Actual color-quad
GPU reads pin the allocation into the logical scene read set, so the lifetime
test covers fetched geometry as well as metadata. Existing two-region clear
ring fencing protects writes to reused ranges; a whole-allocation read pin
protects destruction, not arbitrary CPU writes. Shutdown rejects active scenes,
finishes only during teardown and polls logical notifications before freeing.

v58 standalone build/link passes. Compiled buffer tests exercise exact 65-byte
extent within page padding, busy destruction, release and repeated cleanup;
they run inside the initialized-GXM test section. Hardware execution remains
pending. Engine CreateVertexBuffer/Lock/Release integration and general draw
submission are still unfinished. v58 local, v53 staged, Halo perf307 installed.

## Engine-owned vertex buffers (v59)

Implemented CreateVertexBuffer for the observed Halo usage (nonzero length,
FVF zero, default/managed pools, optional WRITEONLY/DYNAMIC hints). Headers use
native mapped addresses and are tracked separately from storage. Resource
Release now dispatches owned vertex headers: the final caller release marks
it retired, but storage stays alive while stream views or GPU scene pins
remain. A bounded collector frees only reference-free retired entries during
creation, presentation and teardown. No forced GPU wait was added.

Corrected shutdown ordering: the native device collects buffers before GXM
termination and refuses teardown while caller-held vertex resources remain.
Compiled lifecycle tests now keep a vertex buffer alive through attempted
device destruction, release it, and retry. Other compiled tests release a
buffer while bound and while a draw view is retained, confirming allocation
identity persists until the last reference is gone. These are not hardware
passes. Lock/Register, index resources and general draw APIs remain unfinished.

v59 standalone rebuild/link passes, and the complete 466-engine-unit aggregate
relocatable audit succeeds (see native-v59-link-audit.log). This is not a final
Halo executable. v59 local; v53 staged; installed Halo perf307 unchanged.

## Bounded vertex locks (v60)

Added owned vertex-buffer Lock with checked offset/size (size zero means
remaining logical extent), header identity and supported flag validation.
Returns a pointer only while no GPU read pins exist. READONLY/NOOVERWRITE do
not bypass hazards: Halo's dynamic pointer cache returns READONLY on later
locks (draw_primitives.c ~901), so that flag alone cannot establish absence of
writes. Per-byte-range NOOVERWRITE tracking remains needed for efficient
same-buffer suballocation. This conservative implementation may reject a
valid disjoint-range access while another range is in flight.

The void Xbox entry point polls completed logical scenes with bounded 1ms
yields when no scene is active. Active-scene hazards fail diagnostically rather
than waiting for this same thread to EndScene. No full-GPU Finish was added.
Native uncached writes publish before each logical allocation use, including
deduplicated uses; external cached allocations still require their own clean.
Unlock remains the original inline no-op; submission owns publication.

Compiled tests check remaining-size locks, data preservation, bounds, invalid
flags and pending GPU pins for both READONLY and NOOVERWRITE. v60 standalone
build/link succeeds. No physical validation; engine Register/index/shader/draw
integration still incomplete. v60 local, v53 staged, installed Halo perf307.

## Engine buffer-to-GXM qualification draw (v61)

The color qualification path can now fetch four vertices directly from a bound
stream (float3 clip position plus packed color, stride 16). It acquires checked
stream extents, pins source storage into scene completion ownership, and binds
that pointer to GXM without copying/translating vertex contents. The existing
immutable converted quad indices and blend/depth/cull state path are reused.
The helper is explicitly layout-limited; it is not Halo's general DrawVertices
or programmable shader implementation.

The integrated device test now calls CreateVertexBuffer, Lock, SetStreamSource,
submits an additive green quad, releases the resource and unbinds BEFORE scene
completion. Storage must remain GPU-busy until the fragment fence retires,
after which collection must remove its allocation. Expected center ARGB is
7f224484 (including preceding blend/cull checks). An offset that overruns the
four-vertex allocation must reject without an extra draw. Four flips repeat.

v61 standalone compiles/links/packages. This is the first compiled integrated
test that fetches vertices through the new engine buffer APIs, but remains
unexecuted on hardware. v61 local; v53 staged; Halo perf307 unchanged. General
shader declarations, index buffers and game draw integration remain open.

## Index resource creation and binding (v62)

CreateIndexBuffer now creates registered INDEX_BYTES storage for even, nonzero
16-bit index data. Vertex/index resources share deferred ownership collection
but retain distinct registry types; an index header cannot bind as a vertex
stream. SetIndices retains backing storage and publishes D3D__IndexData for
Xbox inline draw wrappers while preserving base_vertex separately. Checked
index-view acquisition validates first/count arithmetic and allocation bounds.
Unbinding and device stream reset release index ownership too.

Important remaining hazard: the Xbox IndexBuffer_Lock is inline pointer
arithmetic, so CPU writes can bypass a native locking entry point. Deferred
submission must snapshot selected indices into frame-owned mapped storage;
retaining this mutable buffer alone does not make zero-copy submission safe.
Base vertex must be applied to stream addressing without 16-bit wrap. Neither
of these requirements is claimed complete by the binding implementation.

v62 standalone build/link passes. Compiled tests check odd-length rejection,
resource type isolation, index contents/base vertex, out-of-bounds rejection
and released storage retained by acquired views. Physical execution pending.
v62 local, v53 staged, installed Halo perf307 unchanged.

## Immutable per-scene index snapshots (v63)

Added two preallocated 256 KiB index arenas, one per logical target slot.
Snapshotting validates the bound source range, records base/min/max indices,
rejects base+max overflow and performs topology conversion directly into the
next aligned arena range. It then publishes uncached writes and pins the arena
until scene completion. Source indices can be rewritten or released afterward.
No per-draw allocation, index truncation, forced Finish or in-flight eviction.
Capacity exhaustion fails explicitly; production overflow handling is pending.

Arena reset occurs only on the existing logical begin path after previous slot
completion. Device teardown retires scenes before destroying the arenas.
Compiled tests snapshot a quad, overwrite its source, snapshot again and check
that the first six indices remain unchanged at a distinct address; busy arena
destruction is rejected. These tests do not yet submit the snapshots to GXM.

v63 standalone builds/links. Physical execution pending. Next: consume these
snapshots in the indexed qualification draw, including base-vertex addressing.
v63 local; v53 staged; installed Halo perf307 unchanged.

## Indexed GPU qualification (v64)

Connected per-scene snapshots to GXM indexed color draws. Base vertex offsets
the checked vertex-stream pointer; 16-bit index values remain unchanged, with
stream extent covering maximum index + 1. Snapshots pin their index arena and
source vertex views pin storage through scene completion. The helper remains
limited to the known float3+color shader layout, not general Halo shaders.

The integrated pixel test now inserts a dummy vertex before the quad, draws
with base_vertex=1, overwrites all original indices immediately afterward,
unbinds/releases the source index buffer and releases/unbinds the vertex
buffer before EndScene. Expected retired center is ARGB 7f224584. A wrong base
vertex or direct mutable-index submission should fail this pixel expectation.
This extends the earlier direct-stream draw; it is compiled, not hardware-run.

v64 standalone builds/links. General vertex declarations/shader translation and
engine DrawVertices entry points remain unfinished. No native runtime/FPS pass.

## Vertex shader reuse audit after v64

Offline audit against the locally owned default.xbe establishes 67/67 exact
byte-for-byte matches for BOTH native January vertex program microcode and its
assigned vertex declaration against existing Xita layout descriptor addresses.
FNV/size was used only to shortlist; section-bounded XBE reads compared complete
program and declaration bytes. Report: build/vita/vertex-shader-reuse.json.
Reproduce with audit_vertex_shader_reuse.py --xita-source SOURCE --xbe XBE
--output REPORT. It outputs hashes/identity metadata, never shader payloads.

This supports reusing the existing generated GXP/layout pipeline rather than
writing a new NV2A vertex translator. It does NOT prove correctness of those
translated programs on the new native path. Integration still needs: native
patcher ownership, program/declaration lookup by content (not fixed guest VA),
uniform window mapping including negative constant registers, persistent
attributes, stream format handling and shader lifetime through scene fences.
Existing xv_shader.c has its own global patcher and embedded program plumbing;
do not initialize a second patcher or blindly transplant its global lifecycle.
The original asset/map build compatibility gate remains open despite these
shader identities. No new executable, deployment or FPS result this audit.

## Vertex constant bank and upload bridge (v65)

Implemented 192 float4 registers indexed -96..95, matching upstream's bank.
SetVertexShaderConstant preserves bit patterns and clips writes at the upper
edge like upstream, with comparisons before arithmetic to avoid signed
overflow. Added checked copies and upload into a caller-reserved GXM vertex
uniform buffer; do not reserve a separate buffer for each constant subrange.

Viewport changes populate reserved c[-38]/c[-37] for current D24S8 targets,
including negative Y scale. NORESERVEDCONSTANTS suppresses those writes; changing
back restores the cached viewport values. Device recreation resets the bank.
Temporary Clear viewport changes do not overwrite the game's constant bank.
Unknown constant modes fail explicitly. The reused translated shader's exact
uniform window and viewport correction still need integration/verification.

v65 standalone builds/links. Compiled tests cover negative indices, end-range
clipping, extreme invalid indices, viewport constants and reserved-mode restore.
No physical shader output validated. v65 local, v53 staged, Halo perf307 remains
installed. No FPS gain from this uninstalled native prototype is claimed.

## Existing vertex GXP catalog loading (v66)

Added a private build generator for all 67 existing Xita vertex GXP programs
and their generated layouts, retaining the Xita GPL provenance. Payloads and
input hash receipts go only under build/vita; do not commit/distribute derived
shader artifacts without the project's existing asset policy. Native catalog
loading uses the existing native shader patcher, validates envelope/layout
bounds, maps linked attributes, and sets up the optional instance-indexed
persistent-attribute stream. It retains/releases patcher ownership and unwinds
partial registration failures. No second global patcher is initialized.

The standalone test now attempts all 67 registrations/vertex-program patches
and releases, checking the patcher cannot be destroyed while a program owns it.
v66 compiles/links/packages, but this loop has NOT run on physical hardware.
Original program/declaration identities were separately established by the
byte audit; GXM acceptance, shader output and game visuals remain unverified.

This is catalog loading only. Runtime content lookup/handles, constant upload,
persistent-attribute snapshots, fragment links and general game draws remain
unfinished. Existing layouts may describe expanded vertex representations;
matching the declaration does not establish raw packed Xbox bytes can be bound
without the original preparation path. v66 local, v53 staged, Halo perf307.

## Vertex shader binding contract audit (v67)

The catalog loader now checks GXP program type, linked attribute category,
duplicate input register bindings, and the constant bank's float4 uniform
shape/capacity before registering a program. Missing compiler-eliminated
parameters remain allowed. This prevents treating a fragment program or an
undersized/non-float constant parameter as the catalog's vertex contract.
A shortened compiler constant array is rejected rather than uploading beyond
its declared capacity; hardware qualification must establish whether any entry
needs a smaller explicit upload window.

v67 cross-compiles, links with no unresolved executable symbols, and packages;
the existing 67-program registration test has not run on Vita. No new physical
rendering or FPS improvement is established. Native v53 remains staged and
Halo perf307 remains installed. Renewed the remote keep-awake lease for 3600s.
Next: qualify the catalog on hardware and integrate runtime shader selection,
packed vertex preparation and constant binding into the real draw path.

## Runtime shader identity lookup (v68)

The private catalog now includes the native engine's original program and
terminated declaration bytes. A bounded lookup compares both complete spans,
leaves output unchanged on failure, and makes no assumption about Xbox virtual
addresses or native pointer locations. Generator rejects incomplete tables,
invalid instruction extents, unterminated declarations, and mismatched catalog
program hash/length. Existing offline byte-comparison receipt confirms each of
67 native entries also matches its same-numbered existing catalog entry.
Generated original bytes remain build-only private artifacts.

v68 standalone cross-compiles, links and packages; generator checks passed for
all 67 entries. Runtime lookup is not yet wired to D3D CreateVertexShader and
has not executed on hardware. Handle lifetime/selection and in-flight shader
retirement still need integration before actual Halo draws. No deployment or
FPS change; installed Halo remains perf307, native v53 remains staged.

## Engine-facing vertex shader handles and packed slots (v69)

Implemented D3DDevice_Create/Delete/Set/Load/SelectVertexShader and
GetVertexShaderSize. Create matches complete native declaration/program bytes,
loads the translated GXP through the native patcher, and publishes a unique
even handle only on success. Unknown programs, usage flags and declaration
token classes outside Halo's stream/data declarations fail explicitly. The
original pointer API requires readable terminated inputs; the internal matcher
uses explicit extents. There are 128 simultaneous handles and 67 cached programs.

Native runtime inspection revealed Halo's packed shader path selects previously
loaded programs with handle zero. Implemented 136 instruction slots, checked
program extents and overlapping-load invalidation. Declaration selection stays
separate from instruction selection; the current-program resolver matches the
combined program/declaration against the catalog and rejects unknown pairs.
Deleting handles does not invalidate already loaded/selected instructions or
free GPU programs. Programs remain resident until device shutdown has drained
presentation/context work and polled both logical slots. Device teardown then
releases them before destroying the shader patcher. Handle IDs never reset,
preventing stale handles from aliasing a later device's shader.

The standalone test now exercises exact identity lookup, duplicate creation,
size queries, handle-zero selection, deletion of selected handles, unique
replacement IDs, overlap invalidation and shutdown retention. These tests are
compiled only. v69 compiles, links with no unresolved executable symbols and
packages; nm confirms all six engine-facing functions are defined. No physical
execution, full Halo draw path, or FPS improvement has been established. Native
v53 remains staged, perf307 remains installed. Next is constant/stream binding
and packed-input preparation before generic native draws can consume selection.

## Raw stream layout validation and vertex binding (v70)

Corrected the earlier concern about required CPU expansion: all 67 native
stream declarations match existing catalog offsets, strides, storage formats
and components. NORMPACKED3 is U8x4 in these layouts, with decoding in the
translated vertex shader; it is not expanded float3 storage. Added an offline
validator to catalog generation so a future format/stride mismatch stops the
build. All 67 pass; injected U8N-to-U8 and stride mutations are rejected.
This proves the descriptor storage contract, not physical shader output.

Added vertex binding for the selected shader inside a logical scene. It
acquires bounded stream views, requires actual stride to match the patched
program, pins only streams used by linked attributes, binds raw pointers,
and reserves/uploads the shader's constant window once. Constant-fed input
requires a separate caller-owned immutable 256-byte register snapshot, checked
against the allocation registry and pinned through scene retirement. Failed
setup never authorizes a draw; already-acquired GPU pins still retire normally.
No CPU position/normal expansion is introduced by this binder.

Standalone coverage now includes binding outside a scene, rejecting an
out-of-range vertex span, valid stream/uniform binding, preventing shader
shutdown in an active scene, and releasing/unbinding a buffer before its fence
retires. These tests compile but have not run on hardware. v70 builds/links/
packages successfully. It still lacks the persistent-attribute snapshot producer,
fragment selection and generic Halo draw submission. No deployment/FPS claim;
perf307 installed, native v53 staged, v70 local.

## Persistent vertex attribute snapshots (v71)

Added the native 16xfloat4 attribute bank and SetVertexData2f/4f/2s/4ub/Color
conversions, matching upstream defaults and ARGB-to-RGBA conversion. Device
render reset initializes the bank. This implements persistent input values;
immediate Begin/End submission remains unimplemented and must add the register
zero vertex-emission hook before claiming immediate-mode support.

Two 64 KiB uncached GPU-readable snapshot arenas follow logical slots. Each
record is immutable, 256 bytes. Cached CPU comparisons reuse the last record
when unchanged; changed values append a fresh record. The snapshot returns a
CPU reference; vertex binding pins it to the logical scene and releases temporary
CPU ownership. Begin resets an arena only after prior scene retirement and when
its allocation has no remaining CPU/GPU references. At 256 distinct records per
scene, allocation fails explicitly without overwriting live data or adding a
mid-scene GPU wait; production overflow policy remains to be decided.

Device owns creation/destruction as a tracked stage. Vertex binding automatically
snapshots the current bank for constant-fed attributes unless a validated
explicit snapshot was supplied. Compiled standalone coverage checks unchanged
reuse, changed-record immutability, ARGB conversion, reset rejection with live
references, automatic snapshot binding for shader 01, destruction rejection while
GPU-pinned, and successful retirement cleanup. v71 builds/links/packages with
no unresolved executable symbols; none of these new tests ran on physical Vita.
No deployment/FPS gain. Next: fragment catalog identity and compatibility with
these vertex programs, then ordinary draw submission. Immediate input and
texture-cache upload integration remain separate open work.

## Shader provenance correction (v72)

Fragment integration audit found source/shaders contains an older generated
shader set: its fragment table lacks canonical ps_key/cube-to-2D fields expected
by current xv_d3d.c. The actual perf307 build uses hierarchy-fused-307/build-x87/
shaders. Nine of 67 vertex GXPs differ (05,08,09,14,18,20,23,41,54); for example,
05 changed fog output from FOG to TEXCOORD7. Mixing the old vertex catalog with
current fragment binaries would risk varying-link/output regressions.

Build and aggregate-link entry points now REQUIRE --shader-dir, propagated to
both clear/test-helper and vertex generators. Layouts and GXP files come from
that same directory; vertex receipts record resolved source paths and layout
hashes. Confirmed all 67 selected vertex binaries exactly match the perf307c VPK.
Rebuilt v72 from that set; all 67 raw declaration storage contracts still pass,
and full executable links/packages successfully. No runtime execution or FPS
claim. Earlier v66-v71 artifacts used the old source shader set and should not
be chosen for hardware qualification. Native v53 still staged, perf307 installed.

Reproduce:
python3 port/vita/build_platform_test.py --xita-source /home/birchwoodgod/xita-backups/2026-09-18-unified-games/source --shader-dir /home/birchwoodgod/xita-backups/2026-09-18-unified-games/hierarchy-fused-307/build-x87/shaders

Next fragment work must preserve mutable per-render-state combiner updates:
Halo can call SetPixelShaderProgram or update individual combiner states. Its
current fragment lookup depends on vertex function hash, normalized program key
and cube-to-2D variant, with colors supplied separately. Do not hash a stale
original definition pointer or reuse the old source table. Fragment matching,
loading, uniforms and texture binding remain unfinished.

## Pixel shader state capture and identity (v73)

Implemented SetPixelShaderProgram with immediate owned state capture and a draw
snapshot that also observes direct inline/individual render-state writes. Most
of the definition prefix follows render-state numbering, but PSTextureModes is
state 116 rather than word 54; a compile-time offset assertion caught this and
the exception is explicitly mapped. Other field offsets are compile-time checked.
Definition mapping metadata is preserved separately and reset on device state
reset. NULL SetPixelShaderProgram follows upstream's no-op behavior.

Reused Xita's GPL program-key/identity helpers (local copies with attribution;
renamed one loop variable for the native gnu89 compiler's scope rules). Canonical
keys omit colors and inactive stages, while snapshots retain full original
state. A per-color cache expands only changed packed colors into 18 float4
constants. Capture rejects combiner counts above eight without changing output.

Standalone tests cover caller scratch mutation, direct state/color writes,
inactive-stage key stability, active-stage identity changes, the special texture
mode state, invalid-count output preservation and reset. v73 compiles/links/
packages, but tests have NOT run on Vita. This is state capture, not a fragment
shader loader or complete draw. Current generated fragment table selection and
GPU program binding remain next. Installed Halo perf307 unchanged.

## Fragment catalog and program linking (v74)

Generated a private catalog from the explicitly selected perf307 build shader
directory: 620 unique sorted (vertex function hash, normalized combiner key,
cube-to-2D mask) entries, 594 distinct GXP payloads (1,014,720 bytes), embedded
once each. Generator rejects unknown table schemas, missing rows, duplicate or
unsorted keys, unsafe paths and invalid GXP envelopes. Provenance receipt records
source paths, sizes, table and payload hashes. These derived payloads stay under
build/vita and must not be committed as source assets.

Added exact tuple lookup, vertex compatibility checks, GXM fragment registration/
linking with the caller's blend state, sampler discovery and uniform parameter
handles for psc/fog/alpha test/texture scale/border. Patcher lifetime is retained
through partial failures and explicit destruction. Caller must retire GPU uses
before destruction; this is a component loader, not yet a draw cache. Programs
record discard/depth replacement for later state decisions.

Compiled tests check every tuple lookup and representative base fragment links
for vertex programs with matching entries, releasing fragments before vertices.
v74 builds/links/packages; no physical execution or rendered output established.
Package provenance check: 577/594 fragment files match perf307c exactly; 17 are
absent from that VPK (zero differing files among those present). All 594 exist in
the selected build directory. The 17 absent entries remain build-directory-only
provenance, not proof they shipped or ran in installed Halo; receipt is
build/vita/fragment-package-comparison.json.

Next: uniform validation/upload and texture bindings, then a draw cache with
retirement ownership. Fragment selection by actual native state is not yet
wired into generic draw submission. Native v53 staged, v74 local; perf307 remains
installed and no native-port FPS result is claimed.

## Fragment uniform capture and binding (v75)

Added immutable fragment uniform records for 18 combiner colors, fog color,
alpha reference/comparison/enable, dependent-read texture scale and loading-border
parameters. Alpha comparisons map Xbox 512..519 to the shader's 0..7 codes;
invalid enabled comparisons fail without publishing output. Texture scale and
border data must be explicitly supplied when the linked shader needs them.
No guessed texture dimensions or sampler border behavior is introduced.

Binding requires a logical scene and matching combiner key. It validates each
present parameter as float4 uniform data and bounds array lengths against the
CPU record before changing GXM state. One default fragment uniform reservation
serves all writes. Compiler-trimmed prefixes receive their declared extent.
Any reservation/upload failure returns failure so caller cannot issue the draw
using stale constants. Compatible texture and vertex bindings remain caller work.

Tests now compile capture cases for all eight comparisons, disabled/invalid
alpha state, colors, explicit texture parameters and output preservation; each
representative fragment link also attempts scene-scoped uniform binding, wrong-key
rejection and notification retirement before destruction. No draws/physical
execution yet. v75 compiles, links and packages successfully. Next: texture stage
state/resources and generic draw ownership; native v53 remains staged and installed
Halo remains perf307 with no new FPS result.

## Texture-stage state and initial sampler mapping (v76)

Added D3D__TextureState[4][32], default state reset and the deferred/specialized
texture-stage setters used by Xbox inline wrappers. Sampler application accepts
a captured stage array, so direct inline writes are observable. It edits a local
GXM descriptor and publishes only after validation, preserving output on failure.

Current scope is single-mip 2D textures: point/linear min/mag, wrap/mirror/clamp,
and transparent-black full border. Nonzero colored borders, anisotropic filters,
LOD bias/max-mip changes, cube descriptors and mip chains fail explicitly. These
are unfinished capabilities, not claimed equivalent fallbacks. One-level texture
mip filtering is disabled because no other mip is present. Common clamp mapping
matches existing Xita behavior. This helper does not yet connect SetTexture or
native map-cache resources to generic draw submission.

Compiled standalone tests cover deferred/direct state changes, GXM descriptor
min/mag/address values, output preservation on unsupported border/filter, black
border, and reset. First compile caught a missing D3DFASTCALL macro; switched to
the same __fastcall spelling used by existing native render-state setters.
v76 now compiles/links/packages; no physical test or rendered pixel result.
Next: texture allocation ownership/cache integration and sampler binding to the
fragment program. Native v53 staged, v76 local, installed Halo perf307 unchanged.

## Logical-scene texture retirement and fragment sampling (v77)

Native cache source confirms texture registration precedes cache_file_read;
loaded is published later. Eviction waits for loaded and IsBusy, then frees the
cache datum containing the hardware header. A GPU sidecar therefore must remain
associated with that datum until final use retires. Full engine cache hooks are
still pending; no assumption is made that Register implies readable pixel data.

Added a logical-scene texture reference list, deduplicated by sidecar and generation
(up to 512 per slot), distinct from display queue texture references. Pins release
only after that logical scene's fragment notification. Failure poisons the scene
rather than permitting unsafe frees. The sidecar object must remain alive while
owned by a scene; generation checking does not protect a dangling object pointer.

Added fragment sampler binding over prepared upload resources. Pending I/O returns
1 without sampling. It takes temporary ownership while copying descriptor state,
applies the explicit stage sampler settings, records scene ownership, then binds
the linked sampler resource index. Compiler-eliminated samplers are skipped.
Single-mip 2D only; cube sampling/other unsupported modes remain explicit failures.
An optional descriptor snapshot supplies dimensions for later uniform preparation.

Standalone coverage compiles pending-load binding, sampler state, deduplicated
multi-stage references, stale-token rejection, eviction rejection during use, and
notification retirement. v77 compiles/links/packages; no runtime execution, draw
output or FPS result established. Next: native cache sidecar lifecycle/SetTexture
integration, generic draw ownership and submission. Native v53 staged, v77 local;
installed Halo remains perf307.

## 2026-09-28 — Native-port experiment cancelled by user

The user explicitly cancelled the separate native-source Vita port after the long
sequence of standalone iterations without a running game. Stop development,
builds and deployment of native-source-vita-001. Preserve its files as reference;
this is not authorization to delete work. v77 only compiled/linked/packaged; it
has not rendered Halo on hardware or demonstrated a performance gain. Earlier
Next steps in native device-integration notes are superseded.

Resume performance work in the existing source checkout and Xita runtime. The
20 FPS physical-hardware goal remains unmet, with Silent Cartographer (b30) the
user-selected baseline. Last verified installed gameplay build is perf307; do
not claim current device state without reconnecting. Preserve saves and rollback.
Prioritize measured NPC/world/effects preparation and submission costs in the
running implementation. No new native replacement port is authorized by this
continuation. No device package was changed by this cancellation.
