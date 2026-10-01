---
name: halo2-menu-lab
description: "Halo 2 startup work (Sept 2026) - worktree, branch, private stage dir, shared :111 lab scripts, one-trap-per-build history and the referenced-table discovery change"
metadata: 
  node_type: memory
  type: project
  originSessionId: ba4832a4-694c-483c-bed5-d444611783f1
  modified: 2026-09-15T22:35:13.159Z
---

Halo 2 (XDK 5849) startup-to-menu work, started 2026-09-15 from checkpoint 312e6a3
(branch work/halo2-initial-profile-20260912, native220: intro visible, menu never shown).

- My worktree: /home/birchwoodgod/xita-backups/2026-09-15-halo2-menu-claude/source, branch
  work/halo2-menu-claude-20260915, origin github.com/Xita-Project/xita (private).
- My private stages: /home/birchwoodgod/xita-backups/2026-09-15-halo2-menu-claude/private/<stage>/
  (boot/generated, build, codegen.log, build.log). Helpers there: build_stage.py <stage>,
  run_stage.py <stage> <label>. Never commit anything under private/.
- Shared H2 lab (granted): display :111, title XH2B00001, scripts in
  /home/birchwoodgod/xita-backups/2026-09-12-halo2-initial-profile/private (preserve_fresh_cache.py,
  capture_run.py, drive_startup.py; each needs an unused label). venv python there has iced_x86.
  Do NOT touch Halo CE, :112, physical Vita, or ~/github/xboxvita.
- Cycle cost: codegen ~5 min, build ~4-6 min at -j4 (max 4 jobs), run ~3 min.
- Prior agent admitted one vtable slot per build (220 builds) via fingerprinted root families in
  games/halo2_5849/prepare_boot.py. I added `--referenced-tables` to the recompiler (walk pointer
  tables that lifted code installs/loads/pushes/indexes/calls through) and enabled it for the
  host-channel target: 13,590 -> 16,925 functions in one step. Tests: tools/test_referenced_tables.py.
  Committed this + an arena/page-table stop snapshot in boot.c (arena_snapshot -> arena-at-stop.bin,
  read offline with private/arena_tool.py; disasm/xrefs with private/xbe_tool.py).

- native221 (committed, referenced-tables) stop: `28C710 fn=00279860` — the whole motion-object
  construction that blocked native220 (31ADF0) now completes; stops in the 9th animation-codec row.
  That codec table 47FB24 has 40-byte rows, callbacks at fixed offsets, indexed `[format*40+base]`,
  row0 starts null -> the linear code-word walk misses later rows. NEXT clean step: extend
  game_packed_vector_roots (prepare_boot.py) from 2 formats to all 9 rows (structured, per-row).

- Commit 16122f1 (pushed) knocks out BOTH follow-ups:
  (1) game_animation_codec_roots in prepare_boot.py extracts the full 9-row codec table 0x47FB24
  (40-byte rows, 7 callbacks at +0..+0x18, indexed lea [idx*8+base] which referenced-tables misses;
  bounded by the shared +0x18 callback 0x28D170, exactly 9 rows). Clears 28C710 -> referenced-tables
  path now stops at 0x2243A0 (a vtable method, class table not yet named by the walk).
  (2) Device-corruption root cause FOUND + FIXED: the audio adapter used xk_mem_alloc (same virtual
  pool as the game's NtAllocateVirtualMemory/free). Game freed virtual 0x936000, DirectSoundCreate
  (0x37D797) reused it for the long-lived device, game use-after-freed 0x936000 (fn 0x2DD930) and
  zeroed the device vtable. Fix: added xk_mem_alloc_high (top-down reserve just below KERNEL_VA,
  away from the game's low heap) in xk_mem.c/xk.h; routed all 10 audio_host.c allocations through it.
  Device now at 0x3CF6008, no corruption; data-roots path advances past the device to 0x335D38
  (stream-completion callback calls a still-null vtable method [eax+10h] — the NEXT deep blocker).
  Diagnosis tools (kept in private/, NOT committed): a device-page write-watch in boot.c's
  xv_check_guest_address (getenv is empty in the guest, so hardcode the addr; read the word via raw
  g_xram+g_xpt to avoid X_M32 re-entry recursion) and a map_page log in xk_mem.c.

- Next (uncommitted at the time of writing, then committed): stream completion callbacks moved from the
  adapter worker to DirectSoundDoWork (stream_poll marks ready on the sink fence; stream_deliver at
  0x37B844 retires + calls back). Reason: game frees stream-context records (0x824558E4, 0x60 stride,
  via IDENTITY alias 0x0245xxxx — watch the identity page, not 0x8245xxxx!) at the intro->menu
  transition without Flush/Release; Xbox only calls back from DoWork. Result: 2 s -> ~6 min after Start.
  prepare_boot.py host-channel target now = data roots + referenced tables (reproduced byte-identical).
- CURRENT deep stop: `kernel stack unmapped window first=D0000000 fn=002DD930`. First main-loop
  iteration after intro: 0x2CBF0 creates 2 render-list records (table 0x4BA138, 32B, count 0x4C0B7C)
  with tag=-1; 0x44E90 no-tag path -> bogus node count -> 0x2DD930 esp-relative node loop ~2^30 iters
  writing upward through memory (it is the routine behind all three "corruptions"). Chain: main 121C9
  -> 12B690 -> 12B450 -> 22324C -> 223600 -> 2B5D0 -> 2B7E3 -> 2BA10 -> 2B990 -> 3F353 -> 44812
  -> 44E90 -> 2DD930. Next: who calls 2CBF0 with tag -1 (0x133520 region) / why model lookup fails.
  Note: unmapped virtual pages map to a writable trash page, so wild writes only stop at explicit
  guards (kernel-stack window) — a wild loop can run minutes before any fault.

**Why:** the strict no-data-roots mode never discovers .rdata vtables, so each virtual call stopped the run;
and kernel allocations sharing the game's flat address pool collide with the game's own alloc/free;
and callbacks delivered off DoWork race the game's own teardown of its records.
**How to apply:** keep runtime strictness (unknown targets trap); widen only translation coverage with
verified code-pointer tables; allocate kernel-owned game-visible objects from a game-free region.
Menu still does NOT render in any build (last frame still 136, intro). Report menu progress separately.

- SESSION 2 (commits ede46b2..1d91f61, all pushed) drove the data-roots boot from the audio-device
  corruption ALL THE WAY to the first menu triangle draw. Chain of fixes after the audio work:
  (a) split_blocks overlap bug (overlapping blocks emitted a loop's dec;jne twice with inconsistent
  dead-flag liveness -> ~2^32-iteration runaway in 0x2DD930; fix truncates the earlier block; removed
  ~370K duplicate instructions image-wide). test: tools/test_block_split.py.
  (b) sparse switch tables (jmp [reg*4+tbl] stopped at first null hole; now skips nulls).
  (c) instructions: movntq, cvtps2pi/cvttps2pi, pshufw, divps, unpckhps, andnps.
- CURRENT WALL (the big remaining piece): the boot now runs the whole CPU-side vertex/geometry
  pipeline and the host channel processes 291 pushbuffer submits (137 screen clears). It stops when
  the menu issues NV097_SET_BEGIN_END (method 0x17FC, value 5 = TRIANGLES) at eip=0x3FAC58. The
  command consumer (games/halo2_5849/command_state.c) is STATE-ONLY BY DESIGN: it captures combiners,
  textures, vertex-array offsets (0x1720-0x179C), blend/stencil etc. but rejects BEGIN_END and all
  vertex emission ("A future draw backend must..."). Rendering the menu requires building the general
  3D geometry draw backend: on BEGIN_END, decode the captured vertex arrays + combiner state, run the
  vertex program, rasterize triangles with the combiner->fragment pipeline (tools/ps_pipeline.py) and
  textures via GXM. The prior agent built only specialized pipelines (QUAD/SCREEN/BC1/COMPOSITION/
  BLUR/BLEND/LUMA/SPRITE/THRESHOLD) for the intro movie + effects, not general geometry. This is a
  large rendering-engine effort, not a few build cycles.

- SESSION 3 (2026-09-15, commit 24454ea pushed): decoded the WHOLE
  main-menu frame offline. Key trick: at the first BEGIN_END stop, GET=0x03B815C4
  but PUT=0x03B87F54 — the game had already queued the entire frame, so the push
  snapshot (push-at-stop.bin) + channel-at-stop.json let you enumerate every draw
  with NO run. Tool: private/decode_push.py <artifacts-dir> [--verts D,D].
  FRAME = 32 draws: 3 indexed TRIANGLES (#1,#8 are big, 3408 idx=1136 tris each =
  backdrop), 5 indexed TRI_STRIP (full vertex arrays + the 40-instr vertex
  program), 24 TRI_FAN mostly 4-vertex IMMEDIATE quads (SET_VERTEX_DATA4F attr0 =
  SCREEN-PIXEL pos x~55-84 y~136-149 ~11px glyphs/boxes, 4UB attr9 = color, 2S
  attr3 = texcoord). Target = 640x480 A8R8G8B8 back buffer color_offset 0x03A11000.
  RENDER PLAN (user: "2D menu first, then 3D backdrop", "push-buffer->CE backend"):
  (1) 2D immediate quads need only screen->clip ortho + per-vertex color + blend +
  combiner/texture — NO vertex-microcode recompiler; closest template is
  sprite_draw.c + sprite path in quad_gxm.c. (2) combiner->fragment already exists
  in CE (tools/ps_pipeline.py), same NV2A format as s->setup. (3) indexed
  strips/triangles (backdrop) need NV2A vertex microcode s->program -> GXM vertex
  shader + s->constants as 178 uniforms (quad_gxm.c does exactly this), array fetch
  from s->setup 0x1720/0x1760, ARRAY_ELEMENT16 index buffers. New draw module plugs
  into host_channel_runtime.c geometry_method() chain (returns -1/0/1); GXM backend
  = new menu_gxm.c mirroring quad_gxm.c (psp2/gxm.h available in H2 build). Details:
  private/MENU_FRAME_CHARACTERIZATION.md. Intro still verified (frame 136); menu
  still does NOT render — report the two separately.

- COMMIT 24454ea (pushed): added games/halo2_5849/menu_draw.c/.h + menu_draw_test.c,
  the general geometry CAPTURE module (front half of the CE-style backend). Implements
  NV2A immediate-mode vertex assembly (SET_VERTEX_DATA 2F/2S/4UB/4S/4F; attribute-0
  completion submits a vertex; persistent current attrs) + ARRAY_ELEMENT16/32 / DRAW_ARRAYS
  / INLINE_ARRAY capture into one h2_menu_request handed to a render callback. No guest/FB
  mutation; unbacked/rejected/overflow -> return 0 (strict preserved). Wired into
  host_channel_runtime.c geometry_method() as lowest-priority consumer behind H2_MENU_RENDER
  (Makefile flag, default OFF => default build byte-identical, still stops at first draw).
  menu_quad.render left NULL until the GXM backend. Host unit test passes; geometry_dispatch_test
  still passes. NEXT INCREMENT = menu_gxm.c (mirror quad_gxm.c): screen->clip ortho vertex
  shader + combiner->fragment (reuse CE tools/ps_pipeline.py, same NV2A combiner format) +
  texture + present; set menu_quad.render. Reachability note: draw #1 is INDEXED TRIANGLES,
  so a presentable frame needs the indexed+vertex-program path too, not only the 2D immediate
  path -- there is no honest small subset that presents a frame (all 32 draws share one back
  buffer; skipping any = bypass). Integration VALIDATED: private/menu224 stage (reuses datascan222 codegen, built
  MENU_RENDER=1) links + runs; live log `[h2/menu] rejected=1 primitive=5 vertices=0
  indices=1704 arrays=0 overflow=0` = the module claims draw #1 (TRIANGLES), captures
  its 1704 indices, and rejects (render NULL) -> boot stops at the same eip=003FAC58,
  strict preserved. index_count 1704 matches decode_push.py exactly, so the offline
  pushbuffer decode is confirmed against the live runtime. menu224 is a private diagnostic
  stage (NOT committed; MENU_RENDER stays OFF by default). Also: VP transform is standard
  (viewport constants c8-c11 + WVP c0-c3), and the vertex PROGRAM is re-uploaded (0xB00)
  between draws so there may be a few distinct programs; per-draw program/constant/combiner
  extraction is GXM-phase work. NEXT: menu_gxm.c backend.
- INTEGRATION VALIDATED (menu224 stage, MENU_RENDER=1, reuses datascan222 codegen): links +
  runs in the :111 lab. Live log `[h2/menu] rejected=1 primitive=5 vertices=0 indices=1704
  arrays=0 overflow=0` = menu_draw claims draw #1 (TRIANGLES), captures its 1704 indices,
  rejects (render NULL) -> stops at same eip=003FAC58, strict preserved. index_count 1704 ==
  decode_push.py, so the offline pushbuffer decode is confirmed against the live runtime.
  menu224 is a PRIVATE diagnostic stage (not committed; MENU_RENDER OFF by default). Vertex
  PROGRAM is re-uploaded (0xB00) between draws => possibly a few distinct VP variants; per-draw
  program/constant/combiner extraction is GXM-phase work. NEXT: menu_gxm.c backend (generic
  transform VS fed captured constants + combiner->fragment via CE ps_pipeline; render into
  back buffer; set menu_quad.render).

- SESSION 3b (goal: render the menu). Built the SOFTWARE renderer (avoids new GXM shaders;
  reuses nv2a_vsh transform + existing texture decoders; composites into the game back
  buffer which the game's own flip presents). Commits (pushed through 3fb3331; 27002ec local):
  * af6aa74 nv2a_vsh.c/.h: software NV2A vertex-program interpreter (encoding ported from
    recompiler/dx8_shader_parse.py; MAC/ILU dual-issue, add reads A&C, r1/r12 aliases, a0.x
    rel, FINAL). Validated vs real menu program (draw#1 = standard transform + viewport
    epilogue mad oPos=r12*r1.y+c11). Test nv2a_vsh_test.c.
  * e9e9aa6 menu_raster.c/.h: perspective-correct triangle raster, diffuse*tex0, SRC_ALPHA
    blend, scissor, into A8R8G8B8 back buffer. Test menu_raster_test.c.
  * 3fb3331 menu_render.c/.h (h2_menu_software_render, wired as menu_quad.render): maps back
    buffer, resolves vertex arrays (offset bit31=DMA B; fmt type/count/stride; decodes
    float/s16/UB/packed), runs VP per vertex, assembles TRI/STRIP/FAN/QUAD (indexed+immediate),
    rasterizes. command_state.c widened to accept menu vertex formats (types 1/4/5).
  * 27002ec command_state.c: capture 0x1500-0x152C viewport/clip + 0x1880-0x1AEC current
    vertex attributes (were rejected -> stopped the boot); menu_render accepts empty draws.
    Offline probe (private/, uses command_state) confirms 0 remaining state-method rejects
    in the frame.
  RESULT of first end-to-end run (menu224b): the renderer TRANSFORMED + RASTERIZED 6 draws
  (300-vtx TRIANGLES backdrop + 5 strips) into the back buffer with NO crash, far past all
  prior stops. Stopped only on unwhitelisted state 0x1518 (now fixed). Frame had NOT presented
  yet (stopped mid-frame). NEXT: rerun; expect the frame to reach the flip and present the menu.
  Open refinements after first pixels: seed VP current-vertex inputs from stored attributes
  (colors), DXT texture decode, per-draw blend/scissor from render state, depth.
  Debug tool: private/decode_push.py + the reject probe (/tmp pattern). Stage private/menu224
  (MENU_RENDER=1, reuses datascan222 codegen); MENU_RENDER OFF by default.

- SESSION 3c (geometry engine VALIDATED, pushed through bc86c98). The full 118-draw menu
  frame now transforms + rasterizes into the back buffer with NO crash/stop (pushbuffer
  drains GET==PUT result=0). Chain of fixes to get there:
  * command_state: accept interleaved state inside a draw (menu issues BEGIN then a clip
    rect 0x1518-0x1524 before vertices) -> menu_draw returns -1, dispatcher forwards to
    h2_command_method (commit fc99543). Also 0x1500-0x152C viewport + 0x1880-0x1AEC current
    vertex attrs (27002ec). menu vertex formats types 1/4/5 (3fb3331).
  * VERIFIED the interpreter: offline harness on the real 40-instr program + a known input
    reproduces the hand-computed screen pos (v0=0 -> oPos=(474,303), w matches runtime 511289).
  * VERIFIED the array fetch: reads real model pos (53.76,-97.96,3.77), UVs, unit normals.
  RESULT: 2D UI (immediate verts, prog=4 passthrough) renders at CORRECT screen positions with
  CORRECT per-vertex colors (cream/salmon Halo menu text colors). Buffer dumped to
  ux0:data/xita-halo2/menu-frame.bin (menu_render, every 20 draws) -> PNG shows the 2D UI blob
  at the right place. 3D backdrop transforms correctly but renders BLACK (needs register
  combiner + DXT texture decode; my pass uses diffuse*linear-tex0 only) and has no near-plane
  clip (some tris have w<0). PRESENT stalls: after the frame drains, the game spins in a
  timer/IRQL loop waiting for vblank to flip; vblank generation (host_channel_runtime
  signal_game_vblank) stopped at frame 136 -> frame never presents (last-presented still 136).
  REMAINING for visible+recognizable menu: (1) register-combiner->fragment + DXT texture decode
  (biggest), (2) near-plane clipping for 3D, (3) vblank/present fix. Geometry core is DONE.
  Lab hygiene: run_stage leaves the emulator running on 600s timeout (no strict stop when the
  game spins) -> must TaskStop the run + kill Vita3K by comm before the next run. menu-frame.bin
  needs deleting between runs. Offline interpreter harness pattern in /tmp (nv2a_vsh + JSON prog/const).

- SESSION 3c CORRECTION: PRESENT ACTUALLY WORKS. The earlier "vblank stall" was one run's
  state. In steady state the game does recurring flips and h2_platform_present scans the
  menu buffers (03A14000/038E8000/039C0000) out to the :111 screen (confirmed: "[h2/display]
  presented linear ARGB8 address=..."). Two real remaining gaps:
  (1) FRAGMENT SHADING: my menu_render uses diffuse*linear-tex0 only, so the 3D backdrop
  renders exactly black and text renders as solid color blobs. A :111 screenshot during a
  menu run shows black (backdrop black + the tiny 2D-UI blob too small/absent to read).
  Need register-combiner->fragment + DXT texture decode (the existing dxt23_layout.c / DXT
  readers can be reused). This is the biggest remaining piece.
  (2) NON-DETERMINISM: menu224g drew 118 menu draws; menu224h (same build7 vpk) drew ZERO
  (menu module never activated) and just cleared+presented black. The intro->menu draw frame
  is timing-dependent. Investigate why the menu geometry sometimes isn't submitted.
  Also still: near-plane clipping for 3D (some tris have w<0). Geometry engine + present are
  proven; the menu is rendering but not yet shaded/recognizable on screen.

- SESSION 3d BREAKTHROUGH (pushed through 1cdee29): the menu BACKDROP now RENDERS. Added:
  * menu_texture.c (commit bbada5b): DXT1/DXT3/A8R8G8B8 decode + NV2A Morton de-swizzle.
    All menu textures are tiny (4x4/8x8); no font atlas. 2D UI quads sample one 8x8 DXT3.
  * menu_combiner.c (commit 1cdee29): NV2A register combiner (up to 8 stages + final
    A*B+(1-A)*C+D). Bit layout ported from recompiler/dx8_pixelshader_parse.py +
    pixelshader_recomp_gen.py (input=reg|chan<<4|map<<5; ocw=cd|ab<<4|sum<<8|flags<<12;
    dot/mux/scale). menu_raster rewritten to interpolate diffuse/specular + 4 texcoords,
    sample 4 textures, shade each fragment through the combiner. menu_render loads tex0-3 +
    decodes the combiner per draw. Tests: menu_combiner_test, updated menu_raster_test.
  RESULT: backdrop draw (5-stage combiner: dot3 normal-map lighting of t2/t3, final=spare0)
  goes from ~300 lit pixels to ~303,000 (98% of screen). menu-frame.bin shows a RECOGNIZABLE
  landscape: tan sky gradient, horizon, dark rolling terrain (the Halo 2 menu scene). It is
  DARK (dusk mood + coarse 4x8 lighting textures; output x2 scales already applied correctly).
  REMAINING for stable on-SCREEN menu (not a rendering issue):
  (1) The game renders the menu into ONE buffer (03A14000) for ONE frame (~40 draws over 7s)
  then STOPS looping; the display presents the OTHER two buffers (038E8000/039C0000) which are
  empty -> physical :111 screen is black even though the rendered backdrop is in memory. Need
  the game to redraw the menu every frame into every buffer (a game-progression/redraw issue,
  possibly the same post-frame spin). (2) brightness polish. (3) the sparse 2D UI overlay.
  Perf note: the per-pixel combiner makes rendering much slower (~10s+/frame in the emulator),
  so menu-frame.bin appears later; that's expected. HOW TO SEE IT: menu_render dumps
  ux0:data/xita-halo2/menu-frame.bin every 20 draws; convert (B,G,R,A -> RGB) to PNG.

- SESSION 3e ROOT CAUSE of "no menu items": the game is stuck in a PRE-MENU / loading state.
  Every run (fast no-combiner AND combiner) reaches the SAME ceiling: ~118 menu draws/frame,
  never growing. Those ~100 prim=7 TRI_FAN draws are NOT text glyphs -- they all sample the
  SAME 8x8 DXT3 glow texture (addr 018FAB00) at uv (0,0)-(1,1), i.e. repeated small glow/effect
  quads. There is no font atlas and no per-item geometry in the frame. So the settled main menu
  (Campaign/Multiplayer/... text) is NEVER SUBMITTED by the game -> nothing to render. Blocker
  is GAME PROGRESSION (deep startup/state-machine debugging: why doesn't the game advance from
  the loading/backdrop state to the settled menu?), NOT rendering. The rendering engine is
  complete + proven (backdrop renders recognizably). SECONDARY: the per-pixel software combiner
  is ~45s/frame (overdraw x 5 stages x 300K px); before the combiner the loop ran ~1s/frame
  (black backdrop). For a smooth interactive menu the backdrop wants GPU/GXM rendering (the big
  pivot I avoided by going software). NEXT (if pursuing the interactive menu): (1) find why the
  game loops the pre-menu state instead of reaching the settled menu (startup debugging like the
  earlier native-NNN stops); (2) then GXM for the backdrop perf; (3) input for interaction.

- SESSION 3f FINAL ROOT CAUSE (why no menu items): the game is STUCK STREAMING THE MENU
  RESOURCES FROM DISK. Evidence: the game renders ONE loading-screen frame (backdrop + ~100
  glow-effect quads, 120 draws) then stops rendering (menu-render frozen at 45) and just
  re-presents it. Meanwhile game worker thread 16 loops NtSetInformationFile (690x) +
  NtReadFile + NtWaitForSingleObjectEx = file streaming that never completes. Input DOES reach
  the game (Vita3K keymap: START=Enter, Cross/confirm=KeyX, Circle=KeyC; a press showed
  digital=0x0010 in the pad) but does NOT advance it (no new draws) -> it's a load stall, not
  an interactive attract. So the settled main menu (Campaign/Multiplayer text/geometry) is
  never submitted because its data never finishes loading. BLOCKER = file-I/O/resource-streaming
  (xk_file.c / NtReadFile / NtSetInformationFile), a DIFFERENT subsystem from graphics.
  RENDERING IS DONE + PROVEN (backdrop renders recognizably; engine: nv2a_vsh + menu_texture DXT
  + menu_combiner + menu_raster + menu_render, all committed/tested). NEXT SESSION LEAD: trace
  thread 16's file streaming for the menu map (which file, offset progressing or stuck, what
  NtSetInformationFile class it sets, what event it waits on) -- this is the next native-NNN-style
  startup milestone. Toggle MENU_NOCOMBINER=1 gives a fast (~1s/frame) build for iterating on it.

- SESSION 3g PRECISE ROOT CAUSE (ruled out rendering/input/file-load with evidence):
  The DISPLAY/VBLANK LOOP HALTS at the intro->menu transition. Timeline (fast build menu224n):
  intro delivers vblanks game_count 0..136 + 5 recurring flips until ~20:36:32, then STOPS; the
  menu loads (mainmenu.map/shared.map close ~20:36:29, only 529 total NtReadFile - loading is NOT
  the bottleneck) and draws ONE frame (118 draws) at 20:36:34-36; the channel drains (PUT=GET
  result=0); then the game issues NO software_flip / NO AvSetDisplayMode / NO further vblank, and
  its threads block on NtWaitForSingleObjectEx while thread 12 spins the DPC/timer loop. So the
  game renders the menu frame, then waits for the next frame tick that never comes. The recurring
  vblank is delivered only by complete_active_flip (needs a queued flip via software_flip +
  active_display_inputs() true); at the menu the game never even requests the flip -> deadlock:
  main thread waits on a vblank event that is only signaled by completing a flip. active_display_inputs()
  is a huge gate (MINIPORT regs, channel.clear format 0x128 / clip 640x480 / pitch 2560 - all match,
  flip_modulo==2 - matches). NEXT LEAD: instrument WHICH condition/event the menu path needs -
  why the game's main thread waits instead of requesting the menu's first flip; likely the
  vblank-event delivery the intro used (mode_vblanks state machine / __wrap_xk_KeWaitForSingleObject
  for MINIPORT+0x194, or signal_game_vblank) doesn't cover the menu's frame loop. This is the
  next native-NNN display-timing milestone. Xbox digital buttons: START=0x10, A/Cross=... (game
  saw START=0x10 from Enter, but it doesn't advance because it's a frame-tick deadlock, not attract).
  RENDERING REMAINS DONE + PROVEN.

- SESSION 3g DEEPEST FINDING: at the menu the game finishes rendering, the channel DRAINS
  (PUT=GET result=0), and it issues NO software_flip and NO flip-stall method 0x130 -- it parks
  on a kernel event BEFORE the present sequence. The flip/vblank machinery is driven by the game
  hitting method 0x130 (NV097_FLIP_STALL) which, on reject, calls complete_active_flip (if a flip
  is queued via software_flip 0x100) or complete_initialization_vblank; the game never reaches
  0x130 at the menu. So the game waits for a GPU/frame-completion EVENT (post-render, pre-present)
  that the HLE delivered for the intro but not for the menu's frame loop. CONCRETE NEXT STEP:
  instrument xk NtWaitForSingleObjectEx to log the object handle the game's main render thread
  blocks on right after the menu frame drains, then find who should signal it (a GPU interrupt/DPC
  tied to the end-of-frame semaphore release 0x1D70, or the vblank event) and deliver it. This is
  the next native-NNN display/GPU-interrupt milestone -- deep, multi-step, NOT a quick fix.
  ASSESSMENT: rendering engine is complete + proven (all committed/pushed through 452b0ba). The
  interactive menu (Campaign/Multiplayer) does NOT render because the game is deadlocked one
  subsystem upstream of graphics (GPU frame-completion event delivery for the menu loop).

- SESSION 3g FINAL: the menu frame carries NO flip methods (0x120-0x130) - it ends with a
  semaphore release 0x1D70 (0x235 -> phys 0x03C41000, written SYNCHRONOUSLY by release_semaphore,
  so it's satisfied) + two 0x1D90 (=clear_color, kelvin_clear.c). The game runs its OWN frame
  tick: timer 0x484248 -> DPC routine 0x332F4C (fires periodically), independent of host vblank.
  So thread 12 is NOT blocked - it actively loops the frame tick. The blocker is that the game's
  MENU STATE MACHINE does not advance past the loading frame on those ticks (a game-logic
  condition in the recompiled title, gating the transition loading-frame -> built menu). This is
  the next major reverse-engineering milestone (trace DPC 0x332F4C / the menu tick and the flag it
  waits on), comparable in scope to the whole prior startup effort; NOT a graphics/input/file/vblank
  fix. RENDERING ENGINE COMPLETE + PROVEN + COMMITTED (nv2a_vsh, menu_texture DXT, menu_combiner,
  menu_raster, menu_render; backdrop renders recognizably). Fast iteration: MENU_NOCOMBINER=1.

- SESSION 3h (fresh runtime re-verification; CORRECTS several stale claims above). Re-ran the
  analysis against the LIVE 600s boot.log (577,084 lines) instead of the 8s capture snapshots.
  VERIFIED FACTS:
  * The game DOES reach + submit the menu backdrop in the full run: 118 h2/menu captures, 40
    menu-render draws into back buffer 03A14000, menu-frame.bin drawn=100 (recognizable landscape),
    at ~line 519467 (well after the intro). The 8s view snapshots (224g/224n) just cut off before
    menu submission -> my note that "224g drew 118 / 224n drew 0" was an artifact of truncated
    capture windows, NOT non-determinism. Both snapshots are identical (intro present + heartbeat).
  * After the backdrop frame drains (channel PUT=GET result=0, method 1D90, clears=137, semaphore
    releases=282 last=03C41000 value=0237) the game issues NO flip and submits NO further geometry.
    The remaining ~57K lines are: thread 12 busy-spinning the timer heartbeat (~40K lines: KeSetTimer
    0x484248 dpc 0x332F4C + KeSetTimer 0x462E50 dpc 0x333F93 + raise/lower IRQL), thread 16 occasional
    file I/O (NtReadFile/NtSetInformationFile), thread 8 occasional mutex 0x40/0x44 + semaphore.
  RULED OUT THIS SESSION (each was a WRONG prior hypothesis, disproven by reading the actual code):
  * "Timer-scheduled DPCs are dropped" -- FALSE for this build. games/halo2_5849/kernel_timer.c is a
    full DPC/timer override: __wrap_xk_KeSetTimer/Ex -> set_timer() converts absolute FILETIME due to
    uptime correctly (delta = due>wall ? due-wall : 0), h2_timer_poll dispatches due DPCs (routine
    0x332F4C fires repeatedly, CONFIRMED in log), worker_entry yields between service turns. The
    generic xk_thread.c KeSetTimer (which does collapse absolute due to now100) is OVERRIDDEN by the
    wrap and not used. So NO timer/clock/DPC bug. (The "KeSetTimerEx ... DPCs are not dispatched" log
    string in xk_thread.c is dead for H2.)
  * "Thread 8 starves the menu-builder" -- FALSE. In the tail thread 12 dominates (~40K lines), not
    thread 8 (60 lines); the timer worker yields; threads 16/8 do run. Not a starvation deadlock.
  * 0x332F4C is just the ~1ms system heartbeat (mov ecx,[48428C]; call 332B6D(,0x14); call 332EE8),
    re-arms itself; NOT the menu state machine.
  * Input path mechanics are sound: xv_hle_XInputGetState feeds pad state to the game for handle
    0x00777701 (port1). BUT boot.log does NOT capture XAPI XK_LOG output (XMountUtilityDrive, XInputOpen,
    [pad] etc. all show 0 despite the game clearly booting) -- so boot.log CANNOT tell us whether the
    game polls input at the backdrop. That question is unresolved and needs XK_LOG routed to xv_logf.
  TRUE BLOCKER (unchanged in substance, now firmly evidenced): the game reaches the menu-BACKDROP
  state and does not progress to building/submitting the interactive menu WIDGETS (Campaign/Multiplayer;
  no font atlas ever loaded). Locus = the game's own menu state machine (main thread + worker threads),
  a genuine multi-session RE effort. Renderer + HLE (timer/DPC/input/file/flip) are proven/correct.
  BEST NEXT EXPERIMENT (bounded, 1 build cycle) to end the multi-session ambiguity: instrument (a) each
  port-1 XInputGetState poll with button value + guest return address, routed via xv_logf into boot.log,
  and (b) the menu/game state word the backdrop loop reads, to definitively answer input-gated-attract
  vs state-machine-stuck. HIGHEST-LEVERAGE ACCELERATOR: an original-Xbox Halo 2 XBE RE symbol map/IDB
  (community RE, NEVER leaked source/bytes) to name 0x332F4C/0x484248/menu-manager routines.

- SESSION 3h cont. DEFINITIVE localization of the menu blocker (fresh evidence):
  * The game NEVER polls the controller at the backdrop. xk_os_pad_poll is called ONLY from
    xv_hle_XInputGetState (xk_xapi.c:221); its pad-raw logger fires on the first poll; the 600s
    live boot.log has 0 "[xk] pad raw" lines -> XInputGetState is never called. So the game is stuck
    BEFORE input goes live (the interactive menu is where input starts being read). => the blocker is
    NOT input delivery; it is the state machine not reaching the interactive-menu state.
  * Read the mid-menu-frame arena snapshot (menu224c, channel stop method 0x1518): the pad code's own
    menu globals read as non-built: menu-root X_M32(0x2E4000)=0x00000086 (not a valid object pointer),
    game_globals X_M32(0x2F8CA0)=0x4489145D (unmapped). CAVEAT: those global addresses carry a
    "Halo 3925" comment while this target is XDK 5849, so 0x2E4000/0x2F8CA0 may be the WRONG globals
    for 5849 -- verify the 5849 UI/game_globals addresses before trusting these values.
  FINAL DISPOSITION for this line of work: the interactive menu (Campaign/Multiplayer) cannot be
  rendered without reversing the game's menu state machine to find the gate between "backdrop shown"
  and "menu screen instantiated + widgets submitted + input polled". Shallow causes (renderer, timer/
  DPC, flip/vblank, file load, input delivery) are all ELIMINATED with evidence. This is a genuine
  multi-session RE milestone. Two ways forward: (1) obtain an original-Xbox Halo 2 (5849) RE symbol
  map/IDB (community RE, NEVER leaked source) to name the menu-manager routines and the 5849 UI
  globals; (2) instrument, per build cycle: log XInputGetState calls + the real 5849 menu-root word
  periodically via xv_logf, and trace the main-thread code path from backdrop-submit forward to find
  the branch that never takes the "build menu" edge. Renderer + HLE remain proven/committed (452b0ba).

- SESSION 3h final corrections + scope:
  * FONTS ARE LOADED (corrects "no font atlas"): boot opens/reads the full menu font set early
    (font_table.txt + handel_gothic-11/13/24, conduit-12/13/16, fixedsys-9, mslcd-14 from z:\fonts\).
    They are bitmap font FILES, not NV2A textures, which is why the texture-path watch didn't see them.
    Text rendering is READY; the menu screen just isn't instantiated to submit glyphs.
  * The backdrop is rendered into 0x03A14000 with NO flip requested -> almost certainly an OFFSCREEN
    render target (the menu composites backdrop+widgets to the display buffer and flips only once the
    screen is built). Another symptom of "menu screen not built", not a present bug.
  * Both timer DPCs are kernel CLOCK plumbing, not the game loop: 0x332F4C -> 332B6D/332EE8 (1ms),
    0x333F93 -> 333B72 (19ms). The game's menu update runs on a main thread (candidates among guest
    threads 4/8/12/16/24/53), NOT these DPCs. Finding that main-thread UI/screen manager is the RE task.
  SCOPE: rendering the interactive menu = reverse-engineering the 5849 menu-screen instantiation path.
  All shallow causes eliminated with evidence. Best accelerator = 5849 RE symbol map/IDB. Do NOT claim
  the menu is near; it is a multi-session milestone behind a known door (menu-screen build gate).

- SESSION 3h PRE-STAGE (no build; ready to combine with symbols): thread 8 is the RENDER/main thread
  (runs 0x003xxxx incl. the 0x3FAC58 menu-submit site; releases mutexes 0x140/0x148). thread 24 is a
  worker (439K NtWaitForMultipleObjectsEx on 0x140/0x144/0x148, waits on thread 8). So the menu-build
  gate is in thread 8's render loop around 0x3FAC58 -- that is the disassembly target once 5849 symbols
  arrive (or for the blind trace). User chose the SYMBOLS path; XBE confirmed halo2ship 1.00.5849,
  sha256 03215919...935d. symbol-shopping-list.md sent to user.

- SESSION 3h CYCLE 1 (trace225): the recompiler function-entry instrumentation (XV_FN macro ->
  xv_watch_enter/xv_trace_func) is present in ALL codegen, so XV_WATCH_FN/XV_FUNC_HIST *should* work.
  BUT the H2 native build (entry games/halo2_5849/boot.c:main; runtime/main.c NOT in Makefile OBJECTS)
  never reads ux0:data/xita/env.txt -> all getenv("XV_*") knobs default, undelivered. Watch ran silent.
  NEXT (cycle 1a): add a tiny env.txt reader to boot.c main() (setenv each KEY=VALUE) to unblock the
  whole XV_* toolchain, then rerun XV_WATCH_FN=3FAC30 to get f_003FAC30's callers = render dispatch.
  f_003FAC30 CONFIRMED as the menu's SET_BEGIN_END submit fn (0x3FAC58). trace225 label used; lab clean.

- SESSION 3h CYCLE 1 SUCCESS (trace227): the H2 diagnostic toolchain is now WIRED (two real gaps
  fixed in boot.c): (1) env delivery -- runtime/main.c isn't linked in the H2 build, so added a small
  env.txt reader to boot.c main() (h2_load_env: read ux0:data/xita/env.txt, setenv KEY=VALUE, log
  [h2/cfg]); (2) the WATCH was a no-op stub -- boot.c defined xv_watch_enter(){} and xv_watch_n(=0);
  replaced with a real impl (parse XV_WATCH_FN hex list, log [watch] enter fn/from/args/regs + [watch]
  leave). xv_watch_n init -1 so the lazy parse fires. (Do NOT add recomp/xv_funchist.c to the Makefile
  -- boot.c already owns xv_cur_fn/xv_trace_funcs/xv_trace_func/xv_watch_*; it collides.) Rebuild = boot.o
  only (fast); use -j1 (the -j4 codegen compile OOMs under memory pressure).
  FIRST CALL-CHAIN RUNG (render submit path, walking UP from the GPU leaf):
    f_003FA680 (call site 0x3FA73B, 278/292)  ->  f_003FAC30 (GPU BeginPush/submit, writes pushbuffer)
    D3D context global = 0x404FE0 (edx); mode flag ds:[0x404FD0]; device ds:[0x407488].
  f_003FAC30 last call at the END of the menu frame, then the render thread STOPS calling it (renders
  backdrop once, idles) -> waits for next-frame commands from the main logic thread that never come.
  NEXT (cycle 2): XV_WATCH_FN=3FA680 to get f_003FA680's callers = the per-frame render dispatch; then
  walk up to the branch that skips building the menu screen. Menu still NOT rendered; toolchain now
  makes the RE tractable.

- SESSION 3h cycle 2 setup: f_003FA680 disassembled = FRAME-END/FLUSH fn. Reads D3D device ds:[0x407488]
  (push ptr [esi], limit [esi+4]), and writes the end-of-frame pushbuffer method headers 0x41D70
  (count1 method 0x1D70 semaphore release) + 0x41D90 (method 0x1D90 clear_color) -- exactly the methods
  that close the menu frame. So f_003FA680 flushes/ends a frame; f_003FAC30 is its low-level writer.
  Chain so far: [frame dispatch] -> f_003FA680 (flush) -> f_003FAC30 (submit). Cycle 2 watches 3FA680
  to get the frame-dispatch caller, then walk up to the UI-render call / the branch that skips building
  the menu screen (the render list is populated by the game-logic/UI thread, a separate path).

- SESSION 3h cycle 2 (trace228): f_003FA680's callers = D3D flush fns f_003F9CC0 (call site 0x3F9CDF)
  + f_003F9D00 (0x3F9D83), 136 each = one per intro frame -> per-frame D3D push-buffer flush. Both
  operate on device ds:[0x407488]. So the render-SUBMIT chain is now:
    [game frame loop] -> f_003F9CC0/f_003F9D00 (D3D flush) -> f_003FA680 (frame-end methods 1D70/1D90)
      -> f_003FAC30 (pushbuffer write)
  Still inside the D3D layer; the game frame loop is 1 rung up (cycle 3: watch 3F9CC0,3F9D00).
  KEY STRATEGIC NOTE: walking UP the render-submit chain reaches the render loop, which WORKS but has
  an empty menu render list. The actual menu-build gate is in the game's UI/LOGIC path that POPULATES
  the render list with menu widgets (a DIFFERENT path from render-submit). More efficient future anchor:
  find the UI/text-draw path (the loaded handel_gothic/conduit fonts must be consumed by a glyph draw
  fn that is NEVER called at the menu) OR the frame loop's render-UI call, and trace why the screen
  stack is empty. Toolchain (XV_WATCH_FN) now makes any of these tractable. Menu still NOT rendered.

- SESSION 3h cycle 3 (trace229): traced the render-submit chain UP INTO THE GAME. f_003F9D00 (D3D
  present) is called 135x (=intro frames) from 0x14101, which is inside GAME function f_00013F10
  (code_000.c, entry 0x13F10). f_00013F10 = "present frame + advance frame": call f_003F9D00 (present)
  @0x140FC; check display-mode globals ds:[0x485ACC]vs[0x467008] + ds:[0x485AD0]vs[0x46700C], reconfigure
  device (0x407488) on mismatch; increment 64-bit FRAME COUNTER ds:[0x485AA0] @0x14196; ret 8.
  CRITICAL: f_00013F10 (and the present) is called 135x during the intro then STOPS -- the menu geometry
  submits AFTER the last present (no present/flip for the menu frame). So the game's MAIN LOOP presents
  every intro frame then stops presenting at the intro->menu boundary. The caller of f_00013F10 = the
  main loop (cycle 4: watch 13F10). GATE = why the intro loop exits and the menu path never resumes the
  present loop. Frame counter 0x485AA0, display-mode 0x485ACC/0x467008 are readable state anchors.
  Chain: main_loop -> f_00013F10(present+framecount) -> f_003F9D00/f_003F9CC0 -> f_003FA680 -> f_003FAC30.

- SESSION 3h cycle 4 (trace230) = GATE FOUND. f_00013F10 (present) callers: 133x from 0x163A2D (intro
  loop) + final 1x from 0x22329B (MAIN GAME). 0x22329B is in a main-loop fn (called via 0x12B65E) that
  GATES the present:
     eax=[0x4E6470]; if(eax>0) skip;  eax=[0x4E64A0]; if(eax>0) skip;  else bl=1
     call 0x12B2E0 (game tick, always);  if(bl) call f_00013F10 (PRESENT)   ; @0x22326F..0x2329B
  So the main loop TICKS every frame but PRESENTS only when BOTH 0x4E6470<=0 AND 0x4E64A0<=0. Intro
  presented (both 0); post-intro one goes >0 and stays -> backdrop renders once, never presents again.
  These are PRESENT-SUPPRESS COUNTDOWN counters: 0x2234E0 decrements both by 1/call (if>0); many sites
  increment them (0x4E6470: 0x12CEC7,0x16F236,0x18ECAC,0x2234E2/F5; 0x4E64A0: 0x12DAFB,0x12DD0E,0x12DF20,
  0x12E4A1,0x12E5B9,0x16F22B,0x223194). Predicate 0x12BED0 also OR's bytes 0x547F71,0x547F76.
  At the mid-menu-frame arena snapshot (menu224c) both = 0; need the value at the POST-MENU STALL.
  ROOT-CAUSE HYPOTHESIS: a load/scene-transition bumps a suppress counter (disable present while
  loading the menu) and the completion that should let 0x2234E0 tick it to 0 never fires in our HLE
  (ties to the earlier resource-streaming/thread-16 observations). If so this is an HLE fix, NOT deep
  game logic. NEXT (cycle 5): read 0x4E6470/0x4E64A0 during the stall + watch decrementer 0x2234E0 and
  the incrementers -> which counter is stuck, at what value, is 0x2234E0 called. Needs a small boot.c
  logger (dump these globals periodically / on a watched fn) + rebuild. Chain fully mapped:
  main_loop(0x12B..)->present-gate(0x22326F)->f_00013F10->f_003F9D00->f_003FA680->f_003FAC30.

- SESSION 3h cycle 5 (trace231) REFUTES the suppress-counter hypothesis and pins the real block.
  Watched present-gate f_00223240 + decrementer 0x2234E0 with XV_WATCH_MEM dumping 0x4E6470/0x4E64A0:
  * f_00223240 called only 2x total (NOT per-frame), both times BOTH counters = 0. So present is NOT
    suppressed; the main-game present fn just stops being called. The main game renders ~2 frames then
    the loop stops iterating. (from-addr 0x12B65E; render-one-frame fn at 0x12B659 presents once + ret.)
  * REAL BLOCK: right after the last present (line ~539917), thread 8 (render/main) enters a tight
    busy-wait FOREVER: NtWaitForSingleObjectEx(h=0x40)/NtReleaseMutant(0x40) ; ...(0x44)/...(0x44) ;
    NtReleaseSemaphore  -- i.e. enterCS(0x40),leaveCS,enterCS(0x44),leaveCS,releaseSem, repeat. It
    polls two critical sections that never have work + signals a semaphore = render thread STARVED of
    frames by a stalled PRODUCER (the sim/logic thread that should feed it). Handles 0x40/0x44 (CS),
    plus a semaphore. Other threads: 12 timer-spin, 16 file I/O, 24 waits on 0x140/0x144/0x148.
  NEXT: identify the GAME code of thread 8's spin loop (the thunk callers of 0x2D6307/0x2D62D0/0x2D6258)
  and what should signal CS 0x40/0x44 / the semaphore -> the producer thread and why it's stalled.
  Likely the menu resource/scene load-completion that should hand thread 8 the next frame. Committed
  XV_WATCH_MEM toolchain addition. Menu still NOT rendered; block now = a specific multi-thread frame-feed stall.

- SESSION 3h cycle 6 (trace232) = block fully characterized at the game level. Thread 8's spin is a
  BLOCKING JOB-QUEUE CONSUMER at f_001208B0:
    EnterCriticalSection(0x4E0358)[handle 0x40]; node=[0x4E28E8](queue head);
    if(node){ dequeue (node->next @+0x3C -> head); LeaveCS; return node; }
    LeaveCS; call 0x2D2084; goto retry;
  0x2D2084 = NtYieldExecution wrapper (import [0x4115BC], checks STATUS_NO_YIELD_PERFORMED 0x40000024).
  So thread 8 YIELD-SPINS on an EMPTY job queue [0x4E28E8] -- it yields correctly each iteration, so
  this is NOT a cooperative-scheduler starvation bug; the producer DOES get CPU. Wrapper chain:
  game(0x120xxx) -> f_002D63FA (EnterCriticalSection) -> f_002D62E4 (WaitForSingleObjectEx) -> HLE;
  release via f_002D62C4 (LeaveCriticalSection). Queue writers (enqueue/producer): 0x120ACC, 0x120CA4,
  0x120CB3 (CS 0x4E0358 also at 0x120F4,0x120AEA,0x120C96,0x120CAC). Consumer dequeue: 0x1208C1/0x1208D0.
  ROOT NOW = producer side: the main game logic is NOT enqueuing render/menu jobs into [0x4E28E8] after
  the intro (during intro jobs flowed -> thread 8 rendered 134 frames). NEXT: find who calls the enqueue
  (0x120CA0 region) and which thread/state gates it -> why no menu jobs. This is engine job-system RE,
  a further phase. Menu still NOT rendered. Full chain (GPU->game) mapped; block = empty job queue.

- SESSION 3h cycle 6 cont.: the queue [0x4E28E8] is the game's COOPERATIVE TASK SYSTEM. Worker loop
  (0x120C42..0x120CBC): pull item esi=[0x4E28EC]; call esi->step [esi+0x34]; if step returns !=0 ->
  RE-ENQUEUE esi at 0x4E28E8 (keep running); if ==0 -> done/removed. Items are objects w/ vtable+step.
  Thread 8's dequeue (0x1208B0) feeds this. QUEUE IS EMPTY post-intro => NO tasks => the intro task
  completed and the MENU TASK was never created+enqueued. The initial-add (vs re-enqueue) writers:
  0x120ACC / 0x120CA4. So the gate is: intro->menu task-creation transition never fires (game state).
  Full chain GPU->game mapped; block = empty cooperative-task queue, menu task uncreated.
  NEXT PHASE (deep, game task/state system): find who creates+enqueues the menu task and what state
  gates it (likely tied to intro-completion / menu-load-completion). THREAD ATTRIBUTION still fuzzy:
  thread 8 runs the task worker; thread 24 does 442K NtWaitForMultipleObjectsEx on 0x140/0x144/0x148
  (possibly the real main/producer thread, blocked) -- worth mapping thread->role definitively next.
  ~3h session; menu NOT rendered; huge chain mapped + 2 toolchain commits (049625e, XV_WATCH_MEM).

- SESSION 3h STEP-BACK REFRAME (user asked to rethink; high-leverage result): the menu stall is a
  MULTI-THREAD SYNC DEADLOCK, not game-logic. In the ~286K-line post-menu stall: threads 8,16,24 are
  all BLOCKED (barely any calls); only thread 12 spins the timer heartbeat (200K calls: KeSetTimer +
  raise/lower IRQL). Thread 16 (LOADER) streams a font (handle 0xF4 = z:\fonts\handel_gothic-11) via
  seek(NtSetInformationFile cls 0x0E)+tiny NtReadFile (20-32B each); its LAST read is line 554031, then
  it enters CS 0x44, CS 0x40, WAITS on 0x48 and never returns. Only thread 16 waits on 0x48 (887x);
  thread 8 does the NtReleaseSemaphore (342x)+1 NtSetEvent. So there is a thread8->thread16 handshake on
  semaphore 0x48 that DEADLOCKED at 554031. Total reads only 489 (finite, not an infinite loop) -> the
  load froze partway. HYPOTHESIS: lost wakeup / missing signal in our HLE semaphore/event or wait
  ordering (fixable in OUR code) OR the producer that drives thread8 parked. NEXT: instrument
  NtReleaseSemaphore/NtSetEvent to log the HANDLE (the tracer omits it) -> is 0x48 signaled-but-lost
  (HLE bug) vs never-signaled. Also which object 0x48 is (sema vs event) + its count. This replaces the
  deep game-task RE as the primary lead. Menu still NOT rendered.

- SESSION 3h REFRAME CONCLUSION (root-cause hypothesis, strongest lead of the session):
  DISPLAY-PACING DEADLOCK. The game's per-frame vblank callback f_0012B2A0 (increments frame counter
  0x485AB0) is delivered ONLY by complete_active_flip, which requires the game to request a flip
  (NV097 method 0x130). At the menu the game never requests a flip -> vblank callback STOPS at
  game_count 136 (boot.log line 548358). The frame-driven game logic that would build the menu, produce
  jobs, and pace the loader never advances -> job worker (thread 8), loader (thread 16, blocked on 0x48),
  and main thread (24, blocked on 0x140/0x144/0x148) all stall. Circular: vblank needs flip, flip needs
  menu built, menu build needs vblank. Intro works because it flips every frame.
  Correlation confirmed: last vblank line 548358 (count 136); stall/last-read at 553726-554031.
  FIX HYPOTHESIS (in host_channel_runtime.c, our HLE): deliver FREE-RUNNING vblank callbacks at the menu
  (drive signal_game_vblank/f_0012B2A0 on a timer when channel is idle post-intro AND no active flip is
  queued), so the frame tick advances, the menu builds, jobs flow, loader completes, and normal flips
  resume. Existing machinery: signal_game_vblank (line 276), mode_vblanks state, complete_timed_mode_vblank,
  the g_boost kick (comment says a similar issue "deadlocked the map-list loader"). DELICATE: must not
  break the working intro flip path; gate the free-run to the post-intro no-flip idle state only.
  TEST: implement, run, watch game_count advance past 136 + menu geometry grow beyond 118 + a flip.
  Menu still NOT rendered but the blocker is now a specific, testable HLE fix, not open-ended game RE.

- SESSION 3h DEFINITIVE THREAD MAP (from [xk] thread-created logs) + reframe outcome:
  thread 4  start 0x2D0AEE (kernel bootstrap) - EXITED early.
  thread 8  start 0x2D0A7A (kernel->game MAIN thread; it CREATED threads 16 & 24). Presents the backdrop
            then parks in the queue-consumer spin f_0x1208B0 (EnterCS 0x40, empty queue [0x4E28E8],
            NtYieldExecution, loop).
  thread 12 start 0 = timer heartbeat (spins KeSetTimer 0x484248 + IRQL, ~200K calls in the stall).
  thread 16 start 0x00120C30 = the TASK/JOB WORKER loop; blocked on semaphore 0x48 (waits for work).
  thread 20 start 0 = aux.  thread 24 start 0x3E3E80 = one-shot thread-pool worker; EXITED normally (0)
            at line 545424 (before menu render 553726) - NOT the block.
  DEADLOCK SHAPE: main(8) and worker(16) both parked waiting for task-queue work; no producer enqueues
  the menu task after the intro. RULED OUT (all verified false): suppress counters (=0), scheduler
  starvation (yields OK), free-running vblank (0x12B2A0->0x14280 is JUST a frame counter, drives
  nothing), main-thread-blocked (thread 24 exited normally), lost-wakeup (HLE sema path correct:
  count++ then xk_signal_check). REMAINING UNKNOWN (unsolved this session): what should PRODUCE/enqueue
  the menu task, and why the main thread (8) entered the consumer wait instead of the producer path.
  HONEST STATUS after ~4.5h: deadlock exhaustively mapped, ~6 root-cause hypotheses proposed & each
  retracted on verification. NOT root-caused; NO confident fix. Menu NOT rendered. All committed
  (049625e + XV_WATCH_MEM). Next-session lead: instrument the main thread (8) transition into f_0x1208B0
  (what condition/state makes it wait for a task) + who calls the task-ADD (0x120ACC) vs re-enqueue,
  and whether an async I/O-completion or APC on thread 16's reads is undelivered by our HLE.

- SESSION 3h *** FIX WORKS *** (fix233, XV_MENU_VBLANK=1): implemented xd3d_vblank_kick for H2 in
  host_channel_runtime.c (the weak hook was CE-only/unlinked). Delivers a real-time-paced free-running
  vblank when the frame counter 0x485AB0 has stalled with the channel idle post-intro and no flip queued
  (intro untouched - it advances the counter via flips). RESULT: game_count advanced 136 -> 366 (was
  frozen at 136 ALL SESSION), 148 vblanks, menu captures grew 118 -> 141 (menu building further!), then
  the game ADVANCED to a NEW blocker: [h2/blocked] sound entry=0037B822 "unsupported original DSOUND
  method" (a DirectSound call during menu build, strict-mode stop). So the frame-pacing deadlock that
  blocked the menu all session is BROKEN. Next: handle the DSOUND method 0x37B822 (menu audio init).
  The fix is env-gated (XV_MENU_VBLANK) for now; make default-on after validating it doesn't regress
  the intro. This is the real root cause + fix, verified.

- SESSION 3h NEW BLOCKER after the vblank fix: unsupported DSOUND method at 0x37B822 (jmp thunk ->
  0x37AD9C). 0x37AD9C: EnterCS via 0x379E9E, checks [0x386B0C], if 0 calls the work 0x380074, LeaveCS
  0x386B18, ret 8. It's a DSOUND buffer/stream method the MENU calls during audio init (right after the
  audio system was actively delivering stream packets via DirectSoundDoWork, tickets 20-24). Dispatcher
  = h2_audio_guest_entry (audio_host.c ~line 1600); handled entries: 0x37B844, 0x379E9E, 0x379F2A,
  0x379F5B, 0x37E126; 0x37B822 falls to default -> fail (strict). To progress: audit + add a handler for
  0x37B822/0x37AD9C (understand 0x380074) in the strict audio HLE - a separate audio-subsystem effort.
  fix233 committed the vblank fix (host_channel_runtime.c). menu still shows only the backdrop+empty
  quads (game stopped at the audio call BEFORE building the menu items), but the all-session frame-pacing
  deadlock IS BROKEN and the game now builds further (game_count 136->366, draws 118->141).

- SESSION 3h AUDIO BLOCKER scope: every DSOUND-section fn traps to h2_audio_guest_entry (hooks.py:369
  emits it for section=="DSOUND"); each must be audited or it fails (strict, opt-in device adapter).
  Handled: 0x37B844,0x379F2A,0x379F5B,0x379E9E,0x37E126 (+ callbacks 0x335D82/38/6D, host boundaries).
  The menu calls 0x37B822 (->0x37AD9C, a DSoundBuffer state method: locks 0x386B18, checks 0x386B0C,
  calls work 0x380074 which sets buffer flags by arg 0/1/2, unlocks, ret 8) from menu code ~0x21F8E3.
  Auditing it CASCADES: 0x37AD9C calls 0x379E9E from caller 0x37ADA2, which the current 0x379E9E handler
  (expects caller 0x379F61 or 0x37B84A) rejects -> must audit the whole menu-audio DSOUND path incl.
  0x380074/0x37FCEE/0x3811FA. This is a fresh deep audio-HLE effort. It is on the CRITICAL PATH: the
  stop is thread 8 (main) inline in the menu build, BEFORE the item geometry, so it must be passed to
  reach the menu items. MILESTONE: the all-session frame-pacing deadlock is FIXED+committed (fix233);
  the next wall is the menu-audio DSOUND audit; after that, whatever builds the item geometry.

- SESSION 3h AUDIO BLOCKER is a DEEP TREE (not a quick add): menu audio init (menu code 0x21F8DE calls
  0x37B822->0x37AD9C, a DSoundBuffer control method, with a 0.1f param) cascades through a tree of
  DSOUND-section methods that ALL trap: 0x37AD9C -> 0x379E9E(lock) + 0x380074 -> {0x37FCEE, 0x3811FA}
  -> {0x379C90, 0x37F407, 0x37FB3B, ...}. Each must be audited in the strict bounded audio adapter
  (h2_audio_guest_entry). This is a large, careful audio-HLE effort - the next milestone-scale piece,
  comparable to the deadlock just fixed. Task constraint: preserve existing audio settings (AUDIO_HOST),
  so the audio-unavailable profile is NOT an option; the DSOUND tree must be audited properly.
  SESSION 3h NET RESULT: the all-session frame-pacing deadlock is FIXED + committed + verified (fix233);
  the game now builds the menu past frame 136 (game_count 136->366, draws 118->141) until the menu-audio
  DSOUND tree, which is the next milestone-scale audit. Menu items not yet on screen. All committed/pushed.

- SESSION 3h AUDIO AUDIT QUANTIFIED: the DSOUND call-closure from the menu-audio entry 0x37AD9C is
  97 methods (direct-call only; indirect/vtable adds more) within the DSOUND section (0x379C60..0x38710C,
  ~54KB). Every one traps in the strict bounded audio adapter and needs auditing; some reach APU MMIO
  that our backend intercepts, so they can't simply "run unchanged" without correctness review. This is
  a large, fresh audio-subsystem effort (milestone-scale, like the deadlock). Options for a future pass:
  (a) audit the runtime-executed subset method-by-method (needs iterative build/run to discover the path),
  or (b) reconsider the interception boundary (hash-verify the DSOUND block + rely on APU interception),
  a bigger redesign. Either is substantial. Constraint: preserve AUDIO_HOST settings; no fabrication.
  SESSION NET: frame-pacing deadlock FIXED+committed (fix233, game_count 136->366, draws 118->141);
  menu items not yet on screen; next milestone = the 97-method menu-audio DSOUND audit.

- SESSION 3h AUDIO PROGRESS (huge): with the vblank fix + a menu-bring-up DSOUND passthrough
  (audio_host.c, gated XV_MENU_VBLANK, committed), the game runs 29 menu-audio DSOUND buffer/config
  methods and advances deep into the menu build (game_count 369, boot.log 340K->482K). Passed-through
  methods logged as [h2/audio-menu]. It now stops at stream_process 0x37AD25 (a HOST-implemented packet
  router, audio_host.c:626) - the menu submits an audio stream via the standard DSound vtable
  (call [ecx+0x10] at 0x2AE897, ret 0x2AE89A), but the handler's submission model expects callers
  0x33610E (initial)/0x335D7B (subsequent) and a specific stream state (flags 0x40000000, callback
  0x335D82, route_bin 27-30); the menu's path (caller 0x2AE89A) doesn't match. This needs REAL stream
  handling (route the menu's stream to the backend), not passthrough - deep audio-stream work, likely
  with more audio methods after. Menu items still NOT built (captures 141) - they are behind the full
  menu-audio setup. NET: all-session frame-pacing deadlock FIXED+committed; menu build now runs deep
  into menu audio; remaining = the menu-audio stream subsystem (large but ordinary bring-up).

- SESSION 3h *** MENU RENDERS *** (fix239, the goal breakthrough): raising H2_MENU_MAX_INDICES 8192->
  65536 and menu_render MAX_VERTS 20000->65536 (menu items are large indexed-triangle draws, 8192+
  16-bit indices that overflowed) + the graceful DSound stream failure cleared ALL blockers. The game
  now BUILDS THE FULL MENU: 100,000+ draw captures including the indexed-triangle item geometry
  (primitive=5, indices 6..8192+), zero overflow/reject/blocked, looping live (game_count 384+). The
  menu UI geometry RENDERS and presents to :111 (row of menu elements visible). First time ever.
  Commits: 023dbc7 (free-running vblank), 9fa7adb (menu-audio passthrough), c6ed726 (geometry cap +
  stream graceful-fail); all gated behind XV_MENU_VBLANK, default builds byte-identical.
  REMAINING for a POLISHED/RECOGNIZABLE menu (finishing work, not blockers): (1) COMBINER build (drop
  MENU_NOCOMBINER) for correct colors/shading/textures - the fast build shows glyphs as untextured
  boxes; (2) PRESENTATION - only 2 presents in a run; the game builds many frames offscreen but flips
  rarely, and the presented UI buffer (038E8000) lacks the backdrop (03A14000) composite; (3) INPUT for
  interaction. The hard part (root-cause deadlock + audio + geometry) is DONE.

- SESSION 3h *** MAIN MENU RENDERS RECOGNIZABLY *** (combiner240): built with the combiner (dropped
  MENU_NOCOMBINER; force-rebuild menu_render.o since make misses flag-only CFLAGS changes). The :111
  display shows the recognizable Halo 2 MAIN MENU: backdrop scene (terrain, sky gradient, 3D structures)
  + the vertical menu ITEM LIST. Zero blockers; the game builds/presents the menu. GOAL MET: the
  interactive menu with items renders. POLISH remaining (finishing, not blockers): (1) brightness -
  backdrop is under-lit (dusk scene + combiner too dark; check gamma/x2 scale/lighting-texture decode);
  (2) text crispness (resolution/glyph shading); (3) CONTINUOUS PRESENTATION - only ~2 presents/run,
  game flips rarely at the menu, and the two render targets (backdrop 03A14000, UI 038E8000) want proper
  compositing into the presented frame; (4) INPUT for interaction. Combiner is ~45s/frame (software
  per-pixel) - a real interactive menu wants GXM for the backdrop. All fixes committed+pushed
  (023dbc7 vblank, 9fa7adb audio passthrough, c6ed726 geometry cap + stream fail); gated XV_MENU_VBLANK.

- SESSION 3i (2026-09-16, per-render-target dumps, run buffers241): menu_render now dumps
  ux0:data/xita-halo2/menu-frame-<color_offset>.bin per target (compose with scratchpad compose.py).
  VERIFIED: (1) 03A14000 and 038E8000 are the game's DOUBLE-BUFFERED swap chain (display log:
  "original recurring flip" alternating between them, 120 swaps in 600s) -- NOT "backdrop vs UI".
  The 038E8000 dump is just a later frame of the menu's moving camera. 02C1B000 gets 4 draws, empty.
  (2) The textured prim=7 immediate quads (UI/glyphs) draw INTO the current back buffer, composited
  on the scene -- there is no separate UI layer to composite. (3) They are invisible because
  menu_texture.c decoded only DXT1/DXT23/swizzled-A8R8G8B8; every other format (A8/A8Y8/A4R4G4B4/
  DXT45/linear...) failed to load -> sampler returns black -> glyph quads vanish (they showed as
  boxes only in the diffuse-only MENU_NOCOMBINER build). (4) The rasterizer ignored captured ROP
  state: blend hard-coded to src-alpha (opaque backdrop got blended against black clear = too dark),
  no alpha test, no depth test (later geometry overwrote nearer = black bridge arch).
  FIX (this session): menu_texture.c decodes all swizzled/linear 8/16/32-bit formats + DXT45 with
  per-format outcome counters (menu_texture_stats, logged every 500 draws); menu_raster honours
  SET_BLEND_ENABLE/FUNC/EQUATION, ALPHA_TEST, and depth-tests against the game's own Z24S8 zeta
  surface (map_zeta in menu_render.c; knob XV_MENU_DEPTH=0 disables; XV_MENU_DETAIL=N logs N draws
  with bbox/v0/tex units/ROP state). NV2A DXT blocks ARE spatially Z-ordered here (prior agent's
  dxt23_layout_test, verified on the intro) -- keep block Morton. Texture unit regs: +0x10 CONTROL1
  (pitch<<16), +0x1C IMAGE_RECT (w<<16|h); +0x18 is a hole.
- SESSION 3i cont. (run rop242, commit 147b6c2 pushed): with ROP state + formats the frame is the
  real Halo 2 menu scene (Covenant assault carrier over the city, tether tower, bridge; peak
  brightness 45 -> 170). Texture histogram of the menu: 0C DXT1, 0E DXT23, 07 X8R8G8B8, 06, 0F DXT45,
  12 LIN_A8R8G8B8 (640x480 screen images -> cap raised to 1024^2), 1D LIN_A4R4G4B4 (256x256 FONT
  CACHE), 0B P8 (Halo 2 bump maps as t2 of 4-unit materials; palette at unit reg +0x20), 1A A8Y8,
  00 Y8, 01 AY8. TEXT IS DRAWN: depth-off prim=7 quads ~10x17 px on one line y=275-293, x=228-403,
  sampling the linear font cache with TEXEL uv (u=79,22,89..; v=1) -> NV2A linear images are
  texel-addressed; normalized-wrap sampling read texel 0 = invisible text. Multipass draws use
  depth EQUAL/LEQUAL (write0) over the base pass -> float z jitter speckles -> 16-unit tolerance.
  Glow-trail sprites: prim=7, 12x12 px, blend ONE/ONE_MINUS_SRC_ALPHA, z=far, DXT3 8x8 units.
- SESSION 3i cont. (runs text243/244): the centred text line rendered as SOLID BOXES. Dumped the
  decoded font cache (256x256 linear A4R4G4B4 at unit0 off=0375A000): glyphs live in rows 0-18 as
  white RGB + alpha coverage, and the glyph row spells "PRES TAOCNIU" = the distinct letters of
  "PRESS START TO CONTINUE" -> after the intro the game sits on the TITLE PROMPT waiting for Start
  (the frozen driver presses Start only during the intro, ~6 min too early). Two combiner bugs made
  the boxes: (1) menu_combiner.c read_in had the ALPHA-combiner channel bit inverted (bit4 set =
  .a per PS_CHANNEL_ALPHA=0x10 and the project's dx8_pixelshader_parse.py; it read .b = 1 for white
  glyphs); (2) unused texture stages sampled (0,0,0,1) -> phantom alpha 1 in the stage-1 sum
  t2*c0 + t0*v0 (xemu: unused stage = vec4(0)). Text combiner: 2 stages, c0=c1=white, final=r0.
  Lab helper private/late_start.py <stage> <label> [hold] [wait]: after run_stage returns (emulator
  still up), focus, screenshot, press Enter(=Start) via XTest, wait, collect dumps/log, stop the
  owned emulator. Enter->Start mapping is the lab's; other buttons unverified.
- SESSION 3i MILESTONE (run text245, commit ea67693 pushed): the :111 display shows the Halo 2 TITLE
  SCREEN with legible "PRESS START TO CONTINUE" over the rendered scene (screenshot sent to user).
  Remaining: (1) the presented frame is GREEN-TINTED; scanout gamma is identity (screen == 038E8000
  dump pixel-for-pixel) -> the tint is produced by the game's full-screen post-process pass (samples
  the 640x480 LIN_A8R8G8B8 scene image); mid-frame 03A14000 dumps are neutral gray. Need that pass's
  combiner state (now logged as "[h2/menu-render] fullscreen draw=..."). (2) Start not acted on: the
  input probe (boot.c h2_platform_pad, logs first 16 button CHANGES) saw digital=0010 only from the
  driver's 8 s intro press; my 1 s late press produced no change line -> too short for seconds-long
  software frames. input.c h2_input_state now logs poll cadence ("[h2/input] poll #N buttons=");
  late_start.py hold arg -> use >=12 s. Xbox START bit = 0x0010 in the sample.
- SESSION 3i cont. (run input246): GREEN TINT ROOT CAUSE: the menu's full-screen pass is
  r0 = dot(t0, c0) with stage c0 = 0x0080B333 (luma weights .5/.7/.2), final rgb = r0 * c0;
  menu_combiner_eval fed the FINAL combiner's c0/c1 from the last STAGE factors instead of
  SET_SPECULAR_FOG_FACTOR0/1 (0x1E20/0x1E24, captured by command_state) -> luma tinted by the luma
  weights = green. Fixed (menu_combiner final_factor0/1 + test). The raw pre-pass scene is warm
  sepia; the game desaturates+tints it (intended Halo 2 menu look). PERFORMANCE is now the wall:
  a title frame ~740 draws (fullscreen passes at draws 104/800/1542), ~95 draws/s -> ~8 s/frame,
  ONE XInputGetState poll per frame (40 polls in a 10-min run). Game DID see Start (poll #40
  buttons=0010) but no visible transition within 90 s (transition needs ~100 frames). Added a
  content-hashed decoded-texture cache (menu_texture_acquire, 40 MB LRU, key = unit regs + hash
  of all source bytes) and rate-limited the per-draw [h2/menu] log; next run waits 300 s after Start.
- SESSION 3i cont. (run cache247, commit 95cd70a pushed): TINT FIXED - title screen now renders in
  the correct dark desaturated blue-gray with legible "PRESS START TO CONTINUE". Texture cache 97%
  hits but draw rate unchanged (~44 draws/s) -> decode was not the bottleneck; added phase profiling
  (acquire/transform/raster ms in the 500-draw stats line). Start still ignored after 12 s hold +
  300 s: ROOT = game TIME pacing: only 438 vblank kicks in a 15-min run (~0.5 Hz). xd3d_vblank_kick
  fires only from yield/wait hooks, which never run while the guest thread sits inside the software
  renderer (~8-10 s per ~740-draw frame). Added vblank_pace_in_render() in host_channel_runtime.c
  (H2_MENU_RENDER only): after each completed menu draw, if >=33.3 ms since the last kick, deliver
  the original vblank callback via signal_game_vblank on active_context (set on each PUT write).
  freerun_swaps now shared by both paths; [h2/vblank] log rate-limited (first 200 + every 100th).
  Unknown: the game's tick catch-up cap per rendered frame (decides how fast the UI transitions).
- SESSION 3i cont. (run pace248 in progress): in-render vblank pacing WORKS (game_count 1000 by
  ~5 min vs ~150 before; no strict stop). PROFILE (8500 draws): acquire 1.8 s, transform 7.8 s,
  RASTER 154 s (94%) -> the per-fragment combiner/raster loop is the cost (~18 ms/draw avg).
  Speedups written (fidelity-neutral): menu_combiner_prepare (unpacked constants + tex_used mask,
  raster skips sampling unused units); menu_raster split into row-band jobs with an executor hook
  menu_raster_parallel; menu_raster_pool.c (Vita) = 2 worker threads (sceKernelCreateThread prio
  100, sema handoff) + caller band, for triangles >= 48 rows and >= 4096 px; knob XV_MENU_THREADS
  (default 2, 0 = inline). Build only when the lab emulator is stopped (link is memory-heavy).
- SESSION 3i cont. (run pace248): with real-time pacing the game REACTED TO START: after the press it
  submitted new geometry with SET_VERTEX_DATA_ARRAY_FORMAT attr1 = 0x814 (UB_OGL type 4, COUNT 1,
  stride 8) which command_state.c only admitted with count 4 -> strict stop (reason 3 at 0x3FAC58,
  screen black). Fixed: UB types 0/4 admitted with count 1-4; menu_render decode_attr reads `count`
  bytes; command_state_test updated. Pacing reached only ~3.5 Hz because vblank_pace_in_render was
  gated by active_flip_queued for most of each frame. Disassembled the D3D vblank handler 0x14280:
  always increments 0x485AB0/B4; only compares record.swaps with ds:[0x55E6B8] (last-seen swap) and
  on change records count-at-swap (0x485AB8/BC) + returns frames-since-swap; flags UNUSED. So the
  in-render tick now passes swaps = X_M32(0x55E6B8) (plain vblank, no fabricated swap) with no flip
  gate; complete_active_flip signals the real swap (serial+1). Stop artifacts (push/channel/arena
  snapshots) land in the lab ux0/data/xita-halo2 when a stop happens during late_start.
- SESSION 3i cont. (run menu249, commit c2086dd pushed): no strict stop; raster pool 1.6x; pacing
  7.4 Hz (big draws had no ticks inside -> added menu_raster_tick row hook, caller thread only).
  After Start the title screen FADES TO BLACK and stays black 300 s while still drawing the title
  scene + "PRESS START" text: the next screen's resources stream through loader thread 16
  (NtReadFile 0x600-0x1800 B chunks on handles 0x154/0x11C, 256 KB NtWriteFile to a cache file
  handle 0x114) at only ~0.4 reads/s. SCHEDULER MODEL: guest threads are FIBERS on one host thread
  (xk_yield -> xk_os_fiber_switch to the scheduler loop; pick_next round-robin), so nothing else
  runs while the render fiber is inside the software rasterizer; the loader only gets the gaps
  between multi-second frames. Plan: call xk_yield() from the pacing tick (every ~33 ms, caller
  fiber) with a PUT-write re-entrancy guard in host_channel_runtime. Raster worker threads are
  real sceKernel threads (independent of fibers). Also noted: some prim=5 draws carry huge texture
  v coords (e.g. 483499) = projective texcoords not divided by q (PROJECT2D stages) - fidelity TODO.
- SESSION 3i cont. (run yield250): xk_yield from the pacing tick works (no stop; game_count 14900)
  but render throughput fell ~3x (14k draws/run) and the screen stays black after Start. Thread map:
  4 init (exited), 8 main/render + job-queue consumer, 12 timer heartbeat (dominates yielded time),
  16 cooperative TASK WORKER (start 0x120C30; does the file I/O), 20 ?, 24 intro/cinematic thread
  (start 0x3E3E80, waits 0x140/144/148, EXITED at the intro end - not the missing main thread),
  53 audio sink host thread. AFTER START THE GAME BUILDS THE MAIN-MENU MAP CACHE: task on thread 16
  reads d:\maps\shared.map (h=154) / n:\cache002.map (h=11C, = mainmenu.map copy 59,670,016 B) in
  0x600-0x1800 B positioned reads and writes z:\cache000.map (h=114) in 0x40000 B chunks; the copy
  reached 12.58 MB in 300 s because ONE task step runs per scheduler round (one xk_yield). The title
  text keeps drawing at alpha .87 and the full-screen pass holds the frame black meanwhile (load
  hold). Added XV_MENU_YIELD_BURST (default 8 rounds per tick) + yielded-time accounting in the
  profile line + full-screen pass logging every 60th. Cache files live in the lab ux0
  save/cache5/cache000.map (grows during the load) and save/cache4/cache002.map.
- SESSION 3i cont. (run burst251, commit 8e61a77 pushed): the yield BURST did NOT speed the
  main-menu cache copy (101 reads/420 s vs 98 with 1 round vs 136 with no yields): the copy is
  paced per RENDERED FRAME (main loop enqueues a streaming job per frame), so yields only slow the
  frames (79% of raster wall time handed to the timer fiber). Use XV_MENU_YIELD=0 (env.txt); keep
  the vblank pacing. cache000.map grows ~25-40 KB/s -> the load needs a long wait (or a fast GXM
  renderer). Yield accounting is in the profile line ("yielded=ms/count").
- SESSION 3i cont. (run long252, yields OFF, 900 s post-Start): no stop; 72,800 draws; 128 input
  polls; the black hold persisted while z:\cache000.map grew to 87 MB (from ~16 MB) at ~60-90 KB/s
  paced per rendered frame. This is Halo 2's FIRST-BOOT CACHE BUILD: cache002.map (59,670,016 B)
  = mainmenu.map copy (done during the intro), cache000.map = copy of d:\maps\shared.map
  (183,370,752 B) -> ~35 more minutes of copying at this frame rate before the main menu can
  appear. On a real Xbox the cache persists on the HDD, so second boots skip the copy: the lab
  must PRESERVE the finished cache between runs (check preserve_fresh_cache.py semantics) rather
  than resetting it. Yields off is the right default (XV_MENU_YIELD=0 in env.txt).
- SESSION 3i cont.: lab cache semantics VERIFIED: preserve_fresh_cache.py moves save/cache4 (+.raw)
  to <label>-prior-cache4 and recreates an EMPTY cache4 (n: drive; the intro re-copies mainmenu.map
  there as cache002.map every run - the driver waits for its 59,670,016 B before pressing Start).
  save/cache5 (z: drive, cache000.map) is NOT reset, but the game RESTARTS the shared.map copy each
  boot (partial file invalid): sizes 12.6 -> 15.7 -> 87 MB across runs, copy begins at the title
  screen (25 MB at +555 s, before Start). Run cache253 = boot + Start + 40 min wait to let the
  ~183 MB copy finish in one run; once complete, later boots should skip it (as on an Xbox HDD),
  so keep cache5/cache000.map afterwards (back it up).
- SESSION 3i (run cache253, 12:46): the first-boot cache build COMPLETED: cache000.map =
  183,370,752 B (= shared.map size; content not byte-identical), finishing in the last ~6.6k log
  lines of the 50-min run (game had no time to react). Backup: private/menu224/cache000.map.complete.
  NOTE the display was already black at the Start press (+600 s) this run: black = "cache build in
  progress" state, not the post-Start fade. Next: boot254 = normal run with the completed cache in
  place (cache5 is never reset) to see whether the game skips the copy and Start reaches the menu.
- SESSION 3i KEY FACT (cache253 log lines 4637-4640): at boot thread 8 DELETES z:\cache000.map
  itself (NtOpenFile DELETE access -> NtSetInformationFile FileDispositionInformation(13) ->
  NtClose -> "[xk] deleted") and rebuilds it from d:\maps\shared.map (183,370,752 B) starting at
  log line ~4941, i.e. during the intro. This mirrors the Xbox: the kernel clears the Z: cache
  partition on every title launch and Halo 2 copies shared.map behind the intro movies (DVD speed
  ~1-2 min; here ~43 min because the copy is paced by rendered frames). A saved complete cache
  CANNOT be carried over (deleted at boot) and must not be faked. So the menu needs ONE run with
  copy time + transition: run menu255 = run_stage + late_start(12 s, 3000 s) with XV_MENU_THREADS=3.
  Prepared for the next build: rate-limited [h2/kernel] heartbeat logging (boot.c, first 400 then
  every 1000th of KeRaiseIrqlToDpcLevel/KfLowerIrql/KeSetTimer) and raster parallel threshold
  24 rows/2048 px.
- H2 GXM BACKEND PLAN (the real fix; software raster = ~10-15 s/frame): keep menu_draw capture +
  command_state; replace h2_menu_software_render with menu_gxm.c mirroring quad_gxm.c: (1) vertex
  shader = generic "already-transformed" pass-through is NOT possible for indexed draws -> run the
  captured NV2A vertex program (s->program) as a GXM vertex shader with s->constants as uniforms
  (quad_gxm.c does exactly this for the intro); (2) fragment = CE tools/ps_pipeline.py combiner->
  shader generator (same NV2A register format as menu_combiner decode; add final-combiner c0/c1
  from 0x1E20/24 and the unused-stage = 0 rule); (3) textures: reuse menu_texture decode into GXM
  linear RGBA textures via the content-hashed cache (P8/linear/DXT all decode to RGBA8; texel-
  addressed linear images need uv/size scaling in the VS or sampler); (4) state per draw: blend
  (0x304/344/348/350), alpha test (uniform, as CE xv_atest), depth test/mask (0x30C/354/35C) against
  a GXM depth surface, scissor from clip regs; (5) render targets: the game's 3 colour buffers
  (03A14000/038E8000/02C1B000) -> GXM colour surfaces in CDRAM; the in-place full-screen pass
  reads the same buffer it writes (needs a copy/resolve); text quads draw straight into the back
  buffer; (6) present = existing scanout path reading the buffer back (or map the GXM surface as
  the scanout source). Keep the software path as XV_MENU_SOFT=1 fallback/oracle for A/B checks.
- SESSION 3i (run menu255, 3 workers, commit 80b48cc pushed): shared.map -> cache000.map completed
  at +2700 s but the screen stayed black: the build then copies d:\maps\single_player_shared.map
  (429,017,088 B) -> z:\cache001.map (54.5 MB by run end). Total first-boot build ~612 MB at
  ~1.2 MB per rendered frame (~13.5 s/frame; raster 59% of wall, game+log the rest) => ~2.5 h per
  boot. Run menu257 = run_stage + late_start(12 s, 10800 s) launched 13:52 with the new build;
  watcher live257 screenshots every 10 min and signals when cache001 >= 429 MB and the screen is
  non-black. Frame rate is the currency for everything now (copy, transitions, input polls).
- GXM PLAN REFINED (read quad_gxm.c/ps_pipeline.py): this codebase ships OFFLINE-compiled .gxp
  shaders (tools/shadercomp; CE's ps_pipeline.py turns logged (VP, combiner) pairs into
  ps_<hash>_<mask>.frag.cg; VP->Cg exists for CE's halo_vs_NN). quad_gxm.c = the pattern: patcher,
  fixed VP with c[178] uniforms, A8R8G8B8 640x480 colour surface + S8D24 depth, textures via
  sceGxmTextureInitLinear, one scene per draw batch, sceGxmNotificationWait. H2 has no on-device
  compile (CE has libshacccg). Prerequisite = the menu's shader INVENTORY: added
  "[h2/menu-shader] pair#N vp=<hash> ps=<hash> ..." once per distinct (VP microcode, combiner
  words, 0x1E70 stage ctl) pair + private dump ux0:data/xita-halo2/menu-vp-<hash>.bin (start,
  len, 136x16 B microcode) in menu_render.c (next build). Then: generate Cg per pair offline,
  compile, embed, select by hash at runtime; software path stays as the oracle.
- SESSION 3i (run menu257, 16:17-16:27): BOTH cache copies completed (183+429 MB, ~2.3 h). Then
  the game spawned two Bink-player threads (entry 0x3E3E80, ids 28/32) and opened
  d:\bink\attract_60.bik = ATTRACT MODE (idle timer long expired during the copy); its frames
  render through the generic menu path as a full-screen prim=7 quad sampling a 640x480
  LIN_X8R8G8B8 image at 0x01064000 (loads OK) -> screen went uniform gray (mean 35). A 12 s Start
  skips it (threads exit) but the game returns to the TITLE with the fade held at BLACK (scene
  vertex colours (0,0,0,1), final tint black; the "PRESS START" glyph quads still drawn), then
  restarts the attract movie ~60 s later (idle timer still expired) -> attract loop. Three spaced
  Start presses (packets 9-12) and a B press changed nothing visible. late_start.py was killed
  (so its 17:02 stop would not fire); the emulator is managed manually (still running at 16:27).
  Open question: what releases the fade after attract mode (audio stream from caller 0x2AE89A
  route -1 keeps failing with DSERR every ~7 s - the menu/movie music stream - suspect).
  Every experiment now costs ~2.5 h per boot (cache build) -> visibility diagnostics must be
  batched into one build: final-combiner c0 per frame, audio-stream failure reasons, UI state.
- PLAN (16:30): the attract loop is an IDLE-TIMER effect of the 2.5 h cache build. Next run uses
  private/menu_run.py (Start at +600 s, then a 1 s D-pad Down tap every 45 s as keep-alive,
  screenshot every 5 min, stop when the picture returns after cache001 >= 429 MB, or at `wait`).
  Diagnostic build diag258 adds final_c0/c1 (fade/tint) to the fullscreen log and the
  [h2/menu-shader] inventory + menu-vp-*.bin dumps. The failing menu stream (flags 0x20000000,
  callback 0x220730, route -1, caller 0x2AE89A) is a different stream type than the handled
  0x40000000/0x335D82 ones - real audio work (unknown callback ABI), deferred.
- GXM SHADER PIPELINE PROTOTYPE (17:30, while run keep259 copies): scratchpad/h2shaders/menu_ps.py
  parses "[h2/menu-shader]" lines -> synthesizes D3DPIXELSHADERDEF (PSDEF_FMT in
  recompiler/dx8_pixelshader_parse.py; c0/c1 mapping identity, final consts 0x98) -> decode_psdef ->
  pixelshader_recomp_gen.generate -> .frag.cg: ALL 17 menu combiner programs generate. Added an
  opt-in NONE_STAGE_ZERO flag to pixelshader_recomp_gen.py (NONE stages read as 0; CE default off).
  Inventory line now also logs vp_start, ctl(0x1E60) cmp(0x1E6C) dot(0x1E74) inp(0x1E78) and the 16
  vertex attribute formats (attrs=) for the GXM vertex layout (next build). Vertex side: menu-vp-*.bin
  (19 so far, 4-55 slots) -> dx8_shader_parse.decode_instruction per slot -> ShaderFunction JSON +
  synthesized declaration -> shader_recomp_gen (uniform c[192], C_BASE=-96 contract) -> .cg.
  Compile: private/compile_shaders.py runs xv_shadercomp (recovered vpk installed as XVSC00001 in the
  LAB pref path; libshacccg.suprx already in lab ur0/data) on :111 ONLY when no emulator runs; never
  use tools/vita3k.sh (its kill_emu pkills every Vita3K, incl. CE's).
- GXM PIPELINE STATUS (18:00): scratchpad/h2shaders/{menu_ps.py,menu_vs.py} generate 17 FS + 19 VS
  Cg + binding JSON from the live inventory/VP dumps. Contracts: VS = `uniform float4 c[192]`
  (hardware numbering, c_base 0 unless the manifest says otherwise), attributes = used v-regs as
  F32x4 (name/regIndex per manifest), output position = window->clip transform of the Xbox
  epilogue output (oPos.xy window, w = clip w; z window/16777215) with the quad_gxm viewport
  (320,320,240,-240, zScale 1). FS = samplers texN for used units, `psc[18]` = factor0[0..7],
  factor1[0..7], final c0/c1 at 16/17 (generator ignores c0/c1 maps; UNIQUE bits set),
  xv_fogcolor, xv_atest(ref,func,enable); NONE stages = 0 (NONE_STAGE_ZERO). TODO: texcoord scale
  uniform for texel-addressed linear images. Runtime design: CPU vertex fetch into packed F32x4
  streams per VS manifest; scenes batched per colour target with guest<->GXM surface sync at
  target change / self-sampling / clear / flip; GXM depth surfaces cleared on the game's zeta clears.
- GXM BACKEND WRITTEN (18:40): games/halo2_5849/menu_gxm.c (+.h), opt-in XV_MENU_GXM=1 in env.txt,
  falls back per draw to the software path when a shader pair is missing. tools/h2_menu_shaders.py
  = the pipeline (log + menu-vp dumps -> local/halo2_5849/menu-shaders/*.cg + .json). NEXT (needs
  the lab free): (1) private/compile_shaders.py local/halo2_5849/menu-shaders -> .gxp via
  xv_shadercomp XVSC00001 in the lab emulator (libshacccg in lab ur0/data); (2) rebuild (Makefile
  packs h2menu_*.gxp); (3) run with XV_MENU_GXM=1 and compare per-target dumps against the software
  oracle (menu-frame-*.bin) at the title screen; watch [h2/menu-gxm] lines for FAIL/no compiled
  program. Design notes: window->clip transform assumes the quad_gxm viewport (320,320,240,-240,z 0..1);
  psc[0..7]=c0[i], psc[8..15]=c1[i], psc[16/17]=final c0/c1; xv_texscale per unit; NONE stage = 0;
  point filtering + REPEAT (CLAMP for texel images) to match the oracle.
- GXM pipeline details (commits f9c63a3, 637aa7c, 04061b3 pushed): fragment programs are per
  (combiner, vertex program) pair -> app0:h2menu_ps_<ps>_<vp>.frag.gxp, generated with
  VARYINGS_AVAILABLE = the paired VP's outputs (most menu VPs write no oFog); vertex programs use
  c_base 0 (shader_recomp_gen honours args.c_base) so s->constants uploads verbatim. Title-screen
  set = 27 pairs / 19 VPs / 17 combiners; .cg staged in local/halo2_5849/menu-shaders (git-ignored).
  The full stage links with menu_gxm.o (build-gxm260). Validation still pending (needs the lab).
- SESSION 3i (run keep259, 19:00): keep-alive D-pad taps every 45 s were SEEN (packets to 73) yet
  the attract movie (attract_60.bik, threads 28/32) started immediately when cache001 completed
  -> attract mode is not (only) an idle timer; likely part of the boot flow after the load. Start
  skips it (threads exit) but the title stays BLACK: the desaturate tint final_c0 = 0x001A2340 is
  unchanged since the visible title, while the scene draws' vertex colours are (0,0,0,1) (were lit
  at the title) => the fade is done through per-vertex lighting / game state, not the combiner
  constant; Start (packet 76) does not release it. The movie itself decodes only ~24 frames (no
  [h2/movie] decode lines after the open; movie-header invocations only) -> Bink playback stalls,
  probably on its DSound stream (flags 0x20000000, cb 0x220730, route -1, DSERR every ~7 s from
  caller 0x2AE89A; created at 0x2AE692 with callback 0x220730 packet_limit=2 mono). Next RE target
  if pursued: why the post-attract title keeps lighting at 0 / which event the UI waits for
  (audio stream start is the prime suspect). Log/dumps snapshot: scratchpad/r_keep259.
- LAB FACT: the recovered xv_shadercomp.vpk (vita-recovery-20260913, installed in the lab pref
  path as ux0/app/XVSC00001) scans ux0:data/xboxvita/shaders (old project name), not
  data/xita/shaders; private/compile_shaders.py stages .cg there, runs the app on :111 with the
  lab config (only when no emulator runs), and copies the .gxp + compile.log back.
- 19:12: all 46 menu shaders (27 FS pairs + 19 VS) compiled with 0 failures via xv_shadercomp in the
  lab (build-gxm262 packs h2menu_*.gxp). Validation run gxm263 launched with env.txt XV_MENU_GXM=1
  (+VBLANK, DETAIL=300, YIELD=0, THREADS=3); compare menu-frame-*.bin at the title screen against
  the software oracle (scratchpad/r247) with scratchpad/abcompare.py; look for "[h2/menu-gxm]"
  lines (ready/target/vertex program/FAIL/no compiled ...).
- LAB FACT: run_lab.py extracts a fixed WHITELIST of VPK members into ux0/app/XH2B00001 (eboot +
  the named intro shaders/contracts); new VPK files do not reach app0. menu_gxm.c therefore loads
  programs from ux0:data/xita/shaders/<name> first (lab pref path private/vita3k/ux0/data/xita/
  shaders, 46 .gxp staged there), app0 fallback (commit 678459c). Run gxm263 fell back to software
  for every draw for that reason (GXM init + 3 targets were fine); gxm264 is the real validation.
- GXM VALIDATION (runs gxm263/264, 19:20-19:28): GXM init OK, 3 targets, scenes batch ~20 draws,
  flush/sync hooks fine, NO API failures. Bugs found+fixed: (1) programs not in app0 (whitelist)
  -> ux0:data/xita/shaders; (2) vertex attributes bound 0: compiled names are "IN.position" etc.
  (commit edc5eae). Compiled FS expose psc/tex0..3/xv_atest/xv_texscale (xv_fogcolor may be
  optimized out - runtime tolerates NULL params); VS uniform "c" present. New VPs keep appearing
  (4 more at the title: d18d3356e17fa32b, 6bd72b5608ac69df, ...) -> after each run regenerate from
  its log + menu-vp dumps (tools/h2_menu_shaders.py), compile (private/compile_shaders.py), stage
  to lab ux0/data/xita/shaders. Run gxm265 = first run with attributes bound.
- GXM MILESTONE (run gxm265, 19:31): the GPU backend renders the Halo 2 title screen recognizably
  (carrier, tether, bridge, legible "PRESS START TO CONTINUE", correct tone) - 14k draws within
  ~90 s of the menu starting (~150 draws/s vs ~40/s software, with a flush+dump every 20 draws).
  Attributes bind (IN.<name>), scenes batch 20 draws, no API failures. Defect: blocky sky = the
  fog layer's SFACTOR CONSTANT_COLOR (0x8001) unmapped in GXM -> fixed by folding into the FS
  (xv_blendconst, generator BLEND_CONST flag, commit pending build). 4 VPs still missing at the
  title (4272956850766903, 6bd72b5608ac69df, d18d3356e17fa32b, ddc93d3e8ca8a021) -> regenerate from
  the gxm265 log + dumps. Screenshot sent to the user (scratchpad/live265/halo2-gxm-live.png).
- 19:35: GPU path measured 166 draws/s WITH a flush+readback every 20 draws (software ~40/s);
  swaps ~1/s. The 4 "missing" VPs were empty BEGIN/END draws (now short-circuited before lookup).
  local/halo2_5849/menu-shaders regenerated (27 FS pairs + 19 VS, with xv_blendconst); chain queued:
  compile -> stage ux0/data/xita/shaders -> build gxm266 (XV_MENU_GXM_DUMP default 200) -> run
  gxm266. NOTE: never `rm` globs in local/menu-shaders before the tool runs (zsh glob failure earlier
  wiped the set; regenerated from scratchpad/r_keep259/boot.log + h2shaders/vp).
- 19:58 (run gxm266, blend-const build): GPU 241 draws/s sustained, ~1 swap/s; cache000 (183 MB)
  finished ~7 min after the menu started, cache001 running -> the whole first-boot build should
  take ~25-30 min on the GPU path (was 2.5 h). Start pressed at 19:57:41 (probe 0010). Remaining
  GPU artifact: fine vertical striping on textured ground/city (software oracle clean) -> A/B
  probe added (XV_MENU_GXM_AB=N env knob, "[h2/menu-ab]" lines with mean|diff| per sampled draw +
  program/texture info; build ab267). Scene batching 200 draws; no API failures; empty draws
  short-circuit.
- SESSION 3j (2026-09-16 evening, runs gxm266/sig268): ROOT CAUSE of the every-boot 612 MB cache
  rebuild: Halo 2 signs z:\preferences.dat with XCalculateSignature(flags=1 NON_ROAMABLE). XAPI's
  non-roamable End (0x2D6532) HMACs the inner digest with the 16-byte kernel export XboxHDKey (import
  slot 0x4115E8, ordinal 323); if those bytes are ALL ZERO it falls back to the EEPROM key via
  ExQueryNonVolatileSetting(0xFFFF) (unsupported -> uninitialized stack key). xk_crypto.c exported a
  zero HD key -> every boot the stored signature mismatched -> preferences loader (0x121440 region:
  sig compare at 0x1214BE, state==8, size==0x1FC) -> reset defaults 0x121220 (cache version 0x510A08=0)
  -> cache_files_initialize 0x2136C0 sees version != 0x2651 -> delete_range(0..6) via 0x2138F0 ->
  z:\cache000/001.map deleted and re-copied. Fix: non-zero virtual HD key in xk_crypto_init (build
  sig270). Evidence: boot.c "[h2/sig-path]" entry traces (fn/return/eax/args) + loader dump
  (computed/stored/bytes/state) at 0x121220/0x121280. Offline HMAC check of the file never matches
  because the stored sig was made with stack garbage. NOTE: the memory-snapshot value 0x470024 =
  FFFFFFFF is NOT evidence of Begin failure (End resets it); Begin works (heap 0x2D09C0 -> 0x2D152F).
  Also: 0x213970 deletes slots 6..19 unconditionally at every boot; n: (partition 4) is formatted at
  every boot by XAPI 0x2D5D40 (utility-drive table at partition0 sector 4, sig 0x97315286) - by design.
- SESSION 3j GPU-path observations (run gxm266): the attract movie RENDERS (real Bink frame, letterbox)
  on the GXM path; after a Start skip the title is dim for ~5 min, then BLACK (screen mean 0.3), and
  any press (Start/X) brings back the bright "PRESS START" title -> it's an idle-dim/attract state,
  not a hang. BUT Start at the bright title (1 s and 12 s holds, packets registered 12/13) does not
  advance to the menu within 3-5 min; the game keeps flying the title camera, reads small sound chunks
  from n:\cache002.map after each press (0x400-0x4A00 B) and the menu DSound stream Process keeps
  failing (DSERR). Game frame rate ~0.7 fps (one input poll per frame: 64 polls/90 s), flips ~6/min.
  Stream ABI decoded: pool of 20+ XBADPCM mono streams (context = pool index), callback 0x220730
  (pStreamContext=index, pPacketContext!=0, dwStatus) -> decoder->[0x18](entry,ctx,status) then
  refill via 0x2AE820 (GetStatus [0xC] READY bit -> decoder [0x20]/[0xC] -> Process [0x10] with
  XMEDIAPACKET on stack; failure -> decoder [0x14]). Stream thunks 0x37B7FF..0x37B822 -> 0x37B370/
  31E/3C2/414(SetEG from 0x2AE817)/466/4B8(headroom)/50A/AD9C.
- LAB NOTES: ux0 boot.log has NO timestamps; private/vita3k/native-attempt-<label>-emulator.log has
  them (emulator stdout). Stop only by PID (late_start.py style); menu_run.py's pkill -x Vita3K would
  hit other emulators. zsh aborts on unmatched globs (use grep -rl without --include globs). run_stage's
  drive_startup step can fail on an ffmpeg window grab while the emulator keeps running (harmless).
  XV_MENU_GXM_AB=200 added to lab env.txt (A/B probe lines "[h2/menu-ab]").
- SESSION 3j cont.: REINTERPRETATION of the 3h "menu renders" milestone (fix239 log: first menu draw
  right after the intro Start press; the 18837-index draw is the TITLE backdrop): no run has ever
  reached the real main menu (Campaign/Multiplayer list). Start at the "PRESS START" title has never
  produced a transition. HD-key fix VALIDATED offline: the file the game wrote under sig271 verifies
  with HMAC(hd,HMAC(key16,data)) == stored; the next boot (sig274) checks acceptance in-game.
  The UI clock 0x54D5B8 advances by dt = vblanks/60 (0x12B0E0), clamped to [0,10 s] -> no per-frame
  tick cap in the UI clock; dt reads the vblank counter 0x485AB0 (our in-render pacing).
  Vita sceClibPrintf: no %f; a SECOND 64-bit vararg misaligns the rest - keep log args 32-bit.
  Tools: scratchpad newfn.py (summarize [h2/newfn]), gtime.py (game-time vs wall clock),
  launch274.sh pattern (wait build -> stop by PID -> run_stage -> watcher -> poller).
- SESSION 3j RECOMPILER BUG (fixed, commit after 0a061f3): dead_flag_writes kept a set of iced
  Instruction objects; iced equality ignores IP -> identical instructions in a block shared the
  "dead flags" verdict. Halo 2 preferences loader block 0x1214DA: `and al,cl` x2 -> the live copy
  before `je` lost X_FLAGS -> je read `cmp esi,1FCh` -> defaults every boot (independent of the
  signature). Fixed by keying on ins.ip (verified with a single-root lift: `python -m
  recompiler.xita_recomp default.xbe --roots 121350 --no-data-roots -o <dir> --files 1`).
  Stage codegen = games/halo2_5849/prepare_boot.py <xbe> --out <stage>/boot --host-channel
  --audio-host (menu224/boot is a symlink to datascan222/boot). New stage flags276 regenerated.
  Also measured: game UI clock ran at 0.12-0.15x wall (9 vblank kicks/s) -> vblank_pace_in_render
  now catches up at 60 Hz (vb275). Start at the title (registered, 20 s hold) entered ZERO new
  functions (newfn tracer) -> the title ignores Start in that state.
- A/B probe (sig274, 588 draws, fixed format): 286/588 draws have mean|diff| >= 20 with worst=765
  (pixels completely different) - dominated by the small DXT1 256x64 + 4x4 X8R8G8B8 scene polys
  (blend 0, depth on, 1 stage) and the font-cache text quads (1D 256x256, 0302/0303) -> the GPU path
  differs in GEOMETRY/rasterization coverage (position/half-pixel/viewport), not just colour; the
  striping on textured ground is likely from that or UV scaling. Deferred behind the menu progression.
  The recompiler dead-flag fix restored 1057 X_FLAGS stores across 87/128 generated files.
- SESSION 3j VERIFIED IN-GAME (run flags276, stage private/flags276 = regenerated codegen with the
  dead-flag fix + HD key + 60 Hz vblank catch-up): preferences loader ACCEPTED (0x121280, computed ==
  stored), z:\cache000.map (complete, 183 MB) KEPT at boot; only the partial cache001.map (88 MB,
  killed mid-copy) was deleted and restarted -> the game validates each cache slot, partial files are
  safe. Boot-to-title no longer rebuilds the 612 MB cache once both slots are complete.
- *** SESSION 3j MILESTONE (run flags276, 21:12): Start at the title now TRANSITIONS: the game shows
  its CHOOSE PROFILE screen (Create New Profile / Default, A select, B back) - the first real
  interactive Halo 2 menu ever reached. Unlocked by the recompiler dead-flag fix (commit 471ffc9;
  1057 restored flag stores) + 60 Hz vblank catch-up (game time now ~real time; vblank ~60-77/s).
  Next: Down -> Default -> A -> main menu (Campaign/Multiplayer/...). Vita3K keys: Enter=Start,
  x=A(cross), c=B(circle), arrows=D-pad.
- *** GOAL REACHED (run flags276, 21:14:10): Halo 2 MAIN MENU rendered by the game on the GPU path:
  CAMPAIGN / XBOX LIVE / SPLIT SCREEN / SYSTEM LINK / SETTINGS over the backdrop. Path: Start at the
  title -> Choose Profile (Down to "Default") -> A. Screenshots live276/p-after-a045.png (sent to user).
  Stage private/flags276 (regenerated codegen + all fixes through commit 471ffc9).
- Main menu NAVIGATION verified (21:15): D-pad Down/Down/Up moved the highlight CAMPAIGN -> XBOX
  LIVE -> SPLIT SCREEN -> XBOX LIVE (live276/nav-grid.png sent). Remaining polish: GPU striping
  (A/B probe shows geometry-coverage diffs), menu audio stream (DSERR, XBADPCM pool, callback ABI
  decoded), frame rate ~0.5 fps on the GXM path (CPU-side submission), gxm266-era tools in scratchpad.
- MULTIPLAYER LAUNCH (21:31-21:49, run flags276): main menu -> SPLIT SCREEN (2 Downs from CAMPAIGN)
  -> A -> lobby profile page ("Profile: Default / PRESS A TO CONTINUE") -> A -> PREGAME LOBBY
  (START GAME highlighted; Slayer on Ivory Tower; 1 player) -> A -> "Waiting for everyone to load
  the map" -> the game finished the cache001 copy first (27 MB/min at the lobby), then opened
  d:\maps\cyclotron.map (46.2 MB, Ivory Tower) -> copied to n:\cache003.map -> STRICT STOP:
  NV2A MMIO read 0xFD003240 from 0x12E150 (a second copy of the D3D PUT/GET/busy probe, not in
  hooks.py INLINE_QUEUE_STATUS). Fixed in hooks.py (guarded sites 0x12E161/67/6F), stage mp278
  (codegen + -O0 build) with scratchpad mpdrive278.py automating title->menu->lobby->START GAME.
  A -O1 guest build of flags276 code exists in private/o1-277 (untested at runtime).
- MENU DRIVING FACTS (run mp278): the game REMEMBERS highlights - Choose Profile opens with the last
  used profile ("Default") highlighted and the main menu with the last item (SPLIT SCREEN): a blind
  Down from the driver selected "Create New Profile" (virtual keyboard, "Halo0001"); B backs out to
  the title. Drive one verified step at a time (scratchpad mp_key.py <keysym> <hold> <wait> <shot>).
  Start countdown "0:04" shows on START GAME; the map load waits for the pending cache001 copy.
- Run mp278 (22:26): map copy completed, then strict stop on the NEXT probe copy (0x12E0DC in
  0x12E0C0). All 17 game-section copies of the inlined probe are now admitted (hooks.py, generated
  from an XBE byte-pattern scan; commit after 51e58b2). Stage mp279 = that codegen; chain279.sh /
  mpdrive279.py (Start, A, A, A, A - relies on remembered highlights). No preferences save happened
  after the cache001 copy in mp278 -> the next boot recopies it (~16 min at the lobby).
- Run mp279 (23:02): all MMIO probes passed; the LEVEL LOAD then strict-stopped on `cmpltps`
  (recompiler had no cmpps). Added cmpps/bswap/pmovmskb (commit after 60413cd). Stage mp280 =
  that codegen; chain280.sh runs codegen+build+run+mpdrive280.py (Start, A, A, A, A). Each cycle
  ~25 min + the cache001 recopy (~16 min) because strict-stop exits skip the game's cache bookkeeping.
  Remaining unimplemented mnemonics in real code: mulsd (4 sites; no SSE2 double model), stmxcsr/
  ldmxcsr (hooked elsewhere), the rest are data mis-decodes.
- Run mp281 (00:13): with 0x1710 admitted the level load ran ~1 min of resource streaming (reads on
  cache000/cache003 via thread 16), then the MAIN THREAD SPINS in guest code with no kernel calls
  (esp 0x5E5F88, last samples KeTickCount reads 0x3314B0 / tag lookup 0x122D00 / 0x9F550), thread 16
  idle on semaphore 0x48, thread 20 = host audio worker (no guest start). Preempts continue (~17/s,
  slice 20000). Run mp282 = same build with env XV_SPIN_BT=1 (logs "spin fn" every 1024 preempts)
  and XV_PREEMPT_SLICE=2000 to identify the spin site / let the I/O fiber run more.
- Run mp283 (01:00, XV_SPIN_BT): level-load stall ROOT CAUSE = the D3D vblank wait 0x12B2E0
  busy-loops on 0x485AB0 (no kernel calls); vblanks were only delivered by draw hooks / yield hooks.
  Fix: xd3d_lockstep_preempt (weak hook called by xv_preempt each slice) -> vblank_pace_in_render
  when c == active_context (commit after 981f439). Stage mp285 = mp280 codegen + this; chain285.sh.
  Spin diagnostics: XV_SPIN_BT=1 in env.txt prints "[xv/x86] spin fn <eip> regs ..." every 1024
  preempts; XV_PREEMPT_SLICE=2000 made everything ~2x slower (reverted).
- Run mp285 (01:33): the preempt-hook vblank fix cleared the load stall; the first in-game PUT then
  hit method 0x17C8=1 (CLEAR_REPORT_VALUE, occlusion query). Implemented the Z-pass pixel-count
  query for real (commit after ecd2697): consumer methods 0x17C8/0x17CC/0x17D0 + report write to
  DMA_REPORT (slot 10 = method 0x1A8; NOT 0x198 which is ZETA), software rasterizer counter,
  GXM visibility test (sceGxmSetVisibilityBuffer, INCREMENT, 4 cores x 4 KB). Stage mp286.
  NV2A context DMA slots: 0x180 notifies,0x184 A,0x188 B,0x190 state,0x194 colour,0x198 zeta,
  0x19C vertex A,0x1A0 vertex B,0x1A4 semaphore,0x1A8 report.
- Run mp286 (02:11): the Z-pass query passed; next stop = audio_host effects_description 0x37BA6F
  "unreviewed reverb description" (level I3DL2 preset, caller 0x21EE74 index 9, custom values).
  Admitted by DSI3DL2LISTENER range validation + any all-masked fcw (commit after 63910b0).
  Stage mp287. Backend note: backend_queue_reverb refuses a second update while one is pending
  (reverb_pending) - watch for "reverb monitor queue busy" if the level updates reverb repeatedly.
- Run mp287 (02:45): the level's reverb description was refused again by the remaining strict
  entry conditions (not the preset); the startup call passed (fcw 0x23F, fsp 0). Relaxed the empty
  x87-stack requirement (conversion must preserve fsp) and added a "[h2/reverb] refused ..." line
  that names every condition (commit 90993b7). Stage mp288 (chain288.sh, mpdrive288.py).

- SESSION 3k (2026-09-17 03:17-03:40). mp288 stopped in the level load at
  `sound entry=0037B66F reason=unsupported FX15..22 mute state/value value=FFFFFAF1` (=-1295).
  Caller 0x21F1A0 = the sound system's ZONE LOOP 0x21F069 (4 buffers at [state+0x2A70+i*4]):
  gain from zone boxes -> 2000*log10(g) (consts 45DDE8=2000, 45DF80=1/pi) -> clamp [-6400,0] ->
  SetVolume. Original SetVolume chain: 0x37B66F -> 0x37B198 -> 0x37A5D4 (stores volume-headroom at
  settings+0x1C) -> 0x381CE4 -> 0x380B97 packs per-mixbin attenuation = -(bin gain+volume)*64/100,
  unsigned, clamped 0xFFF (1/64 dB units) into VP regs FE820360/364/368 (voice select FE8202F8).
  Hardware gain (xemu vp.c attenuate()): 10^(-a/1280), 0xFFF = silence. -6400 AND -6399 both -> FFF.
  Fix (commit b048755, pushed): h2_audio_fx_source.attenuation + h2_audio_fx_attenuate(15..22) (mute =
  attenuate(FFF), output_mask kept), mixer multiplies samples by source_gain(); fixed_commit_ready no
  longer needs muted_extra==255; host buffer_control admits caller 0x21F1A0 volume in [-6400,0]
  (FFF -> fx_mute path, else h2_audio_backend_fx_attenuate). Log: "[h2/fxin2] caller=0021F1A0 ...
  volume=%d original attenuation%03X".
- audio_dsp_test was ALREADY FAILING at HEAD (a3be24b/90993b7 changed the reverb contract to range
  validation without updating it). Fixed in b048755: fields refused just outside DSI3DL2LISTENER
  ranges, in-range presets admitted, fsp/fcw=0x7f/FPSCR bits admitted; fcw 0x37e/0x300 refused.
  The test's xv_logf stub XORs native_fp, and reject()/call() assert FPSCR intact -> any host log
  before a possible fail() must be bracketed with h2_platform_fpscr_read/write (done for both reverb
  logs). Full `make -C games/halo2_5849 BUILD=<dir> test-host` = 63 tests, all pass.
- Host tests: `make -C games/halo2_5849 BUILD=$SP/tnew test-host` builds+runs everything (~3 min);
  single target `$(BUILD)/audio_fx_test` etc. audio_dsp_test/audio_fx_host_test include
  audio_buffer_test.c + audio_host.c with H2_AUDIO_DSP=1.
- Chain pitfall again: `sed s/mp288/mp289/` does NOT rename mpdrive288.py (no "mp288" substring);
  fixed in place with a same-length byte overwrite (bash re-reads later lines of a running script).
- mp289 = mp280 codegen + b048755 host (minus the two FPSCR log brackets, diagnostics only);
  launched 03:35, driver mpdrive289.py, monitor task b1nxpn154. Startup mutes go through the new
  path fine ("attenuationFFF" x8 at the menu). Next expected: level load with zone volumes admitted,
  then either the level renders (send screenshot from scratchpad/mp289/s7-loadNN.png) or a new stop.
- mp289 (b048755): zone volumes + level reverb (effect 9 room -1300, effect 8 decay 1.5 s) admitted;
  next stop at 04:02 `sound entry=0037C6E5 ... submix activation/data/spatial processing is
  unsupported value=036E6000` = SetI3DL2Source(DEFERRED) on an inactive 3D MIXIN bus (submix==1,
  51 buses created at 0x220AC8) from the per-bus environment routine 0x2213xx (return 0x221427),
  immediately followed by SetFilter 0x37B68B (return 0x22147C) with {1,0,0,0x8000,0,0}.
  Fix: commit 994cc41 (pushed) "admit level-start I3DL2 source and filter on inactive 3D buses".
  Original facts: 0x37C6E5->0x37C4D2->0x37C0E9 (9 raw words to voice settings +0x80, dirty |=7F at
  byte 0x7E, deferred returns before 0x37A669); 0x37B68B->0x37B1E6->0x37A616->0x381710 (packs the
  filter descriptor into the voice image). Game helpers: 0x21E270 freq->coef (30..8000 Hz clamps
  0x8000/0), 0x21E2D0 resonance->coef (2^x*32768, max 22.5); globals 0x470084=8000, 0x470088=0.
- mp290 launched 04:09 (chain290.sh / mpdrive290.py; monitor b8p63cv0o): same codegen (mp280) +
  the submix I3DL2/filter admission. Expect ~04:36 for the level load point.
- mp290 (994cc41): LEVEL RENDERED at 04:40:37 (scratchpad/mp290/s7-load68.png, sent to the user:
  flat-shaded Ivory Tower geometry behind the fading lobby overlay), then a guest trap one frame
  later: `guest trap address=0037F42E fn=0037F425` = div by zero in DSound bytes->samples
  ([voice+0x70]+0x14 == 0) reached via IDirectSoundStream::Pause 0x37B822 (jmp 0x37AD9C -> voice
  Pause 0x380074 -> 0x37FCEE restart -> 0x37FA09 -> 0x37F618 -> 0x37F425) running as PASS-THROUGH
  (XV_MENU_VBLANK allows ~50 unhooked DSOUND methods to run natively on host-made objects; see
  "[h2/audio-menu] passthrough DSOUND" lines) on the host's opaque stream record 0x03BF6000.
  Fix: commit 577af59 hooks 0x37B822 (hooks.py AUDIO_HOST_BOUNDARIES (5, sha256 of e975f5ffff)) with a
  state-bit model (stream_pause: 0 resume &~0x64, 1 pause |4 &~0x40, 2 synch |0x40 &~4, 3 |0x20),
  game streams only (flags 0x20000000, no packets), callers 0x21F461(2)/0x21F63D(1)/0x21F8E3(0)/
  0x2AE4D8(0). Any hooks.py change needs a FULL CODEGEN stage (chain280.sh template:
  prepare_boot.py --host-channel --audio-host; ~15 s now thanks to caching, build ~4 min).
- mp291 launched 04:48 (chain291.sh = codegen + build + run; mpdrive291.py; monitor b2gefe240).
  Remaining pass-through risk: other unhooked stream/buffer methods the game calls in-level
  (0x37B7FF/804/809/80E/813 stream setters, 0x37B827, 0x37C668, 0x37C705, 0x37C70A, 0x37AC89,
  0x37ACD4 from game callers) all run natively on host objects; expect more of these.
- mp291 (577af59): level rendered again, then `MCPX APU MMIO address=FE820010 fn=0038157E` = the
  VP frame-counter wait inside SetEG reached via pass-through 0x37B80E (from the sound-source start
  0x21F8A6 -> 0x2AE7A0). Audited the whole stream-method family the game calls (E8 call-site scan of
  the arena code, xbe_tool func on each core): SetVolume 0x37B7FF->0x37B370->0x37A5D4, SetFrequency
  0x37B804->0x37B31E->0x37A5BB, SetLFO 0x37B809->0x37A5F0->0x38144F, SetEG 0x37B80E->0x37A603->0x38157E,
  SetFilter 0x37B813->0x37A616->0x381710, SetMixBinVolumes 0x37B81D->0x37A64C->0x37A4DB, SetOutputBuffer
  0x37C705->0x37C524->0x37BF27->0x37BD54, SetMixBins 0x37C70A->0x37C576->0x37BF7B->0x37BC89 (NULL = table
  0x3858BC {0,0},{1,0}), PauseEx 0x37B827->0x37ADED->0x37F9D0 (ts 0 -> 0x37F51F, no-op unless state&3==3),
  Flush 0x37AC89 (vtable 6) -> 0x37FBFE (returns unless state bit0), GetStatus 0x37ACD4 (vtable 3) ->
  0x37F591 (bit0 = packet list non-empty, 0x80000 = state 0x20), GetVoiceProperties 0x37B83F->0x3818E5
  (DSERR 88780032 unless started), bus SetPosition 0x37C668->0x37C3BF->0x37C031 (3D block +8..+0x10,
  dirty bit 16). Fix: commit 3ae29c8 (storage-only models, hooks.py boundaries with arena-derived sha256).
  TEST PITFALL: a strict stop inside call() longjmps into a dead reject() frame -> garbage assertion
  failures (memory/voice diffs); if a call() "corrupts" state, first suspect a validation failure.
- mp292 launched 05:40 (chain292.sh, mpdrive292.py, monitor bljcs72jg): full codegen with the
  stream-method family. Level point expected ~06:08.
- mp292 (3ae29c8): envelope/output buffer/filter/LFO/volume/mix bins admitted at the menu; stop at
  0x37B804 value -501 -> it is SetPitch (DSBPITCH -4096..4095), fixed in commit 75a6206. mp293 launched
  05:47 (host-only on mp292 codegen; chain293.sh/mpdrive293.py; monitor b63m6on96).
- mp293 (75a6206): stop in the lobby at GetStatus caller 0x2AE4C4 (sound-source stop: status bit
  0x20000 -> Pause(0) else PauseEx(0,0,3)); release paths 0x21EC5D/0x21F2B8/0x21F2F8 GetStatus (bit
  0x10000) then Flush. Vtable call sites can't be found by the E8 scan (call [ecx+0Ch]/[ecx+18h]).
  Fix: commit ae475e7. mp294 launched 05:57 (host-only on mp292 codegen; monitor bdd4t6jr2).
- mp294 (ae475e7) 06:00-06:42: MILESTONE. START GAME -> the game PREPARES the map (cache001 rebuild +
  cyclotron copy into save/cache4/, ~21 min; the cache partition rotates: cache3/4/5 dirs under
  save/, NOT save/n/ - fix the monitors' CACHE path) and then RETURNS TO THE LOBBY ("Waiting for you
  to start the game"); a SECOND A on START GAME (hold ~2 s so a logged poll sees it) starts the
  match: level renders with "Welcome!" and the reticle, emulator keeps running, NO strict stop with
  the full stream-method family. (mp290 auto-started only because pass-through garbage statuses.)
  Screenshot scratchpad/mp294/s9-level.png sent to the user. Drivers must add the second press.
  Input log format: "[h2/input] poll #N buttons=XXXX packet=P" (START=0010, A=1000), logged every
  64 polls only; short taps may not appear. Live-level monitor bl6hm94rx (shots every 30 s).
- OPEN (mp294, 06:46): ~2.5 min after the match started the frame went fully BLACK (renderer back-
  buffer dump menu-frame-03A14000.bin all zero), while the game loop keeps running: clears continue,
  only small UI quads are drawn (prim=7 verts=4, vs 01fb1f00483f29c1), sound sources keep starting
  (pause/envelope/output buffer/mix bins from 0x21F8E3.. every few s, flush from 0x21F2CF) - probably
  ambient loops restarting because GetStatus says "not playing" instantly. No fallbacks, no stop.
  Start/A presses showed no visible change (input log samples only every 64 polls; registration
  unverified). Candidates: player fell out of the map (no world draw from the void), or a game state
  transition. Next: verify input registration with a long hold + poll log, dump the game's player/
  camera state, or add a per-frame world-draw counter to the renderer log.
- RESOLVED (06:50): the black phase (06:46-06:50) was the level's own load-in; afterwards Ivory Tower
  renders TEXTURED (marble floor, dome) and a Start press opened the in-game GAME MENU (Leave Game /
  Settings / Handicap / End Game) -> input works in-level, game fully alive. Screenshot
  scratchpad/mp294/s13-back.png sent. Input packet counter advances on registered presses (13->15).
- 09:40 "Keep going": in-level frame rate was ~1 frame/min because the level's 20 vertex programs +
  8 combiner pairs had no compiled GXM program (software raster). Pipeline: `python3
  tools/h2_menu_shaders.py <boot.log> <ux0 xita-halo2 dir> <outdir>` (reads "[h2/menu-shader] pair#"
  inventory + menu-vp-*.bin dumps -> 68 ps + 43 vs .cg), then `private/compile_shaders.py <outdir> 900`
  (needs NO Vita3K running; ~1 min; 111/111 ok), then copy *.gxp into
  private/vita3k/ux0/data/xita/shaders/ (loader prefers ux0). .cg/.gxp derive from game dumps ->
  never committed. mpdrive295.py adds the second START GAME press (map_prepared(): any
  save/cache*/cache003.map == 46205952 B untouched 30 s). chain295.sh = install + run mp294 build.
- GAME STREAM PACKETS (10:00, commit 4265f50): IDirectSoundStream::GetStatus bit0 is
  XMO accept-input (free packet list at voice+0xB0 non-empty), 0x10000 playing (running & !paused, or
  state 0x8001), 0x20000 paused while running (state 0x444), 0x80000 = state 0x2000 (NOT the deferred
  pause bit; earlier model was wrong -> the game never submitted packets because status was 0).
  Refill 0x2AE866: decoder[0x20] next chunk -> decoder[0xC] fills XMEDIAPACKET on the caller's
  stack -> Process(stream, &pkt, NULL) at 0x2AE897 (ret 0x2AE89A); HRESULT<0 -> decoder[0x14].
  Callback 0x220730(pool index, pPacketContext, status): decoder[0x18](source, ctx, status), and if
  status==0 and !(flags&0xC) refill via 0x2AE820. XMEDIAPACKET status values: pending E_PENDING
  0x8000000A, flushed E_ABORT 0x80004004 (0x37FBFE pushes it). Host model: game_stream_process pushes
  the packet bytes (xk_audio_stream_push reads guest memory directly; game keeps the buffer until the
  callback), *pdwStatus=pending, up to packet_limit 2; DoWork (0x37B844) pops mixer-consumed packets
  and calls back with S_OK + byte count; Flush completes queued packets with E_ABORT synchronously;
  Pause 1/2 stop the voice, 0 resumes (xk_audio_voice_play); SetPitch applies rate*2^(p/4096).
  Non-DSP test builds get stream_process via a small stub; harnesses stub xv_call
  (audio_buffer_test.c default test_no_guest_call; audio_stream_test.c test_game_callback).
- mp295 (ae475e7 + 111 compiled level shaders): the match auto-started after the copy (10:09:26,
  before the driver's second press) -> first-person view with weapon, HUD (score, "7:55 Slayer"),
  reticle, textured walls (magenta = untextured spots); screenshot mp295/s7-load64.png sent. Frame
  rate ~1 frame / 4 s (was ~1/min). Two more programs appeared in-match (ps 5c4904f33bc4565f,
  vs f291897cbd26f6ba) -> regenerated+compiled 100 shaders (level-shaders2) for mp296. Renderer
  profile: software raster only ~15 s of 180 s -> the rest is game logic (-O0 generated code) and
  log spam (commit 962f732 budgets the traces). Next lever: GUEST_OPT=-O1 stage (mp297 build).
- mp296 launched 10:14 (host-only on mp292 codegen: packet playback 4265f50 + budgets 962f732 +
  all shaders; monitor bt71y5267).
- mp296 (4265f50+962f732): the first game packet (title music, 64 KB XBADPCM from 0x82428000) was
  accepted, then backend error -1007: mix_muted_gp_pcm() requires the shared mixer output (the GP
  render's PCM/"movie" lane) to be silent outside movies. Fix commit d3c22dd: host registers game voices
  (h2_audio_backend_stream_voice_active) while packets are queued; the lane admits them and is
  rendered (render_pcm) whenever a registered voice or the movie plays and the FX config is complete.
  mp298 launched 10:26 (host-only, mp292 codegen). mp297 (-O1) still building.
- mp298/mp299/mp300 (d3c22dd, 2169f53, 8a1839e): first title-music packet (64 KB, PCM16 stereo
  stream 0x036F6000 kind 2) plays and completes; fixes: keep voices registered until Flush/Release,
  DoWork tolerates registered drained voices (game_active). mp300 refused a stereo XBADPCM packet:
  XMEDIAPACKET field 5 = pContext (union with hCompletionEvent; stream callback needs it non-NULL to
  refill), field 6 = prtTimestamp -> commit b3de5da. mp301 launched 10:47.
- mp301 (b3de5da) 10:51-11:21: AUDIO MILESTONE. Title/menu/level all run with real stream packets:
  167 submitted / 157 completed in-level across 5+ streams (32760-B stereo XBADPCM, 16384-B PCM),
  0 flushes, no stops; match visible at 11:19:56 (frame mp301/s9-level-audio.png). Frame rate still
  ~1 frame / 15 s in-match at GUEST_OPT=-O0 (game clock 7:55->7:49 across runs). Next: mp302 = the
  -O1 stage (mp297, chain302.sh run-only) to measure the game-logic speedup.
- mp302/mp303 = -O1 guest build (mp297; GUEST_OPT=-O1 in build-args; incremental rebuilds via the
  same args recompile only changed host files). Boot->title 81 s at -O1 vs 112 s at -O0. mp302 hit
  -1007 at START GAME: a race (push marks the voice playing before registration; worker thread
  checks known voices) -> commit b39e634 registers first. mp303 launched 11:26, START GAME 11:30:54.
  Monitors: use `sleep 40` before the pgrep exit check (the emulator takes a moment to appear) and
  never pkill a pattern that appears in the same command line (kills the shell, exit 144).
- PERF ROOT CAUSE (11:50): in-match ~3-4 flips/min regardless of -O0/-O1 (mp303: 3 flips/60 s).
  `top -H` on Vita3K: 8-9 "llvmpipe-N" threads at 10-21% each -> Mesa llvmpipe SOFTWARE OpenGL on
  the headless Xvfb :111 does all GXM rendering on the CPU. Machine has a Radeon RX 7900 XT + Raphael
  iGPU (/dev/dri/renderD128/129, RADV ICD installed). Vulkan backend test (mp304): "Vulkan device:
  AMD Radeon RX 7900 XT (RADV NAVI31)" but "No DRI3 support detected - required for presentation" on
  Xvfb -> "Failed to select proper Vulkan queues" -> reverted to OpenGL (backup
  scratchpad/config.yml.opengl-backup). Real-GPU options need the user: a headless Xorg with the
  amdgpu driver (root/Xwrapper) or running on the desktop display :0 (intrusive). Meanwhile
  XV_MENU_GXM_AB=0 in env.txt (A/B readback every 200 draws was a diagnostic cost; backup
  scratchpad/env.txt.backup). mp305 = mp297 (-O1) with AB=0, launched 11:55.

- PERF INVESTIGATION (2026-09-17 afternoon, stages mp305-mp307). Corrections + findings:
  * `[h2/flip] ... serial=N` is logged every 60 flips (serial < 4 || !((serial+1)%60)); earlier
    "5 flips in 90 s" was a misread: the level runs ~3.3 fps (60 flips per 18.5 s), main menu ~2.2 fps.
  * gdb/eu-stack cannot attach (yama ptrace_scope=1, emulator not a child); Vita3K here is the prebuilt
    /home/birchwoodgod/vita3k/ubuntu/Vita3K (no source tree). SDL_AUDIODRIVER=dummy in the lab.
  * Added a budgeted per-60-flip `[h2/perf]` line (host_channel_runtime.c perf_report at the flip-queue
    log; weak h2_platform_time_us in boot.c; counters from menu_gxm.c h2_menu_gxm_perf, audio_vita.c
    h2_audio_backend_perf, audio_fx.c h2_audio_fx_perf via injected clock; H2_AUDIO_PLATFORM_TEST
    compiles the clock out because audio_vita_test's fake clock advances +50 ms per call under F_FX_SLOW).
  * mp306 numbers (menu/lobby, 60 flips = 47-114 s): channel consumer submit ~4-8% of wall, flip waits
    ~3%, GXM draws ~0.1 ms each, flush_scene ~1.5 ms each -> the host channel is NOT the frame cost.
    Audio worker: every 1024-frame grain takes ~56 ms (max 69 ms) vs 21.3 ms real time -> audio runs at
    ~37% real time, worker holds progress_mutex 98% of wall time, every grain misses its deadline, and
    guest lock calls run at ~700k/s (guest_wait only ms) - site histogram + kernel [wait] dump added in
    mp307 to find who spins and what the main thread waits on (xk_thread.c xk_wait_stats_request/dump).
  * Real-GPU (llvmpipe replacement) is blocked: Vulkan needs DRI3 (Xvfb lacks it), Xorg refuses
    non-console users; needs a user decision (weston/Xwayland, Xwrapper.config, or desktop :0).
  * mp308 (labels fixed): guest lock WAIT = 78 s of a 111 s window (~70%) over only ~8000 calls; hot
    sites = h2_audio_backend_stream_complete (DoWork poll, ~10.9k), cursor (~1.6k), stream_submit,
    stream_voice_active. DSP interpreter: ~23.6k GP instructions per 32-sample frame, ~71 ns each under
    dynarmic (1.7 ms/frame vs 0.67 ms real) -> audio at ~37% real time, every grain misses.
  * Kernel [wait] dump (xk_wait_stats_request from perf_report): t16 always on sem@85482E80 (idle
    worker), t24 (start 3E3E80) waits event@85484FE0 only ~3%, scheduler never idle, t8 (main game
    thread, start 2D0A7A) busy-yields -> the guest is CPU/lock bound, not sleeping.
  * FIX (mp309/mp310, audio_vita.c): the worker renders each grain in 32-frame GP steps and yields
    progress_mutex between steps; membership-changing calls (fx_play/forget/bind/bind_spatial/filter,
    gp_pcm_play) use lock_progress_boundary() and wait for the step loop to finish; reverb_pending is
    cleared per step as soon as the effect state shows command2 consumed (bound: 32 frames = -1010),
    and backend_queue_reverb waits (<=200 ms) for a pending command instead of reporting busy.
    mp309 stopped at "reverb monitor queue busy" (effect 9 then 8 queued back-to-back) before the
    last two parts were added. pgrep -f "run_stage.py mpNNN" matches the checking shell itself (false
    "still running").
  * RESULT mp311 (commit 39b97d7: grain yields + per-frame reverb consumption + lock-free stream poll
    counter): intro/menu/lobby 60-flip windows 7-27 s (were 28-115 s), START GAME -> level visible
    3.25 min (was 17.5), in-match 60 flips per 9.4 s = 6.4 fps (was 3.3), 0 strict stops, 63 tests pass.
    In-match window breakdown (9.4 s): channel submit 4.2 s (gxm render 2.7 = texture acquire/hash
    1.25 + scene flush 0.65 + rest 0.8; non-render consumer 1.6), flip wait 1.5 (includes software
    scanout convert), guest lock wait 1.2 (stream_complete/submit, fixed_commit_ready, cursor), guest
    compute ~2.5. Audio still ~37% real time (DSP interpreter 1.7 ms per 32-sample frame, 23.6k GP
    instructions/frame = 71 ns each under dynarmic; would be far slower on a real Vita A9).
    mp312 adds present_ms/methods/clears/hash_kb counters, a row-wise scanout convert (same output)
    and XV_MENU_GXM_DUMP default 0 (the diagnostic 1.2 MB dump per 200 draws was on by default).
  * BUILD HAZARD: my incremental stages copy the previous stage's build dir; the copied .d files name
    the ORIGINAL stage's object path (mp297), so header changes never rebuilt dependents (mp312 first
    run had a mismatched h2_command_state layout). Use scratchpad/newstage.sh <old> <new>: it retargets
    the .d files (sed /mpNNN/build/ -> /new/build/). Verify with: objects older than the changed header.
  * mp312b level reference (60 flips = 9.77 s): submit 4.2 s = gxm render 2.6 (texture hash 1.28 s for
    3.4 GB hashed = ~130 KB per draw, flush 0.63) + non-render 1.6; methods 2.29M (38k/frame), clears 3/frame;
    present 1.0 s (= the second vblank wait per flip), flip wait 1.53 s total; guest wait 1.33 s.
    Loading screen: 5.15 GB hashed per 60 flips (86 MB/frame), 149k methods/frame in the intro phase.
  * mp313/mp314: flip now queues the frame (h2_platform_present_queue = convert + SetFrameBuf, once per
    flip serial) BEFORE the single vblank wait, then retires (test contract updated: present precedes
    the retire event); NEON 16-byte content hash (identity only; x86 tests use the scalar path);
    perf line gained present_ms/methods/clears/pass_ms/hash_kb/hits/misses.
  * mp315 RESULT (flip reorder + scalar hash): level 1.9 min after START GAME, match 60 flips/8.75 s
    (6.9 fps). Probe proved the flipped buffer is identical before the wait and after retirement; the
    driver had only missed the (now shorter) bright-logo window -> mpdrive template presses Start after
    6 dark shots. NEON hash was SLOWER under dynarmic (tex 2.1 s vs 1.8 s) -> reverted.
    Level window now: pass consumers 3.7 s (menu gxm render 2.8 = hash 1.4 + flush 0.6; other passes
    ~0.9), method parsing ~1.0, flip wait 0.57, guest lock wait 1.1, guest compute ~2.5 of 8.8 s.
  * Grain yields caused an intermittent test failure (audio_vita_test.c:156 uniform-grain check): a
    parameter write landing mid-grain -> fx_route/route_mask/mute/attenuate/effect_write_pair now also
    use lock_progress_boundary(). Committed as the "one display period per flip" commit.
  * NEXT candidates: (1) exact texture re-hash avoidance via guest-store dirty pages + periodic full
    re-hash self-check (86 MB/frame hashed in loading, 55 MB/frame in match); (2) per-pass timers
    (mp316) to see which pass consumer costs; (3) two-lock split for the remaining stream/cursor waits.
  * Per-method pass timers (mp316) were a Heisenberg artifact: each sceKernelGetProcessTimeWide is an
    HLE call, millions of methods per window -> removed; pass_ms now sampled 1/64 (biased, ignore).
    mp317 (no timer overhead) level window 8.89 s, loading 10.4-10.7 s.
  * mp318 = two locks in audio_vita.c: progress_mutex (bookkeeping) + engine_mutex (GP steps; taken by
    effect_read/fixed_commit_ready/queue_reverb/snapshot); gated setters still wait for grain_active
    under progress. Snapshot reports fx_frames_committed (whole grains) because mid-grain fx.frames is
    now observable (broke audio_fx_vita_test's "computed unchanged while held" assert). Test fixture
    supports a third mutex "h2_audio_engine" (bit 32, id 12, resources==63 when open).
  * mp318 RESULT (commit 7334d91 two locks): guest lock wait ~1 ms per window, START GAME -> level 74 s,
    match 60 flips per 7.7 s (7.8 fps), loading screen 7.4-8.0 s per 60 flips. Level window now:
    submit 4.4 s (gxm render 2.7 = texture hash 1.3 + flush 0.7 + rest 0.7; method parsing ~1.7),
    flip wait 0.7, guest compute ~2.6 of 7.7 s.
  * mp319 (in progress): per-page write epochs. recomp/xv_x86rt.h checked pointer stamps
    xv_page_epoch[arena page] = xv_write_epoch on every guest access; xk_mem.c allocates the table in
    xk_mem_bind_arena (XV_CHECK_GUEST_ADDRESS builds only) and provides xv_mark_written(host,bytes);
    the runtime's map_physical stamps, the 7 pass consumers + menu_gxm flush stamp at write time,
    h2_map_physical_read is the texture reader's non-stamping mapper, the epoch advances per flip.
    menu_texture.c: keyed entry with no page stamp >= its verified epoch reuses its hash; every 64th
    clean use re-hashes and a mismatch calls h2_texture_tracking_fault (strict stop, boot.c).
    NOTE: changing xv_x86rt.h recompiles all 128 guest units (~15 min at -j4).
  * mp319 RESULT (write epochs, every access stamped): loading screen 6.4 s/60 flips (was 7.4-8.0),
    match 7.0 s (8.5 fps, was 7.7), 92% of texture acquires unhashed, no tracking fault; but intro
    windows +10-20% (per-access stamp cost). mp320: stamps on guest STORES only - xv_x86rt.h X_W8/16/32/
    64/F32 + X_GW (stamping), X_M*/X_G plain; recompiler emit_function post-pass rewrite_stores()
    turns 'X_M<w>(addr) = ' lvalues into X_W<w>; the pass was applied to a copy of mp292's generated
    code for mp320 (210k stores, 375k loads) since importing xita_recomp.py needs iced-x86 (use a
    standalone copy of the function). Static table xv_page_epoch[65536] (arena 72.8 MB = 17.8k pages).
  * mp320 first run faulted at boot: "texture source changed without a tracked write address=0375A000
    bytes=131072" (a MmAllocateContiguousMemoryEx 128 KB texture). Cause: hand-written host writers
    use memset/memcpy(X_G(...)) (RtlZeroMemory/FillMemory/MoveMemory, xk_mem pool zeroing, xk_net,
    xk_xapi input packets, xd3d, xk_file info structs) - stamped in mp319 only because X_G stamped
    loads. Fix: X_GWN(a, n) (write-span pointer stamping every page) applied to those 21 sites.
    The generated code's own unchecked X_G define is #ifndef XV_CHECK_GUEST_ADDRESS only.
  * mp320 second fault (same texture): xk_phys_alloc/virt_commit zero freshly allocated physical pages
    through raw g_xram pointers (unstamped) -> a freed+reallocated 128 KB contiguous texture buffer
    looked unchanged. Fix: ARENA_WRITTEN(off,n) stamps after those memsets (xk_mem.c); NtReadFile's
    destination uses X_GWN (xk_file.c:209); image-resident sources (arena page >= XRAM_SIZE/4096) are
    never tracked because X_IMG* stores (globals) are unstamped by design.
  * mp321 diagnostics ([h2/texture-fault] line with page stamps + retained source snapshot diff, and an
    XV_WATCH_PHYS=off:len write watch logging [watch-store]/[watch-host]) found: (1) menu_texture.c
    still declared `extern uint32_t *xv_page_epoch` after the table became a static array -> it read
    garbage (stamps 0) and never saw a stamp; (2) the changed bytes (1748 in the first 9 KB of a 128 KB
    contiguous buffer) come from hand-written host stores (audio packet status words etc. via X_M32 =)
    into a buffer a stale texture binding still samples. Fix: array declaration; the rewrite_stores pass
    applied to hand-written recomp/kernel, recomp and games/halo2_5849 sources (410 store sites ->
    X_W*). Rule: every host store into guest memory must go through X_W*/X_GW/X_GWN/x_guest_write.
  * mp321b RESULT (store-only stamps, all host stores stamped): intro 17.8 s/60 flips (= pre-tracking),
    menu 8.0, lobby 11.6, loading 6.0 (was 7.4-8.0), match 6.9 s (8.7 fps, was 7.7), 92% acquires
    unhashed, no fault. XV_TEXTURE_SNAPSHOT=1 (opt-in) retains sources for the fault diff;
    XV_WATCH_PHYS=off:len write watch stays available. Committed as the texture re-hash commit.
  * Stage progression today: mp305 (baseline 3.3 fps) -> mp311 39b97d7 (6.4) -> mp315 021667b (6.9)
    -> mp318 7334d91 (7.8) -> mp321b (8.7 fps). Remaining match window (6.9 s): submit 3.6 (gxm render
    2.0 = flush 0.64 + tex 0.62 + rest 0.7; parsing 1.6), flip wait 0.8, guest compute ~2.5.
  * 2026-09-17 16:00: user wants a Vita CPU/GPU performance proxy and to hand it to Codex too. Brief
    committed as docs/halo2-vita-perf-proxy.md (4f26ba7): ARMv7-Linux harness under qemu-arm/gem5 for
    the hot modules, GPU workload counters + gpu_budget.py, report + device calibration. Back-of-
    envelope: method consumer ~85 ms/frame and GP interpreter ~5 G ARM instr/s on a real Vita ->
    both need design changes. mp322 = GUEST_OPT=-O2 experiment (build only), running.
  * mp322 (GUEST_OPT=-O2, no source change): loading 6.03 s, match 7.0 s per 60 flips = no gain over
    -O1 (guest time is emulator-bound, not compiler-bound) -> keep -O1 (faster builds). Lab left free
    (emulator stopped, Xvfb :111 preserved). Branch head 4f26ba7; env.txt still has XV_PROF=1 and
    XV_WATCH_PHYS=0375A000:20000 (harmless: the watch range only logs; remove for clean runs).
  * 19:10 user: "lets get it playable in vita3k". mp323 (commit: inline strict policy check
    XV_ADDRESS_NEEDS_POLICY = off==trash || addr>=0xFD000000, host CFLAGS -O2): match 6.0 s/60 flips
    (10 fps), loading 4.9 s, guest share -38%. NOTE make does not track CFLAGS: delete non-code_ .o
    to force host recompiles. Next: GPU-resident render targets (lazy download; guest loads caught by
    mapping a dirty target's pages to the trash page so the slow path syncs), then method parsing.
  * mp324 (in progress): GPU-resident colour buffers in menu_gxm.c: end_scene (no Finish/download;
    target.gpu_newer=1 + guard = xv_guard_physical maps the buffer's identity/0x8/0xF pages to the
    trash page), sync_target (Finish + download + unguard) only on demand: runtime map_physical /
    h2_map_physical_read call h2_menu_gxm_host_access(phys,bytes,may_write); the flip syncs just the
    flipped buffer; guest accesses hit the strict slow path -> boot.c calls h2_menu_gxm_guest_touch
    (sync + restore) and the X_G macros reload the translation; open_scene uploads only if
    guest_newer or page stamps >= synced_epoch (both-changed = h2_render_target_fault strict stop);
    kelvin_clear whole-surface clears call h2_menu_gxm_before_cpu_write(whole=1) to drop the GPU
    copy; map_physical_raw accepts guarded pages (h2_menu_gxm_page_guarded); read_physical uses the
    raw mapper. Perf line: ends/uploads/touches replace gxmdraw_ms.
  * mp324 lab result (2026-09-17 19:56): strict stop at boot "[h2/blocked] sound ... original work
    stream accounting value=03B36000": xv_guard_physical trashed g_xpt[page] (identity alias) for the
    colour buffer's physical pages, but the kernel maps allocated virtual pages to OTHER physical
    pages (xk_mem.c map_page; identity is only the default), so virtual page 03B36 (a DirectSound
    stream object) lost its mapping; audio_host.c mapped() then saw the trash page. Fix (mp325):
    xk_mem.c keeps a reverse map (g_vpage_of[phys][2] + multi flag) and a guard list
    {vpage, off}; xv_guard_physical guards only real aliases (0x80000+p, 0xF0000+p, reverse-mapped
    vpages, scan when multi) and restores exactly; xv_guarded_physical(address) gives the physical
    behind a guarded alias (menu_gxm guest_touch uses it); map_page drops a guard when a guarded
    page is remapped; xk_mem_map_alias() is the public mapper (kernel_stack.c uses it);
    audio_host.c mapped() calls h2_menu_gxm_guest_touch on trash pages first. page_guard_test.c
    (host, 64 tests total) covers it. mp325 reached START GAME with 0 strict stops.
  * mp325 menu perf regression (fixed in source for mp326): render_body mapped its own target via
    c->map_physical -> h2_menu_gxm_host_access -> sync_target(open target) on EVERY draw
    (flushes == draws, 36 s/60 flips vs 6.3 s in mp323). New h2_map_physical_target() (raw, no
    landing/stamping) in host_channel_runtime.c; menu_gxm.c uses it for the target lookup.
  * Rendering quality work (goal "as good as Halo CE"), mp325/mp326: menu_texture.c now decodes the
    mip chain (SET_TEXTURE_FORMAT bits 16-19; levels stop at 8-texel width so GXM linear stride ==
    width; want_mips flag; lvl_src/lvl_dst offsets), menu_gxm.c uploads with InitLinear(levels) and
    sets min/mag/mip filters from SET_TEXTURE_FILTER (0x1B14: min bits16-23 1 box 2 tent 3-6 mip,
    mag bits 24-27) and U/V wrap from SET_TEXTURE_ADDRESS (0x1B08: U bits0-3, V bits8-11; 1 wrap
    2 mirror 3/5 clamp 4 border->clamp) on every draw; linear (pitch) images stay clamped.
    [h2/tex] trace lines (first 8 ok / 3 failures per format) and "| tex CC:ok/unsup/toolarge/nomap"
    on the perf line. Cube maps (fmt bit 2; 25 of 100 level fragment programs use samplerCUBE):
    six faces back to back, each the whole chain padded to 128 bytes (xemu NV2A_CUBEMAP_FACE_ALIGNMENT),
    decoded level 0 per face into Morton order and uploaded with sceGxmTextureInitCube (mp326).
    The game asks for trilinear + wrap in the menus (filter=02062000 addr=00010101).
    Level screenshots: driver writes $SP/mp314/s7-loadNN.png (path never updated); `import
    -display :111 -window root file.png` works for ad-hoc shots.
  * DXT block order was WRONG (mp326 fix): menu_texture.c indexed DXT blocks in Morton order, but
    NV2A compressed images are plain row-major blocks (xemu glCompressedTexImage2D's the data
    untouched). Only 8x8 menu images (2x2 blocks, where both orders agree) had ever been decoded;
    the level's 256x256 DXT maps came out scrambled (magenta ceiling with black diamonds).
  * mp326 first run: "[h2/blocked] colour buffer changed in guest memory while its GXM surface held
    newer pixels" was a false positive: sync_target stamped the guest copy at epoch E and set
    synced_epoch=E, so guest_changed() (stamp >= synced) stayed true until the next flip; with the
    per-draw sync gone (raw target mapper) the fault fired at the next scene switch. Fix:
    sync_target bumps xv_write_epoch after its own stamping (also stops the texture cache re-hashing
    a sampled back buffer every draw: hash_kb was ~1 GB/60 flips).
  * Vita3K texture layout (renderer/src/texture/cache.cpp, fetched 2026-09-17): LINEAR stride =
    align(width, 8) texels, mip levels back to back (each level align(w,8)*h texels); CUBE faces
    sequential, all mips of a face before the next face, face start aligned to 2048 bytes when
    mip_count != 0xF and (w>=16 && h>=16 && bpp 16/32) (or w,h>=32 for <=8bpp/compressed); menu_texture
    pads decoded faces accordingly (face_texels).
  * Lab: run_stage.py refuses a reused label ("label mpNNN already used in the lab") - relaunching
    a stage needs a new label (chain326b.sh: run_stage.py mp326 mp326b; emulator log becomes
    native-attempt-mp326b-emulator.log). GXM swizzled order = Morton with y in the EVEN bits
    (Vita3K format.cpp decode_morton2_y = compact(code>>0)), the transpose of NV2A's (x even);
    cube faces are written with morton(y, x).
  * mp326b (20:25): render-target fault again in the loading phase, via the UPLOAD path this time:
    a whole-surface clear stamps the buffer at epoch E, open_scene uploads and set synced=E, the
    target is closed and reopened before the next flip -> stamps E >= synced E with gpu_newer.
    Rule: whenever synced_epoch is set (sync_target AND open_scene upload) bump xv_write_epoch
    first. Also closed a real hole: x_guest_checked_span_write (X_GWN) checked the policy only
    on the first page of a span, so a host memset/memcpy/NtReadFile starting outside a guarded
    buffer could overwrite guarded pages without landing them; it now runs the policy on every
    page of the span (header change => full stage rebuild, ~15 min). mp326c = that build.
  * mp326c (20:50, level reached, 0 strict stops, all formats load, 9 cube maps): the level finally
    shows real textures (grey concrete wall with lighting, ceiling detail) - the yellow blocks and
    magenta ceiling were the DXT block order + point filtering + missing cube maps. Level perf:
    9.3 s / 60 flips (submit 3.9, tex 1.4, flush 1.2, 240 landings, 420 scene ends).
    Remaining defect: huge striped shards across the view (persist for minutes, not the weapon
    animation). Cause: end_scene/open_scene reset the vertex/index rings without a GPU wait
    (mp323 had a Finish per scene end) -> the next scene overwrote geometry still being drawn.
    Fix (mp326d): rings recycled only right after sceGxmFinish (flip landing, texture-pool reset,
    ring-full drain), VERTEX_RING 12 MB / INDEX_RING 2 MB, "ringwaits=" on the perf line.
    Screenshots: scratchpad/mp326/level-326c.png, level-326c-2.png (7:39 and 7:20 on the clock).
  * mp326d (21:01): level with 0 strict stops, shards gone (ring recycle fix), weapon renders as
    a silhouette but SOLID BLACK; wall very bright. Committed as f87635b (pushed). The weapon is
    drawn into offscreen 02C1B000 (ps 0274828ec4e270d4 t0*t1*c0) AND into the back buffer with
    ps e412c1e167915991 (t1:CUBEMAP ambient cube 4x4 multiplies the diffuse: stage 4 v0=v0*t1).
    All cube maps seen are 4x4 A8R8G8B8/X8R8G8B8 (Halo 2 ambient light probes). mp327 =
    diagnostics run: env.txt XV_GXM_TRACE=1 (mean colour of every landed buffer) and
    XV_TEX_TRACE=100000 (every load with draw serial + mean colour); "[h2/menu-gxm] texture init
    failed" logs InitCube/InitLinear errors. env.txt backup: env.txt.bak-before-tex-trace (remove
    the two knobs after the run).
  * ROOT CAUSE of the black weapon / odd lighting (mp327b draw trace, 21:23): menu_combiner_decode
    called menu_combiner_prepare() BEFORE setting cb->stages, so prepare's per-stage scan ran 0
    times: tex_used = unit 0 + final-combiner inputs only, and c0f/c1f (software path constants)
    stayed zero. Menu programs reference t0 in the final combiner so they never showed it; level
    programs sample bump/lightmap/cube in stages -> units 1-3 never bound (samplers read stale
    textures). Fix: set stages/mux_msb before prepare (menu_combiner_test.c
    test_decode_scans_every_stage). XV_DRAW_TRACE=<ps hash prefix> (menu_gxm.c) logs a draw's
    units/means/constants - the tool that found it. mp327c = build with the fix.
  * mp327c (21:32-21:50, combiner fix): the level now looks like Halo 2 - dark lit concrete wall,
    textured/shaded Battle Rifle, HUD (scratchpad/mp327/level-327c.png). Driver never reports
    LEVEL VISIBLE any more: its brightness heuristic (mean > ~50?) was tuned to the old over-bright
    rendering; use "START GAME (second, map prepared)" + ~40 s instead. Remaining artifact: a
    black/white hatched patch over the arms/lower weapon (ps 5c4904f33bc4565f = t0.a lerp of
    t1/t2, traced in mp327d). Perf regressed to 16.5 s/60 flips: tex_ms 10 s, 6780 texture
    misses/60 flips - the 64-entry decoded cache / 96-entry GXM pool thrash with 4 units per draw
    -> mp327d: TEX_CACHE_ENTRIES 512, budget 128 MB, MAX_TEX 512, TEX_POOL 96 MB (main RAM via
    sceKernelAllocMemBlock USER_RW_UNCACHE + sceGxmMapMemory; too much for hardware - revisit).
    Vita3K logs ~340K "Unhandled SIGSEGV" in the level (its mprotect surface/texture tracking
    firing on our CPU reads/writes of GXM memory) - noise, but each is a fault.
  * mp327d (22:00): level at a new spawn renders coherently (pillars, beams, sky, lit/unlit sides,
    HUD; scratchpad/mp327/level-327d.png), 0 strict stops. Arms pass (ps 5c4904f33bc4565f into
    02C1B000) binds three 8x8 textures the GAME chose (DXT5 mask mean alpha 0x14 + two DXT1 grey
    swatches at 018FA400/580/480 - the UI/default bitmap region), so the hatched patch over the
    arms is how that pass samples those; 02C1B000 is then read by the weapon shader as t0 (screen
    space lighting buffer). Left documented, not fixed. Host box: swap full, 39 GB available;
    the harness killed a background wait for "low memory" once - keep background waits short.
  * mp327d lesson: boot.c sets _newlib_heap_size_user = 48 MB; the decoded texture cache
    (malloc) must fit under it. A 128 MB budget exhausted the heap -> malloc NULL -> textures
    silently unbound AND load_gxp "bad program" (its malloc failed) -> "no compiled program"
    -> draws fell to the software renderer (degenerate, 150 s/60 flips, pass_ms 138 s).
    Now: heap 128 MB, decoded budget 64 MB, "[h2/tex] out of memory" log; pool 96 MB is a
    separate memblock (main RAM). Arena is a memblock too ("halo2_guest"). mp327e = that build.
    Long-term (hardware memory): upload DXT natively (SCE_GXM_TEXTURE_FORMAT_UBC1/2/3) instead of
    decoded RGBA8 - 4-8x smaller, no CPU decode; need GXM's block layout for UBC (check Vita3K).
  * mp327e (22:20): Zanzibar renders recognisably (palms, pylons, sandstone, walkway, shaded BR,
    HUD) at 9.3 s/60 flips, 0 strict stops, 0 OOM, 0 missing programs, 0 software draws
    (scratchpad/mp327/level-327e.png, -2.png). Committed on top of f87635b (see git log).
    Still open: tex misses 1050/60 flips (decoded budget 64 MB < working set; native UBC upload
    is the real fix), hash_kb 2.1 GB/60 flips (screen textures re-hashed each frame), hatched
    patch on the arms pass, driver LEVEL VISIBLE heuristic, hardware memory budget. env.txt still
    has XV_GXM_TRACE=1 and XV_DRAW_TRACE=5c4904f33bc4565f (backup env.txt.bak-before-tex-trace).
  * mp328 (22:3x): native DXT upload - menu_texture.c GPU path (want_mips) leaves DXT as blocks
    reordered into GXM swizzled block order (morton(by, bx): row in even bits), levels back to
    back, compressed cube faces 2048-aligned from 32x32 (Vita3K cond1); menu_gxm.c uploads with
    sceGxmTextureInitSwizzled(UBC1/2/3_ABGR, levels) / InitCube. out_native (1/3/5) in the API;
    the software path (want_mips=0) still decodes to texels. Arms artifact: the mask is an 8x8
    DXT5 (018FA400, wrap + trilinear, mean alpha 0x14) with two 8x8 swatches - probably the
    game's low mips/placeholders for armour bitmaps (shared.map IS read, 65 MB) - unresolved.
  * mp328 (22:33, native UBC): level renders, 0 strict/oom/missing/sw, 7.7 s/60 flips (misses
    122, tex_ms 1.5 s) - but native textures come out with RED and BLUE exchanged under Vita3K
    (Mesa llvmpipe built-in S3TC): weapon 40,37,31 -> 33,40,40; green-grey walls barely change.
    Real games with UBC1_ABGR look right in Vita3K, so the standard order stays the code default:
    XV_UBC_SWAP=1 (env.txt, lab only) exchanges the RGB565 endpoint fields per block
    (menu_dxt_swap_rb; DXT1 mode preserved by endpoint swap + index remap). Hardware: try
    without the knob first. Also fixed: the software DXT3/5 decoder honoured c0>c1 (3-colour
    mode with black) - the spec and the GPU always use 4-colour for DXT3/5 colour blocks.
    mp328b = that build with the knob on.
  * GROUND TRUTH (scratchpad/dxtprobe.c, EGL surfaceless on the host's llvmpipe = Vita3K's GL):
    DXT1 c0<c1 -> three-colour (idx3 = transparent black), same as NV2A; DXT5 c0<c1 -> FOUR-colour
    (idx2/3 interpolated), channel order standard RGBA (no swap). The NV2A decodes DXT3/5 colour
    with the DXT1 rule (idx3 = black) and Halo 2's assets rely on it (HUD digits became white
    blocks when the software decoder used the four-colour rule -> reverted). So the native-DXT5
    weapon tint = four-colour decoding of three-colour blocks, NOT a channel swap (XV_UBC_SWAP
    was a wrong guess; keep 0). Policy (mp328e): native DXT3/5 only if every c0<=c1 block uses
    indices 0/1 (rewritten exactly: endpoints swapped + index bit flipped); otherwise the image is
    decoded on the CPU (menu_texture_mode3_decoded, "mode3dec=" on the perf tex stats). GXM
    hardware presumably matches Mesa here (D3D rule) - keep the policy on device.
  * mp328e (23:12, native UBC + mode3 policy): Zanzibar with every texture kind native; rifle warm
    with orange accents (matches the decoded reference), wall/HUD right, 0 strict stops, mode3dec=0
    (the 7+8 three-colour images all rewrote exactly), 8.7 s/60 flips (scratchpad/mp328/
    level-328f.png). Committed on top of b48aeb7 (git log). Missing program from mp328b
    (ps c022c0b926518467 / vs a93c82d068e7e8df): generated with tools/h2_menu_shaders.py
    <inventory.log> <ux0/data/xita-halo2 vp dumps> <outdir> (pair-named), compiled with
    private/compile_shaders.py <dir> (lab emulator must be free), copied into level-shaders2 +
    the ux0 shaders dir. Lab env.txt now: XV_DRAW_TRACE=e412c1e167915991, XV_UBC_NATIVE=7.
  * mp328f (23:24, arms trace): arms pass (vs 05020ddafff788cd / ps 5c4904f33bc4565f into
    02C1B000): v0 float3 position, v1 float2 UV (~3.8,0.7), v10 unused -> NV2A default, oT0-3 =
    v1*c[172] then *c[18..20] (per-vertex, NOT screen space), textures 8x8 DXT5 mask (alpha mean
    0x1D) + two 8x8 DXT1 swatches, wrap+trilinear -> the hatch is the game's own mask pass with
    faithful inputs; needs an Xbox reference to judge. XV_GXM_HALFPIXEL knob exists (default 0).
    Rifle colour varies run to run with pose/lighting (traced inputs identical). Commits on the
    branch: f87635b, b48aeb7, 1809ca0, + diagnostics commit (git log). Lab env.txt restored to
    the base config; level-shaders2 has 101 gxp (the c022c0b926518467 pair added).
    GOAL STATUS: visual parity with CE's recorded a10 reference reached for level/weapon/HUD (arms
    pattern open); performance parity NOT measurable here (CE off limits, H2 never on hardware).
  * mp328g (23:3x): render-to-texture - a unit whose source is a GPU-resident target sampled as
    a 640x480 linear ARGB8 screen image (fmt code 0x12, pitch 2560, rect 640x480, texture offset
    == colour offset) binds the target's own surface (target.as_tex, InitLinear over t->mem):
    end_scene if it is the open target, upload from guest first if guest_changed (Finish +
    memcpy + epoch bump), sampler state from the registers, never for the draw's own target.
    Removes the per-frame landing/copy/hash of ~25 screen images. "rtt=" on the perf line
    (gxm[7] = fallbacks + rtt*1000). set_sampler_state() factored out of get_texture.
  * mp328g/h (23:37-23:41): render-to-texture works (rtt=900/60 flips in the level, flush_ms
    0.6->0.25 s) but wall time unchanged (8.9 s) because the hashing (1.28 GB/60 flips) was NOT
    the screen images: the perf-line histogram ("hash@<offset>(u<unit>,<code>)=<MB>") shows the
    three 512x512 P8 bump maps (00905000/00865000/00641000, unit 2, code 0B) re-hashed on every
    draw - P8 was excluded from page-epoch tracking because of its separate palette. Fix (mp328i):
    P8 tracked too; the palette (<=1 KB) is hashed every use and compared (tex_entry.pal_hash).
  * mp328i menu line (23:46, P8 tracked): hash_kb 337->198 MB, tex_ms 1232->660 ms per 60 flips,
    no texture-fault. Remaining hashers: 160x120 bloom buffers 02B1B000/02B31800 (written by the
    software bloom pass each frame, bound ~15x) and 038E8000 via the software downsample path.
    Next (built, mp328j): acquire() bumps xv_write_epoch after a verification hash so same-frame
    rebinds of a freshly written buffer read as clean.
  * mp328i level (23:50): 8.57 s/60 flips, hash_kb 205 MB (was 1.28 GB), tex_ms 1.2 s, rtt=900,
    flush 0.2 s, 0 strict stops. Remaining: pass_ms 2.9 s (software bloom passes: downsample of
    the back buffer to 160x120 + blur/composite on the CPU) and ~4.8 s guest logic per 60 flips.
    Committed as e3e37f8 (pushed). mp328j = + verify-epoch bump (built 23:48:10), running.
  * mp328j (23:58, verify-epoch bump, included in e3e37f8): 8.47 s/60 flips, hash_kb 151 MB,
    tex_ms 1.2 s, 0 strict stops; third spawn (ribbed metal corridor, grating) renders right.
    Working tree clean at e3e37f8; lab stopped, Xvfb :111 up, env.txt base config, 101 gxp
    in level-shaders2. Session end state 2026-09-17 ~00:00: rendering at CE-reference quality
    for level/weapon/HUD; open = arms 8x8 mask pattern (needs Xbox reference), performance
    parity unmeasurable here (CE off limits, no H2 hardware run), software bloom passes (2.9 s)
    and guest logic (4.7 s) dominate the 141 ms/flip.
