---
name: xboxvita-stage4-recompiler
description: Stage 4 status - recompiled Halo runs on Vita (menu, campaign a10 renders lightmapped); kernel/D3D HLE facts, combiner pipeline, debug knobs, open issues
metadata: 
  node_type: memory
  type: project
  originSessionId: 7eb570ec-3881-4274-bfc7-669ab7be6783
  modified: 2026-09-01T18:33:15.530Z
---

Stage 4 of [[xboxvita-pipeline-status]] (as of 2026-09-01): the recompiled Halo BOOTS on the host
(recomp/host/harness) through XAPI startup, main(), D3D init, ui.map load, into the Bink intro movie.

**Toolchain**: `xbe_recomp.py` (iced-x86 via scratchpad venv `…/scratchpad/venv/bin/python`; plain
python3 lacks iced) lifts 8,101 functions / 921k instructions (99.99%) into recomp/code_*.c (32 files,
~63 MB C; host -O0 build ~7 s with build.sh; ARM ~10 s/file -O1, ~5 MB Thumb total). Runtime:
recomp/xv_x86rt.{h,c} (lazy flags verified vs native x86 pushf on 200k vectors - PASS), MMX for Bink,
x87 double stack. Kernel translator: recomp/kernel/ (xk_os host backend = POSIX+ucontext fibers;
xk_mem, xk_rtl, xk_file, xk_thread, xk_xapi, xd3d null-renderer D3D8 object model).
Symbols: XbSymbolDatabase (github Cxbx-Reloaded/XbSymbolDatabase, built CLI at ~/XbSymbolDatabase/build)
-> halo_symbols.json (359 syms). xbox_kernel_exports.py has the full ordinal table + KERNEL_ARGC.

**Key architectural finding**: the old "0x8xxxxxxx aliases collapse into one 64 MB block" assumption is
WRONG. Halo demands PHYSICAL memory at fixed addresses (MmAllocateContiguousMemoryEx tag cache
0x1600000 B at exactly phys 0x3A6000; maps carry absolute 0x803A6000 pointers) while the XBE image owns
the same numbers as VIRTUAL addresses. Guest memory is now a page table: X_G(a) = g_xram +
g_xpt[a>>12] + (a&0xFFF); arena = 64 MB phys + image copy + trash page; 0x80000000/0xF0000000 alias
phys 1:1; NtAllocateVirtualMemory commits private top-down phys pages (bitmap g_virt_committed - the
"identity page looks committed" bug clobbered a thread stack via a 1.6 MB file read and cost hours).

**Xbox boot semantics learned**: entry = XAPI mainCRTStartup -> CreateThread(mainXapiStartup) ->
XapiInitProcess -> _rtinit/_cinit -> main(0,0,0); lift ALL of XAPILIB (HLE only D3D8/DSOUND/XNETS +
XInput/XGetLaunchInfo/etc device entry points). TLS: StackBase[0] = TLS block, KTHREAD+0x28 = TlsData.
Kernel data exports (KeTickCount etc.) = guest vars patched into thunk slots; function thunks =
0xFE00nnnn magic dispatched by ordinal (xv_call hook). WriteFileEx/ReadFileEx pass ApcRoutine =
kernel export NtUserIoApcDispatcher (ordinal 232) with ApcContext = completion routine; deliver user
APCs on alertable waits (SleepEx 5s fallback otherwise). ObReferenceObjectByHandle must materialize a
real guest dispatcher header or KeSetEvent on the returned pointer signals a phantom object.
Halo boot: deletes z:\cacheNNN.map, formats cache000-005 (~290MB+), decompresses ui.map into
cache002.map, loads tags to 0x803A6000, D3DResource_Register on 0x803BDxxx resources, creates all 67
vertex shaders (decls 0x1E1388..0x1E144C match Stage 2), plays d:\bink\intro.bik (thread at 0x1B9C50).

**Debug tools**: XV_TRACE=<frame> env + --trace-calls regen = per-call-site kernel/HLE tracing;
yield-storm + scheduler-idle thread dumps in xk_thread.c; XV_LENIENT=1 skips unknown indirect targets.
Regenerate: `venv python xbe_recomp.py haloce/default.xbe --manifest game_manifest.json --symbols
halo_symbols.json -o recomp --files 32 --trace-calls`. Host: recomp/host/build.sh then
`./host/harness halo_image.bin ../haloce host/save` (cwd recomp). Progress artifact:
https://claude.ai/code/artifact/b94333be-91c7-4e38-9c09-42c1b8935465

**Next**: menu draws on host (after Bink intro), then Vita/Vita3K integration: xk_os_vita.c (sceIo +
xk_sched fibers), bridge xd3d_r_* hooks to xv_d3d.c/GXM, Makefile recomp targets, shader cache for the
runtime-built pixel combiners. Shell gotcha: Bash cwd persists across calls - always cd first.

UPDATE (later 2026-09-01): recompiled Halo reaches the MAIN MENU render loop on host: steady 60 Hz
Present with 86 draws + 1 clear/frame, all via immediate mode (D3DDevice_Begin/SetVertexData/End -
Halo's UI rasterizer; no Draw*Vertices in menu). Fixes that got there: (1) virt_commit bug (identity
page owned by tag cache treated as committed -> 1.6MB read clobbered t12 stack; now g_virt_committed
bitmap, top-down private pages); (2) ObReferenceObjectByHandle materializes real guest dispatcher
headers (KeSetEvent on returned pointer was signaling phantom objects); (3) X_PREEMPT() emitted on
backward branches (cooperative fibers + guest spin loops = starvation; budget 200k back-edges then
xk_yield); (4) Bink movie skipped via --hle-addr 0x2CAE0:Movie_Play:1 (custom conv: arg in eax + 1
stack arg, ret 4) because Bink audio pacing calls LIFTED DSOUND internal 0x193D1F (only the 56
DB-known DSOUND symbols are HLE'd - internals poke MCPX hw; movies need that solved later);
(5) 60 Hz vblank kernel thread (xk_thread_create_host) invoking the SetVerticalBlankCallback guest cb -
Halo's frame limiter counts vblanks at [1F8C80] and deadlocks if vblank only fires on Present.
DSound play cursor now advances in real time (ds_cursor 48kHz stereo). ARM smoke build: all 42 objs
compile, 155 s, ~23 MB text. Makefile has recomp-lib target; kernel/xk_os_vita.c written (sceIo+
fibers+sceCtrl). Next: GXM/host renderer for the immediate-mode UI path -> first pixels.

CRITICAL LIFTER BUG FOUND+FIXED (fxch/fcom operand selection): for two-operand x87 forms
(`fxch st,st(i)`, `fcomi st,st(i)`) iced reports op0=ST0 - taking operand 0 made every fxch a NO-OP
and fcomi compare st0 with itself. Fix: use the LAST operand (sti(op_count-1)). This scrambled all
rotated/multi-term x87 math (menu vertex arrays failed the parallelogram test). After the fix the
menu renders correctly composed (smoke banner + central panel) in the host soft renderer
(recomp/host/softgfx.c: runs the real UI VS program decl 0x1E13EC: pos=dph rows vsc[28..31],
uv=((c34.x*v0+v4*c34.y)*c37+c35.zw)*c32, color=r9; UI = QUADLIST via Begin/SetVertexData2f/End,
vertex struct stride 20: x,y,u,v,color). CRT fmod (wrapper 0x180ADA, len-prefixed name table
0x25F628) is HLE'd natively: st1 mod st0, net pop 1 (xv_hle_crt_fmod). Current regen command adds:
--hle-addr 0x2CAE0:Movie_Play:1 0x180ADA:crt_fmod:0. Still missing on host picture: glyph text
(check skipped decls), combiner blue tint (ps 2FCE60 ignored). Note: ALWAYS cd to the project root
first in Bash - cwd persists and repeatedly caused misapplied patches.

GXM UI BRIDGE (2026-09-01): xv_ui_gxm.c/.h + shaders/xv_ui.frag.cg = hardware renderer for the
recompiled immediate-mode UI path. Overrides xd3d.c's weak xd3d_r_clear/xd3d_r_im_end/xd3d_r_present
with real sceGxm. Design forced by the runtime's threading model: the render pump (core 1) owns the
GXM scene, the game fiber only RECORDS - so the bridge records UI batches into a GPU-mapped vertex
ring on the fiber (double-buffered by frame parity, published via atomic index) and the pump replays
them inside its BeginScene/EndScene via xv_ui_gxm_replay(ctx), which main.c now calls right after
xv_d3d_render. Zero-copy: ui_vtx {float2 pos(v0), float2 v4, D3DCOLOR v9} stride 20 packed straight
into sceGxmMapMemory'd USER_RW; runs Halo's OWN compiled UI shader (decl 0x1E13EC = halo_vs_03.gxp,
found by scanning xv_halo_vs[]) with c[] bound from xd3d_state.vsc[c_base+96..] via
xv_vshader_begin/set_constants; tint = fragment uniform xv_tint (xv_ui.frag.cg = tex*color0*tint,
alpha blend baked in fshader load). Quads drawn as SCE_GXM_PRIMITIVE_TRIANGLES via a static U16 index
buffer (0,1,2,0,2,3 per quad) - GXM has no per-quad fan. Texture control words built once over guest
pixels (xbox_fmt_to_gxm mirrors xv_d3d.c) and cached; skip sceGxmSetFragmentTexture when data ptr
unchanged. Clear = fullscreen clip quad via xv_vs_clear + xv_color.frag (clr_vtx stride 16). COMPILES
clean against vitasdk GXM headers (one enum fix: X1U5U5U5_1RGB); Makefile SRCS updated; xv_ui.frag.cg
auto-picked by SHADER_CG wildcard. REMAINING to see it on Vita3K/hardware: (1) compile xv_ui.frag.gxp
on-device (SceShaccCg) like the other shaders; (2) link librecomp.a into the app and point the guest
fiber at the recompiled XBE entry instead of main.c's mock game; (3) capture the combiner tint into
xd3d_state and pass it in xd3d_r_im_end (currently white). DXT swizzle/block-reorder still TODO for
compressed UI textures.

APP ASSEMBLY - RUNS ON VITA3K (2026-09-01): the recompiled Halo menu renders end-to-end on the
emulated Vita (Vita3K, Vulkan, 960x544, ~28 FPS, title XVIT00001). Build: `make RECOMP=1` (variable
in Makefile) compiles the runtime with -DXV_RUN_RECOMP, swaps xv_d3d.c/xv_scene.c for
xv_boot.c+xv_ui_gxm.c, and links librecomp.a (make RECOMP=1 build/recomp/librecomp.a, ~7 min for the
32 code_*.c; incremental after). ELF 23.8MB text, VPK 10.7MB. xv_boot.c owns g_xram on Vita: one
sceGxmMapMemory'd USER_RW arena (=xk_mem_arena_size ~67MB) so page-table X_G(0x80000000|data) is
zero-copy GPU-visible; runs the recomp scheduler (xk_run_until_idle) on a 2MB-stack core-0 thread.
main.c gated under XV_RUN_RECOMP: skips xram_init + mock game + pump; xv_present renders synchronously
(record->flip->replay->flip, one thread, no cross-thread fence). Bridge uses X_G not xv_guest_ptr;
falls back to xv_texmod.frag.gxp when xv_ui.frag.gxp absent (tint white => identical). Data on ux0:
data/xboxvita/halo_image.bin + haloce/maps/. Bugs fixed getting there: (1) WEAK-SYMBOL-VS-ARCHIVE
TRAP - making main.c's xk_init/xk_yield weak let main.o satisfy the symbol so the linker never pulled
the kernel's STRONG xk_init from librecomp.a -> thunks never rewritten -> TRAP 0x80000018
(ExQueryNonVolatileSetting raw thunk). Fix: RENAMED main.c's scheduler xk_init->xks_init,
xk_yield->xks_yield (no collision). (2) CS DEADLOCK - mainCRTStartup (thread 4) takes critical
sections then exits; RtlEnterCriticalSection busy-yielded forever (7.5M yields/3s). Fix: cs_owner_dead()
steals a CS whose owner KTHREAD has state==3. Vita3K env: binary ~/vita3k/ubuntu/Vita3K (a FILE),
ux0 ~/.local/share/Vita3K/Vita3K/ux0, run DISPLAY=:0 Vita3K -f -w -r XVIT00001. Remaining: UI atlas
texture shows as noise/checkerboard (swizzle decode - GXM swizzled-format path needs the same twiddle
the host softgfx did; likely DXT block-reorder or wrong swizzled format); compile xv_ui.frag.gxp for
real tint; then real hardware. _newlib_heap_size_user=48MB for g_xpt(4MB)+fibers+objects.

RENDER POLISH (2026-09-01 cont'd): UI textures were noise/checkerboard because GXM hardware twiddle !=
NV2A swizzle for Xbox's atlas formats. Fix in xv_ui_gxm.c: CPU-decode each guest texture ONCE into a
24MB GPU-mapped linear RGBA8 pool (ui_decode: Morton de-swizzle + DXT1/3/5 + A4R4G4B4/565/1555 + linear
32/16-bit, ported from host softgfx.c), bind sceGxmTextureInitLinear(U8U8U8U8_ABGR). Cached by
(data,fmtword). Menu binds exactly 2 textures: DXT3(0x0E) + A4R4G4B4(0x04), both 128x128 -> now smooth
smoke. Added Y-flip viewport in replay (sceGxmSetViewport 480,480,272,272,0.5,0.5) to match Xbox D3D
orientation -> central menu PANEL (box with row dividers) now visible/centered. MENU PROGRESSION
BLOCKER: on Vita the front-end is stuck BEFORE input polling - it creates 26 default playlists
(save/cache/saved/playlists/default_playlist/NN/blam.lst) + hdmu.map, renders the loading/menu-frame
(67 Begin/End quads, 2 tex, pixel shader 002FCE60 bound) steadily, but NEVER calls XInputGetState (host
DID reach XInput). So interactive menu + item text not yet shown. Remaining for "full menu": (1) advance
the front-end state machine past attract/loading to input polling (likely the Movie_Play stub or an
init step diverging from host - needs tracing which guest loop it sits in); (2) text glyphs render
through Halo's register-combiner pixel shaders (SetPixelShaderProgram 002FCE60) + font atlas, which the
bridge ignores (fixed texmod fragment) - needs the runtime combiner->GXM fragment path. Auto-Start/A
pulse added to xk_os_vita.c pad poll (polls>180) but game isn't polling input so it has no effect yet.

FRONT-END / TEXT INVESTIGATION (2026-09-01 cont'd): Established the recompiled front-end IS
interactive and running normally - main loop f_00056960(main) -> f_000BD420(update) -> f_000BB060
(frame limiter, reads vblank counter [1F8C80]). It polls input EVERY frame (XGetDeviceChanges x2 +
XInputGetState(handle 0x00777701) - handle/state return correctly), no unimplemented calls, no traps,
file I/O done. So it's NOT stuck - it sits on a menu-frame state rendering steadily (~86 immediate-mode
quads at frame 60; other frames draw ~19 DrawVertices backdrops). TEXT MECHANISM SOLVED: menu/UI text
is a FONT ATLAS (A4R4G4B4 128x128 @ guest 0x01EF8000, handle 03D06C80) with glyph coverage in ALPHA,
RGB=black. texmod (tex.rgb*color0) shows nothing. Fix (in xv_ui_gxm.c ui_texture_for AND host
softgfx.c): detect alpha-coverage (anz>16 && rgbnz*20<anz) and white-fill RGB -> texmod renders
vertex-coloured text with tex.a blend, no new GXP needed. Host validated: menu PANEL + row separators
now visible (recomp/host/menu_frame.png). BUT menu-item text still not visible in this state - the
font atlas has only ~785/16384 alpha texels (sparse; not the full menu character set), so the fully
interactive main-menu screen with CAMPAIGN/etc text is NOT the current state, OR those glyphs draw via
a path we drop. TWO REMAINING FRONTIERS for the full menu (both substantial, not quick fixes):
(1) THE DrawVertices/DrawIndexedVertices PATH IS A NO-OP in the bridge (xd3d_r_draw unimplemented) -
the full-screen animated backdrops (320x240 fmt 12/3F textures) and possibly menu geometry draw via
vertex buffers + SetVertexShader(handle), NOT immediate mode, so ~half the scene is missing on Vita.
Implementing it = bind the recompiled halo VS matched by the CreateVertexShader hash (xv_d3d.c already
does this for the mock path; port that logic), set stream from guest VB, c[], texture, sceGxmDraw.
(2) whether the interactive main-menu text state is actually reached, or the front-end needs a
transition we haven't triggered. GXM Y-orientation on Vita is flipped vs host (host softgfx flips Y in
ui_vs; GXM viewport sign differs) - sceGxmSetViewport(480,480,272,272,0.5,0.5) tried, still smoke-low;
needs the correct yScale sign. RECOMMENDATION: the DrawVertices path is the highest-value next chunk -
it renders the missing backdrops and completes the D3D draw surface.

MAIN MENU SOLVED ON HOST (2026-09-01 late): the "smoke everywhere" picture was a TEXTURE-STAGE bug, not
a state-machine problem. Halo's widget combiner (psdef hashes DB6B3636/E8012696, 7 stages) reads the
bitmap from TEXTURE STAGE 2 (stages 0/1 = smoke 128x128 DXT3 @0x47E034 used only as a <=5% glow term
c0*smoke); rgb = t2.rgb*v0.rgb, a = t2.a. Both softgfx and the GXM bridge always sampled stage 0 with
oT0 math. Fix: ps_image_stage(psdef) = highest tN register read in AlphaInputs@0x00/RGBInputs@0x88 for
stages < CombinerCount(0xD4)&0xF; per-VS texcoord math keyed on the VS function FNV at guest
(vs_handle&~1)+12: 1DAF0284 (halo_vs_03 widgets: oT2 = v4 at menu frame, oT0/oT1 per c32..c38 formulas),
4469E1F8 (halo_vs_04 text: oT0 = v4*c[32], font 0x3D06C80 at stage 0), BB2F446B (halo_vs_38: clip-space
pos, oTn = dph(v4,c[15+2n]),dph(v4,c[16+2n]), c_base -81). MENU LABELS ARE BITMAPS (DeLa widget
background_bitmap @0x44; 256x64 DXT3 @0x47E514/ECD4/FCD4/FD34, logo 1024x256 DXT5 @0x47E4F4, white 4x4
DXT1 @0x47E4D4); widget text_color alpha 0 (@0x10C) is INTENTIONAL, so "missing text" was never a bug.
host/softgfx_0400.png = correct Halo main menu. GXM port: shaders/xv_ui_t0..t3.frag.cg (tex2D(tex0,
TEXCOORDn)*color0; xv_ui.frag.cg deleted), xv_ui_gxm.c has ui_prog[4] (one VS + up to 4 stage-linked
fragment programs each, since GXM fragment programs link against a specific VS), batch carries prog+stage.
Layouts: D3DPIXELSHADERDEF AlphaInputs 0x00, FinalABCD 0x20, FinalEFG 0x24, C0[8] 0x28, C1[8] 0x48,
AlphaOutputs 0x68, RGBInputs 0x88, CompareMode 0xA8, FinalC0/C1 0xAC/B0, RGBOutputs 0xB4, CombinerCount
0xD4, TextureModes 0xD8, total 0xF0; input byte low nibble = reg (8..B=t0..t3, 4=v0, C/D=r0/r1, 1/2=c0/c1).
bitm tag: bitmap_data block @0x60, 48-B entries (w@4,h@6,fmt@0xC: 14=DXT1,15=DXT3,16=DXT5, pixoff@0x18,
runtime +0x24 texcache datum, +0x2C base). ui.map: 0x800 header then zlib; scratchpad ui_decompressed.map
+ uiwid.py parse it (tag base 0x803A6000). Build env: `export VITASDK=$HOME/vitasdk
PATH=$HOME/vitasdk/bin:$PATH` before `make RECOMP=1`; output is xboxvita-recomp.vpk (NOT xboxvita.vpk);
shaders compile in-emulator via `tools/vita3k.sh shaders shaders`; run `tools/vita3k.sh install
xboxvita-recomp.vpk XVIT00001; tools/vita3k.sh run XVIT00001 <secs> <framedir>` (frames every 1.5 s).
Host knobs: XV_DUMP_DRAWS/XV_DUMP_TEX/XV_DUMP_PS/XV_DUMP_MEM/XV_DUMP_EVERY(-> host/softgfx_%04u.ppm)/
XV_LOG_TEX=<frame>/XV_LOG_RS. Harness runs >=140 s: use run_in_background; never chain build+run+grep
in one Bash call (exit 143). Not modelled: smoke glow term, item highlight bitmap; [col] debug log in
recomp/kernel/xd3d.c still noisy.
VERIFIED ON VITA3K (2026-09-01 17:40): main menu renders correctly (logo + CAMPAIGN/MULTIPLAYER/SETTINGS),
frame saved recomp/host/vita3k_menu.png. Y-orientation fix: replay viewport must be
sceGxmSetViewport(480,480,272,-272) (GXM default sign) since Halo's VS emits D3D clip space; +272 flipped it.
GAME DEMOS row is hidden on Vita because the game stats d:\XDemos\XDemos.xbe (1.1 GB haloce/xdemos, not
copied to ux0:data/xboxvita/haloce - only maps/); host shows it. Not a bug. Vita3K render window is square
(1054x1053) so raw captures are stretched; magick -resize '960x544!' restores aspect. Unknown still: the
focused-item highlight bitmap never draws on either renderer (pad focus / second bitmap sequence?).
HARDWARE PREP (2026-09-01 17:50): user now has a real Vita. Added: xv_log.c/h (xv_logf -> sceClibPrintf +
ux0:data/xboxvita/xboxvita.log; all XV_LOG/UI_LOG/BOOT_LOG/xk_os_log route through it); xk_os_vita.c
LOGICAL truncation (g_lsize per-path table; Halo SetEndOfFile's cache000/001=291MB, 003-005=49MB would
zero-fill ~770MB on exFAT - now 2KB files, reads past physical EOF zero-fill); param.sfo ATTRIBUTE2=12
(extended memory, SFO_EXTRA in Makefile); Makefile targets deploy-usb (VITA_MOUNT autodetect),
deploy-ftp VITA_IP=, deploy-log-usb, DEPLOY_MAPS?=ui.map. VPK = xboxvita-recomp.vpk (10.7MB, eboot 33MB).
Data needed on card: ux0:data/xboxvita/halo_image.bin (recomp/, 3.7MB) + haloce/maps/ui.map (14MB).
Scripted pad probe removed (A tap did not navigate the menu on Vita3K; no focus highlight drawn either -
pad/focus path is the next frontier). Est. RAM ~200MB (ELF 28 + heap 48 + arena 71 + tex pool 24).

HARDWARE CRASH #1 (2026-09-01 18:00): first real-Vita launch died right after "thread 8 created". Core
dump (ux0:data/psp2core-*.psp2dmp; parse with scratchpad core/vita-parse-core = xyzz/vita-parse-core
ported to py3, needs build/xboxvita-recomp.elf) showed NO cpu exception: xv_recomp thread Running in
SceLibKernel sceIoWrite stub, stop reason 0x10006, sp on a memalign'd fiber stack, r2=0x77 = exact
log-line length. Vita kernel kills the process when a syscall's stack-resident buffer (libkernel copies
small sceIoWrite payloads onto the stack) is outside the thread's kernel-registered stack. Hand-rolled
fibers (custom stacks) are therefore unusable on hardware for anything that syscalls. FIX: xk_os_vita.c
fibers are now real SCE threads (512KB kernel stacks) + per-thread semaphore baton handoff
(fiber_park/fiber_thread; destroy = dead flag + signal + WaitThreadEnd + DeleteThread). Vita3K never
caught this (no such check). Text slide on device: elf 0x81000000 -> 0x81047000; data 0x826d0000 -> 0x82800000.
HARDWARE CRASH #2/#3 (2026-09-01 18:40): (a) sceIoOpen failed at the 23rd playlist blam.lst - Vita
per-process fd table is small; xk_os_vita.c now keeps an LRU cache of 16 live sceIo fds behind guest
handles (path+flags, positional I/O, reopen on demand) + logs open failures. (b) REAL bug, hidden on
Vita3K/host by lenient memory: guest TLS layout was wrong. Halo's mainCRTStartup sets _tls_index =
-(round16(tlssize)+4)/4 and XapiThreadStartup builds the TLS block at KTHREAD.TlsData+4 with a pointer
slot at TlsData; codegen reads [fs:4 + index*4] = StackBase - (round16+4). Correct layout (xk_thread.c):
StackBase = stack top, TlsData = top - TlsDataSize where TlsDataSize is the PsCreateSystemThreadEx arg VERBATIM (XAPI already passes round16(size)+4; adding +4 again put the slot 16 B off - verified with the [tr] TLS chain dump), esp below. Old layout made every TLS read
garbage -> first exiting thread (24, saved games) freed -1 pointers in RtlFreeHeap -> data abort only on
hardware (Vita3K happily reads/writes past the arena). Tooling that found it: recomp/xv_trace_stub.c on
Vita = per-thread import-call trace (XV_TRACE_THREAD, default 0) to xboxvita.log, diffable vs Vita3K.
*** MILESTONE 2026-09-01 19:07: Halo main menu RUNS ON REAL PS VITA HARDWARE (user: "HOly moly it work!").
Build = xboxvita-recomp.vpk with: SCE-thread fibers, fd LRU cache, correct TLS layout, ATTRIBUTE2=12.
First run shows black for a while (first-run save/cache/playlist creation on the card) then the menu.
Unknown yet: real fps and memory headroom on hardware (pull xboxvita.log frame stats next time the card
is mounted). Agreed plan: iterate on Vita3K (menu navigation/pad focus, campaign load), hardware
checkpoint per milestone; make Vita3K stricter by pointing unmapped guest pages at a guard page.
MENU NAVIGATION SOLVED (2026-09-01 19:50): pad input always reached the game (276A5C pad state, UI event
queue at 2E4410) but Halo blocks menu input while its saved-games scan thread runs (flag 2E4065, set in
f_000CFE70, cleared when GetExitCodeThread(hThread) != STILL_ACTIVE). XAPI GetExitCodeThread reads the
KTHREAD directly: SignalState byte at +4, ExitStatus at +0x120 -> xk_thread_exit now sets both
(KTHREAD_EXITSTATUS moved 0x1F4->0x120, KTHREAD_SIGNALSTATE 0x04). Focus highlight was never a render
problem - focus is only assigned after the scan completes. Verified host + Vita3K: highlight moves, A
opens Settings->"SELECT PROFILE TO EDIT". Tooling: host XV_PAD="450:down,520:a" (xk_os_host.c);
Vita/Vita3K: ux0:data/xboxvita/pad.txt "600:down,700:down,800:a" (xk_os_vita.c, absent on hardware).
Host [dd] dump now prints combiner constants (psc C0/C1/FC). The ring backdrop is NOT DrawVertices
(no Draw* calls at all in menu frames): it's the ui.map scenario's 3D sky, which the engine never
renders - the 3D renderer path never starts; investigate why (next). Vita3K GXM: profile-screen
button-legend text garbled.
RECOMPILER BLOCK-ORDER BUG (2026-09-01 ~21:00, root cause of "no ring/3D"): xbe_recomp.py emit_function
emitted blocks in address order with NO jump to the entry, so any function owning a block below its entry
started executing another function's code - 790/8101 functions. game_time_initialize (0xFA620, sets
game_globals.active=1 at main-menu load) ran 0xF8140 instead -> menu inactive -> render path took the
UI-only branch (main loop 0xBD819: world renders only if gg.active || gg.paused_by_ui). Fixed: emit
`goto L_entry` when min(block)!=entry (regen takes ~5 s; host rebuild ~35 s with JOBS=16). Now: active=1,
game ticks, scripted camera moves, 7 DrawIndexedVertices/frame (ring 411 tris, planet, 2 quads, BSP
290/1456/172; VS fnv AC1984DB sky & 9E2E9021 env). Halo internals learned: game_globals ptr 2F8CA0
(+0 map_loaded,+1 active,+2 paused_by_ui,+0xC.. game_time: +0x10 elapsed,+0x18 speed); main loop
f_000BD420; main_menu_load f_000BD0A0->f_000BCE20->main_new_map f_000BCD40; UI-only frame f_000BC8E0->
f_0005D260; world frame f_000BCB30; widget load f_000D04E0 (def flags bit1 = pause game); events
2E4410; camera director 2331D9/E4. Side effects fixed: intro movie now really played -> Bink hung in
sound feed (DirectSound buffers had no memory/Lock) -> xd3d.c now has real ds_buffer model (backing
memory from DSBUFFERDESC/SetBufferData, time-based cursor, Lock ptrs, Play/Stop) and stream XMEDIAPACKET
pacing + CDirectSoundStream vtable (0x1D6CF4) via xv_hle_extra table in xv_call; then Bink hit a NULL
converter fn ptr -> BinkOpen is now HLE'd (--hle-addr 001B83C0:BinkOpen:2, returns NULL = movies
skipped; Bink decode is far too slow for Vita anyway). Regenerate cmd now includes that --hle-addr.
Debug helpers added: xv_guest_ptr() for gdb, XV_D3D_HIST=<frame> per-frame HLE histogram (xd3d.c),
XD3D_RET call-site logging, XV_FORCE_ACTIVE=<frame>, XV_SPIN_BT=1 (host backtrace on preempt),
preempt log shows guest stack code ptrs; [wait] logs in xk_thread.c (temporary, noisy - trim later).
MESH PATH (2026-09-01 22:15): world geometry renders on Vita3K (ring visible behind menu, camera moving).
Wiring: xv_ui_gxm.c xd3d_r_draw/xd3d_r_clear/xd3d_r_present (strong hooks) push kernel xd3d_state into
xv_d3d.c (Stage-3 bridge, now compiled in RECOMP build; XV_LOG -> xv_logf): xv_d3d_handle_for_hash(fnv)
registers halo_vs_XX on demand, xv_d3d_SetAllConstants, xv_d3d_DrawIndexedVerticesBase(prim,count,
ibData,index_base), xv_d3d_EndFrame() -> frame id replayed in main.c xv_gfx_render_frame before UI replay
(viewport set there). Textures: xv_d3d texture_for uses xv_ui_gxm_texture() (CPU-decoded cache) in
recomp build. Token conversions: GL blend enums->D3DBLEND, z_func 0x200+i -> D3DCMP, cull 0x900/0x901.
Clear goes to xv_d3d_Clear (UI clear quad disabled: it would wipe the world). TODO next: register-
combiner -> fragment programs (currently texmod = tex0*color0 for everything), BSP draws not visible
(depth or tex fmt 00), blend 9/1 (DESTCOLOR/ZERO lightmap multiply) unsupported, DrawVertices FVF.
OVERNIGHT 2026-09-01/02 (autonomous, Vita3K-only per user):
- Hardware "freeze when highlight moves" = thread 8 spinning in Halo's sound channel pump (0x28B00): our
  CDirectSoundStream vtable base was wrong. NOTE halo_image.bin has an 8-BYTE HEADER (base,size) - offset
  file reads by 8! Real vtable from ctor 0x1946EA: [obj]=0x1D6CF0 {AddRef,Release,GetInfo,GetStatus,
  Process,Discontinuity,Flush}, [obj+4]=0x1D6CE4; CreateSoundStream returns obj itself; CreateSoundBuffer
  returns obj+0x1C. Halo calls vtbl+0xC GetStatus(this,&dw) (bit0=accept) and vtbl+0x10 Process(this,pkt,0).
  3925 DSSTREAMDESC = {flags, maxPackets, wfx, lpfnCallback(+0xC), lpvContext(+0x10)} (no mixbins);
  XMEDIAPACKET = {pvBuffer, size, pdwCompleted, pdwStatus, pContext(+0x10)}. Stream packet completion now
  fires the callback (__stdcall (streamCtx, pktCtx, status)) via xv_call - Halo decrements its pending
  count there. DSOUND symbol arities verified against thunk `ret N` (all fine). Two lifted DSOUND helpers
  HLE'd: --hle-addr 0019C5E7:DSoundVoiceIsPlaying:1 0019C5FF:DSoundVoiceStop:1.
- Recompiler: jump tables indexed downward (memcpy tails `neg ecx; jmp [ecx*4+T]`) were missed -> trap at
  0x19692 on profile creation; switch_targets now walks negative indices (6760 targets vs 4933).
- XBE HEADER WAS MISSING from halo_image.bin (xbe_image.py copied sections only). XAPI reads it at run
  time: SizeOfStackCommit (0x80000!) -> main thread had a 64 KB stack -> checkpoint save (128 KB frame)
  overflowed and wiped the kernel heap (03D0xxxx) -> all DSound objects zeroed -> sound-pump trap on
  campaign start. Fixed: header copied into the image. Side effect: XapiInitProcess now runs the real init
  (title id 4d530004, TitleMeta/TitleImage/SaveImage.xbx, XMountUtilityDrive -> implemented, returns TRUE).
  halo_image.bin on the CARD must be updated too (deploy-usb copies it).
- RESULT: CAMPAIGN -> profile keyboard -> a10.map loads and runs on host: 212 DrawIndexedVertices/frame,
  game active. Pad script for it: "500:a,700:a,800:a,900:start,1100:a,1300:a,1500:a".
- LEVEL RENDERING (2026-09-02 00:00-02:00, Vita3K): a10 cryo bay renders lightmapped (keep_cryo_a10.png).
  Chain of fixes, each verified with a Vita3K frame capture:
  * Textures: level working set blew the 24 MB RGBA decode pool (469k "decode pool full" lines, log grows to
    24 MB - the on-card log APPENDS across runs, truncate before a Vita3K run). Now: DXT uploaded as BC1/2/3
    via sceGxmTextureInitSwizzled with blocks reordered by the same twiddle ui_unswz() uses (Morton in the
    min square, tiles along the long axis) - no decode; mip chain walked down to <=256 px (UI_TEX_MAXDIM);
    pool 32 MB, 768-entry cache, bad-texture list 1024; pool exhaustion sets tex_purge -> after present
    sceGxmDisplayQueueFinish + wipe (rate-limited 8 frames).
  * Offscreen render targets: Halo renders object shadows to a texture (SetRenderTarget 03D05800/5880 ...
    Clear F0 ... draws with vs_39, then samples it with blend ZERO/INVSRCCOLOR). We ignored SetRenderTarget,
    so the offscreen colour clears wiped the frame -> black world. Now xd3d_r_state("SetRenderTarget") sets
    g_offscreen (record nothing) and xv_d3d_NoteOffscreenTarget(surface Data) makes draws sampling it skip.
    X_D3DCLEAR_TARGET is 0xF0 (R|G|B|A), ZBUFFER 1, STENCIL 2. Z-only clears use a COLOR_MASK_NONE variant.
  * D3D LIBRARY GLOBALS the game reads directly (XDK 3925): D3D__IndexData 0x18F17C (SetIndices writes
    pIndexBuffer->Data; Halo passes DrawIndexedVertices pIndexData = that + WORD offset -> without the mirror
    every draw after the first used indices 0,0,0...), D3D__TextureState 0x18F180 [stage*32+type],
    D3D__RenderState 0x18F380 [state] (0x74 entries), D3D__pDevice 0x192390 (not touched by game code).
  * xv_guest_ptr in the RECOMP build now goes through the page table (g_xpt) like X_G - the linear
    mask was wrong for heap objects (texture headers).
  * REGISTER COMBINERS = real pixel shaders now. Halo builds ONE X_D3DPIXELSHADERDEF in .bss (0x2FCE60),
    SetPixelShaderProgram's it, then PATCHES combiner registers per material through the push helper
    0x1820A0 (HLE SetRenderState_Simple: ecx=method, edx=value) and through SetRenderStateNotInline (D3DRS_*
    0..0x33 are the def fields; state->method table at .rdata 0x1F08C8, 0x52 entries; our HLE now routes it
    through rs_method()). xd3d.c keeps ps_shadow (def + pokes: method->def offset map ps_method_to_def:
    0x260 alpha ICW, 0x288/28C final, 0xA60/0xA80 C0/C1, 0xAA0 alpha OCW, 0xAC0 colour ICW, 0x17F8 compare,
    0x1E20/24 final consts, 0x1E40 colour OCW, 0x1E60 control, 0x1D90 texmodes, 0x1E74/78) and hashes the
    program fields per draw (xd3d_ps_sync). Constants are PER STAGE (bit 12/16 of PSCombinerCount =
    UNIQUE_C0/C1, parser polarity was inverted): runtime uploads psc[18] = C0[8], C1[8], final C0, C1.
    Pipeline: run in Vita3K -> log "[psdef] hash hex" + "[pspair] vsfnv pshash" -> tools/ps_pipeline.py
    <log> (shaders/psdefs/, generates shaders/ps_<hash>_<vsmask>.frag.cg with --varyings from the VS's
    "outputs" header, writes shaders/xv_ps_table.h) -> compile with XVSC00001 in Vita3K (copy .cg to
    ~/.local/share/Vita3K/Vita3K/ux0/data/xboxvita/shaders/, `tools/vita3k.sh run XVSC00001 60`, copy .gxp
    back) -> make. xv_d3d.c links ps programs per (vs slot, table entry, blend); unknown pairs fall back to
    texmod. CUBEMAP stages are approximated as normalisation cubes. 33 programs for a10 cryo + menu.
  * Blend: variants are now dynamic (g_blend_combo, 11 slots + NOCOLOR), any D3DBLEND pair.
  * Debug knobs (Vita3K via ux0:data/xboxvita/env.txt KEY=VALUE, parsed in main()): XV_D3D_HIST=<frame>
    also traces every draw/clear/SetRenderTarget/SetIndices of that frame ([hist] cmd ...) + NV2A methods
    poked; XV_DUMP_VS=halo_vs_16 dumps 3 transformed verts + indices per draw; XV_FS_FORCE=tex0|lm|texmod
    bypasses combiners; XV_FUNC_HIST=<frame> (needs regen with --trace-funcs) dumps a per-frame histogram of
    every recompiled function (recomp/xv_funchist.c) - used to diff menu vs level frames.
  * OPEN: in a10 the cinematic never advances (same on host and Vita3K): world + Chief in cryo tube render,
    game ticks run (game_time_globals +0xC/+0x14 advance 30/s), START opens the pause menu, but scripted
    camera/objects never move and no letterbox appears. game_time_update = f_000FA920 (tick loop at
    0xFAAB0, tick fn candidates F8780/F7F20 called 1/frame). Next: find hs_update / cinematic globals,
    check what the a10 startup script sleeps on (sound_impulse_time? sleep_until?).
  * OPEN: cryo tube lid/glass draws black (unknown/transparent combiner), HUD text strip top-left garbled,
    shadows dropped (offscreen pass), DSOUND still silent.
- LATE FIXES (02:00-02:40): GXM twiddle is PowerVR order (Y in the even/low bits, X odd) = transpose of
  NV2A's - BC upload with NV2A order scrambled the menu font/icon atlases. gxm_unswz() fixed square
  textures; NON-SQUARE twiddle is still unknown (ring/planet streaked) -> BC upload only for square DXT,
  everything else takes the RGBA decode path at the capped mip. Menu verified identical to pre-change frame.
- CORRECTION on the "cinematic stalls": it DOES play the camera-only shots - frame sequence LOAD LEVEL ->
  CHOOSE DIFFICULTY -> exterior shot (Autumn silhouette black/untextured against the planet, sky yellow-
  green instead of space) -> hull flyby (hull texture streaked, probably the cubemap approximation or
  texcoords) -> cryo bay, where it holds forever. The stall point is the first dialogue-synced shot, and no
  sounds are ever started in the level (0 CreateSoundStream after the menu) -> suspect Halo waits on a sound
  (sound_impulse_time / sound_playing) that our DSOUND HLE never starts or reports. Investigate the sound
  cache/impulse path next (xd3d.c DSound HLE, DSoundVoiceIsPlaying returns 0).
- Release build for the morning: xboxvita-recomp.vpk (02:15, 11.5 MB) generated WITHOUT --trace-funcs
  (per-function counters cost a load+branch per call); recomp/halo_image.bin unchanged since 23:14 (has the
  XBE header). Card needs: VPK reinstall + halo_image.bin + haloce/maps/a10.map (174 MB) via
  `make deploy-usb DEPLOY_MAPS="ui.map a10.map"` (Vita was not USB-connected overnight).
- HARDWARE 2026-09-02 07:00: first a10 load on the Vita crashed with a data abort in f_000E6460 - VFP `vldr`
  at an unaligned guest address (x86 `fld dword [esi+6]`). Vita3K/host never fault on this. Fix: X_M16/
  X_M32/X_M64/X_MF32 in recomp/xv_x86rt.h use aligned(1),may_alias types so GCC emits ldr+vmov / ldr pairs
  (never vldr/ldrd). Also: recomp objects had NO header deps in the Makefile (rebuild after a header edit
  did nothing) - added $(wildcard recomp/*.h recomp/kernel/*.h) to the %.o rule. With that build the a10
  intro cutscene PLAYS ON HARDWARE: Autumn, camera pan, Longsword models; ring + some models render wrong
  (their VS/PS pairs are not in xv_ps_table.h yet -> texmod fallback). User measured 7-8 fps (menu smooth).
  No perf work done yet: recomp code is -O1, every guest access goes through g_xpt, single core.
- Vita pad mapping (xk_os_vita.c): D-pad doubles as the Xbox-only inputs (Halo default layout ignores the
  D-pad in gameplay): down=LTHUMB click (crouch), up=RTHUMB click (zoom), right=White (flashlight),
  left=Black (switch grenade); D-pad bits still sent for menus. Built 07:14, not yet pushed.
- 2026-09-02 08:00-10:45 (user away from hardware; Vita3K + host):
  * "a10 cinematic stall" was a misread twice over: the intro plays fully (exterior -> bridge/Keyes -> cryo);
    the level then waits on Halo's look tutorial: script sleeps on player_action_test_look_relative_left, then
    _right (flags at [[0x276794]]: 0x200 left, 0x400 right, 0x80/0x100 pitch, 0x800 forward; setter
    f_000F8A70 from the player action struct; hs function table at 0x1EB4xx: {ret,flags,name*,evaluate,parse,
    IMPL at +12}). Scripted stick input passes them; the user does the rest by hand on hardware.
  * Pad: Y axes wrapped at raw 0 (+32768 -> int16 -32768: full-up read as full-down) - clamped. D-pad
    doubling (Black/White/stick clicks) broke UI navigation (extra bits = cancel in Halo's UI): now only when
    a non-UI map is streaming (xk_file_in_ui_map from the cache header type at +0x60: 0 sp,1 mp,2 ui; Halo
    copies maps to z:\cacheNNN.map so names don't tell) and !paused_by_ui. pad.txt/XV_PAD syntax now
    "frame:input[*hold]" with l/r triggers and lup/ldown/lleft/lright/rup/... sticks.
  * Split screen needs TWO players (dialog after START at the P2 screen); System Link needs the recompiled
    XNet stack + PHY emulation -> Blood Gulch solo is not reachable yet.
  * TEXT FIXED (in-game small font): three layers - UI path read the stale combiner def (now ps_shadow via
    xd3d_ps_sync), picked the highest texture stage read anywhere (now backward liveness over the combiner:
    text = t0*v0 -> t0; AB/CD products tracked separately, ICW A=bits24 B=16 C=8 D=0, OCW cd=0 ab=4 sum=8),
    and the glyph texture is Halo's RUN-TIME FONT CACHE (CreateTexture 128x128 A4R4G4B4 @03D06C80) which
    the game fills incrementally -> dynamic textures (headers in the kernel heap >= 0x03D00000) are
    content-hashed once per frame and re-decoded on change (ui_tex_hash). L8 textures: no white-RGB hack;
    coverage variant (RGB white, A=L) only for the UI path.
  * QUADLIST/POLYGON DrawVertices now rewritten to indexed triangles (rewrite_quads, per-list pool).
  * AUDIO: recomp/kernel/xk_audio.{c,h} software mixer: Xbox ADPCM (0x69, 36 B/ch/64-sample blocks, headers
    then 4-byte channel-interleaved nibbles) + PCM, per-voice resample (16.16) + volume (dB/100), 48 kHz
    stereo (Vita MAIN port rejects 44.1k), grain 1024, thread on core 2 (xk_os_audio_* sinks: sceAudioOut /
    host paced sleep + XV_WAV=<file> dump). Halo uses 12 XADPCM 22 kHz streams for everything; stream packets
    complete when the mixer consumed them, with a due_us+300 ms fallback so a stalled device can't wedge the
    pump. Started lazily in DirectSoundCreate. Host WAV of the menu music is smooth (diff/rms 0.09) - decode
    verified statistically, not by ear. NOT yet heard on hardware.
  * Camera readout in frame stats now real (view-projection rows of the first world draw); frames showed
    Keyes' face on the bridge with letterbox bars.
- PLAN agreed 2026-09-02 11:30 (user): (1) promote env.txt to ux0:data/xboxvita/xboxvita.cfg read at boot
  (fps overlay, log level, volume, control preset incl. D-pad doubling on/off + rear-touch preset, stick
  deadzone/sensitivity/invert, texture pool, debug knobs); (2) in-game debug overlay on hold SELECT+START:
  fps, frame-time split (game/record/GPU), draw+texture counts, voices, free memory - part of the perf work;
  (3) later a plain GXM text launcher (settings/debug/game list, home of the games/halo split). No plugins.
  Performance is the next work item (-O2 recomp build, identity fast path for guest memory, per-draw
  overhead), validated for correctness in Vita3K; fps numbers only on hardware.
- Release for the user to test (2026-09-02 ~10:45): xboxvita-recomp.vpk with text fix + audio mixer + pad
  fixes; copy kept at scratchpad xboxvita-recomp-audio-text.vpk. Untested on hardware: audio audible?,
  BC textures on real SGX, D-pad doubling in-level.
- PERF pass 1 (2026-09-02 11:45): main.c requests scePower 444/222/222/166 (homebrew default is 333/111 -
  nothing had asked before); recomp objects -O2 -fno-strict-aliasing (eboot 35->27.7 MB, full rebuild ~6 min
  on 16 cores; `rm build/recomp/*.o` needed - the %.o rule does not depend on the Makefile); XD3D_COUNT is a
  no-op unless XV_D3D_HIST/XV_DS_CHECK set; texture cache lookup hashed (texhash[1024] + chain). Frame-time
  split logged every 60 frames by xd3d_r_present: "[xv/ui] frame time: game X ms + render Y ms = fps".
  Vita3K-verified. Staged: release/xboxvita-recomp-2026-09-02-perf-O2-clocks.vpk (supersedes text-audio).
- 12:15: config file ux0:data/xboxvita/xboxvita.cfg (KEY=VALUE -> setenv at boot; env.txt still read) and the
  debug overlay (xv_ui_gxm.c xv_ui_gxm_overlay: 7-segment fps/game-ms/render-ms via the clear program; toggle
  SELECT+START held ~1 s, swallowed from the game, or XV_FPS=1; XV_DPAD_EXTRAS=0 disables the D-pad doubling).
  Staged release/xboxvita-recomp-2026-09-02-perf-overlay.vpk (has everything: text, audio, pad, -O2, clocks).
  LESSON: a python str.replace with an empty `old` (slice with end<start) exploded main.c into 51 MB - recovered
  by splitting on the inserted text. Always assert start<end before slicing; the repo has no git yet.
- 12:15-13:05 HARDWARE RESULTS: log confirmed clocks 444/222 applied; audio thread creation FAILED with the
  0x10000060 priority (priority 64 + CPU_MASK_USER_ALL fixed it -> "The sound is working!!! buggy"); sticks
  were dead on hardware because sceCtrlSetSamplingMode(ANALOG_WIDE) was never called (Vita3K ignores the
  mode) -> fixed, "sticks work". Frame time on hardware in-level: game 86-180 ms + render 0.2-5 ms -> the CPU
  (recompiled code) is ~97% of the frame, GPU negligible. Sampling profiler added (xv_funchist.c: XV_FN stores
  xv_cur_fn, 1 kHz thread samples it, [prof] top-40 every 10 s; on by default in --trace-funcs builds,
  XV_PROF=0 off). Vita3K profile only shows Halo's vblank wait loops f_000BB060/f_000BB4E0 (idle at the 30 fps
  cap) - hardware profile pending (release/xboxvita-recomp-2026-09-02-prof.vpk). Second Vita (card 334B-7B5D)
  got a full deploy; user copies VPKs between Vitas by hand. Combiner table now 97 programs / 100 pairs.
  User taking Vita screenshots (ux0:picture/SCREENSHOT/) of visual bugs for matching against [pspair] lines.
- 13:40-14:30 CODEGEN PERF (user away): Vita3K level profile is useless for hardware (82% in Halo's vblank
  wait loops f_000BB060/f_000BB4E0; emulator does the level's work in ~6 ms vs 100-180 ms on the Vita ->
  the A9 is 15-20x slower on our generated code: memory traffic suspected). Recompiler changes, all
  verified host+Vita3K (intro -> cryo -> look tests -> next cutscene, 0 traps):
  * dead-flag elimination: per-block backward liveness with iced rflags_read/rflags_modified; strip the
    X_FLAGS(...) store (7 stores/op) when overwritten before any read (FLAGS_RE, FLAG_KEEP for adc/sbb/rcl/
    rcr/cmpxchg/cmps/scas/sahf/lahf/pushf/popf); block end conservative (all live) except after call/ret.
    code_012.c: 3948 -> 1595 X_FLAGS. Release: release/xboxvita-recomp-2026-09-02-prof-deadflags.vpk.
  * `xctx *restrict c` on generated functions + per-function locals xram_/xpt_ with X_G redefined per
    code file, so GCC can keep guest registers and the memory base in ARM registers across guest stores
    (may_alias access types otherwise force reloads of everything after every store). Building.
  * profiler attribution fixed: XV_FN_BACK(caller) after each direct call (leaf helpers were absorbing
    their callers' time).
  * Audio: static was (a) ADPCM blocks straddling stream packet boundaries (now a carry buffer assembles
    whole blocks), (b) sample-and-hold resampling imaging 22 kHz sources as hiss (now linear
    interpolation; hi/lo band energy 0.058 -> 0.014), (c) -6 dB headroom. User heard "hiss" on the
    pre-fix build; fix in release/...-prof-audio2.vpk and later. Spectrogram tool: numpy in venv
    (scratchpad spec*.png). Remaining: clicks at stream start/stop.
  * User wants a website: halovita.io (HaloVita = the Halo port, XboxVita = toolchain); static site from
    the progress artifact, never host game data.
- 14:30-16:00: System Link path in Vita3K reaches SELECT MAP / SELECT GAMETYPE (maps listed by hardcoded
  names d:\maps\<name>.map; the Vita only had bloodgulch -> empty list: deploy all 24 maps, 1.8 GB), then
  "Unable to load saved game file" after picking a gametype: game does FILE_OPEN on z:\lastmpvr.txt (not
  found; seeding it did not help) - the variant "saved game" comes from somewhere else; parked.
  XV_MAP_REDIRECT (xk_file.c map_redirect: by cache-header name, header name patched on read via
  alias_name) boots the substitute but Halo shows "problem with the disc" - cache validation goes beyond
  the header (cache files are FIXED-SIZE slots: 291 MB campaign / 36-49 MB MP; z:\saved\hdmu.map is
  probably the cache index). Level select still open. Cache header tag is the bytes "daeh" (dword
  0x68656164) - the earlier 0x64616568 check meant map-type detection (D-pad extras gate) never fired.
  Fixed. Unknown-pair fallback: FS_TEX0 when the VS writes no color0 (xv_vs_outputs table from
  ps_pipeline). Table: 136 pairs / 133 programs. git: 5 commits. release/xboxvita-recomp-2026-09-02-latest.vpk
  (15:47) = everything incl. profiler (regen without --trace-funcs for a non-profiling release).
- RELEASE TARGET (user, 2026-09-02 16:03): public release on Labor Day, Mon 2026-09-07, as a technical
  preview. Gate: stranger reaches the menu in <1 h from a clean checkout (extraction tools documented, 2 OSes);
  zero crashes in boot->profile->a10->cutscene->save&quit; audio clean; site status board = build truth
  (incl. fps); no game data anywhere, LICENSE, "own copy" stated 3x. Plan: Wed hardware pass + profile-driven
  perf; Thu-Fri perf + soak, feature freeze Fri night; Sat release engineering (clean checkout, docs,
  versioned VPK, repo public audit); Sun buffer, tag v0.1.0-preview; Mon publish site + write-up.
  Naming: site HaloVita, paths xboxvita (README explains once; no renames before release).
- 16:50-17:35: HARDWARE screenshots (18, scratchpad/vita_pull): overlay 9-11 fps, game 94-121 ms, render 3-4
  ms; menu 20 fps (45 ms). Visible bugs: Chief pink/green in the pod, hull fly-by streaks, menu ring garbled,
  exterior sky yellow/grey; bridge/cryo scenes right; SELECT MAP works with all 24 maps deployed. The log is
  truncated per boot -> now rotates (.1/.2/.3). CUBE MAPS implemented: Format bit 2 = cube, 6 faces back to
  back, face stride = mip chain bytes padded to 128 (verified by dumping faces: room reflection, planet glow,
  normalisation cube); decoded to RGBA, twiddled with gxm_unswz, sceGxmTextureInitCube; generator emits
  samplerCUBE/texCUBE (--cube real, default) and the ps table carries cube_mask; xv_d3d binds an 8x8 grey
  fallback cube when a cube stage has a non-cube texture. Chief's armor correct again in Vita3K. Decoded
  textures now carry a box-filtered mip chain (levels packed after level 0, linear texture mipCount; min
  filter MIPMAP_LINEAR). XV_TEXDUMP=<minw> dumps decoded textures/cube faces as PPM (Vita3K ux0 texdump/).
  XV_D3D_HIST_LEVEL=<n> traces the n-th frame after level start. Hull fly-by streaks are NOT textures (they
  decode perfectly) nor cube maps: likely texgen/reflection UVs on that BSP - open. System Link "Unable to
  load saved game file" reproduced on hardware. release/xboxvita-recomp-2026-09-02-cubemaps.vpk (17:33).
- 17:40-18:30: b30.map was CORRUPT (header all zeros, 291 MB): the XV_MAP_REDIRECT experiment redirected
  the cache DESTINATION path too, so the cache rebuild copied b30 over itself. Re-extracted from
  haloce.xiso.iso (extract-xiso -x -d, 162,877,440 B), copied to haloce/maps, Vita3K data dir and card 1.
  map_redirect/alias_name REMOVED; xk_file now refuses write/truncate on any host path containing /haloce/
  (game media is read-only). Lesson: never route game-data paths through a writable redirect. XV_LEVEL=b30
  (level-table string patch) WORKS on host once the map is intact (b30 loads, camera, 37 Begin/End).
  Hardware log 17:53: "black screen past a door" = the game thread stopped presenting (no trap; prof thread
  kept dumping) right at the a10a->a10b structure-BSP switch; build on card had no XV_FN so prof named
  nothing. Prof-instrumented build (regen --trace-funcs) verified in Vita3K: "[prof] 10000 samples, 366
  functions" - works. D-pad DOES move the player in gameplay (user) -> in_control now masks D-pad bits.
  DARK BSP WALLS (hardware AND Vita3K, so a translation bug): level draw trace shows Halo's environment
  program (vs_16 + ps 953BD9C7/32F4D803) does dot(t0 bump, t3 normalisation cube) with t0 = 512x512 P8
  (0x0B) - we decoded P8 as luminance, so normals were noise. Implemented D3DDevice_SetPalette
  (xd3d_state.palette[stage] = 0x80000000|RES_DATA), per-draw xv_d3d_SetTexturePalette, texture cache
  decodes P8 through the palette (D3DCOLOR ARGB -> ABGR) with the palette hash in the cache identity
  (ui_tex_entry.palsum). Hist "[hist] cmd" now prints "(off)" when blend is disabled. Frame trace facts:
  all BSP draws in the traced frame were blend 5/6 factors (enable unknown before this fix), 0x304 poked
  188x/frame; t3=07:64x64 cube shared by all BSP draws.
- 18:30-18:55: ROOT CAUSE of black/white/garbage BSP lighting: xv_d3d.c per-frame vertex CONSTANT POOL was
  48K floats = 64 full-window (c[-96..95], 768 floats) snapshots; Halo's environment/skinned programs
  (vs_09/vs_16/vs_39/vs_47...) read the full window and a level frame has 250+ draws -> every draw past
  ~64 was issued with NO constants (const_n 0, no uniform buffer reserved) = GPU read stale memory ->
  nondeterministic per run (b30 beach black in one run, white in the next). Fix: SetAllConstants bumps
  S.vsc_gen only when c[] changes; record_draw reuses the previous snapshot while (gen, base, count) are
  unchanged; pool 256K floats (1 MB/list); "[d3d] frame N: K draw(s) without constants" log. Verified:
  b30 renders consistently (Pelican lit, cliffs/rocks textured); a10 unchanged. Likely also the cause of
  the hull fly-by streaks + black ship silhouette (UV/transform constants missing) - confirm on hardware.
  Makefile: code_*.o now depend only on xv_recomp_protos.h/xv_x86rt.h; kernel/HLE objects use -MMD ->
  kernel header edits rebuild in ~8 s instead of 6 min.
  a10 cryo walls STILL dark after (a) P8 palette (verified: decoded P8 dumps are proper normal maps,
  ~(127,127,250)), (b) replacing the normalisation cube (--cube normal stand-in, compiled on-device)
  -> not the cube. ps 953BD9C7 math: out = t2(diffuse) * lerp(1, N.L, v0.a) + t1.r, N.L =
  dot(expand(P8 normal), expand(cube L)). Suspects left: v0.a/oT3 (tangent-space light vector from the
  VS: NORMPACKED3 tangent/binormal unpack, or constants), MUX_MSB semantics, dot-stage output clamp.
  On b30 the sand is over-bright white (ps 154066FD: out = 2*t0*t1*(v0+t2.g) + ...). Sky draws as the
  clear colour (sky model missing?). git 815f244. release/xboxvita-recomp-2026-09-02-consts.vpk = current
  best (prof-instrumented; regen without --trace-funcs for release). Card 1 has -pal build + fixed b30.map.
  XV_HIST_CONSTS="13,23,..." prints D3D-numbered vertex constants per traced draw (Halo uses
  D3DSCM_192CONSTANTS: SetVertexShaderConstant regs -96..: -84 n3 = UV rows hw12-14, -79 n11 = lighting
  hw17-27; the .cg slot comments use HARDWARE indices c[0..191]).
- 19:04 FIRST HARDWARE PROFILE (30 s cryo bay, 10 fps, prof build): 12D79 17.6% (XAPI WaitForSingleObjectEx
  wrapper: call [1D666C] loop while 0x101), BB060 12.2% (vblank-count wait loop: [1F8C80] counter vs
  [2E3660] target, Sleep(1) via 12EBB), 88B80 5.7% (x87 vertex/matrix maths, 641 lines), 53F60 4.2%
  (3-instruction compare fn -> a qsort-style comparator called via fn ptr from the 53E90 loop over
  [39BE58]+0xF8 entries), 7EDF0 3.3%, 52520 3.0%, then a flat tail (top30 = 73%). Caveats found: (1)
  xv_cur_fn was NOT restored after a fiber switch, so time after the main thread resumed was charged to
  whatever helper thread (t20 vblank-event waiter at 30/s, t12) last entered -> the 12D79 figure is
  partly mis-attribution (fixed: xk_yield saves/restores xv_cur_fn per thread); (2) XV_FN_BACK only after
  direct calls, so indirect-call returns smear time onto the callee. Scheduler = cooperative fibers on ONE
  OS thread (xk_run_until_idle; idle sleeps until the earliest timeout). Added: blocked-time accounting
  (xk_thread.c ws_add: per thread x object kind x object, caller eip, count/ms; KeDelayExecutionThread as
  kind "delay"; scheduler idle ms) dumped with every [prof] dump as "[wait] idle ...", and XV_VBLANK_HZ
  (vblank_thread rate, default 60; Halo waits for a vblank event then for count>=target each frame - at 10
  fps up to 2 vblanks lost per frame, so 240 Hz may buy ~20 ms/frame if the wait stats confirm). Custom
  loading bar REMOVED at the user's request (git a573213): Halo draws its own loading screen (CreateTexture
  320x240 fmt 12/3F + 128x16, 2 draws/frame at ~6 fps during the load) - it shows black; the pairs it uses
  were among 3 missing programs (vs AC1984DB ps 557269F4/E1ABD409, vs FF7E5030 ps CCFE3765). ps_pipeline
  over the hardware logs: 136 -> 308 pairs / 299 programs, all compiled (Vita3K XVSC), packed in
  release/xboxvita-recomp-2026-09-02-waitstats.vpk (pushed 19:20). User: XV_LEVEL never applied on card
  (no xboxvita.cfg there) - a10 is fine for them.
- 19:30 COLOUR WRITE MASK: hull trace shows Halo's environment passes: vs_58 (oT0 = stream-1 NORMSHORT2
  lightmap uv) + ps 91A49E77 (rgb = 0, alpha = dot(lightmap, c0)) blended DESTALPHA/ZERO, with NV2A
  method 0x358 SET_COLOR_MASK poked ~180x/frame (0x00010101 = RGB only, 0x01000000 = alpha only). We
  ignored 0x358, so the alpha-only lightmap pass painted every wall black before the DESTALPHA/ONE detail
  passes -> the dark BSP everywhere (a10 walls, hull). Implemented: xd3d_state.color_mask (init 0x01010101
  in CreateDevice), per-draw xv_d3d_SetRenderState_ColorWriteEnable(R1 G2 B4 A8), blend combos are now
  (src,dst,mask) with BLEND_MODES 24, SceGxmBlendInfo.colorMask from the combo. Vita3K a10 verification
  running at 19:35. Loading screen in Vita3K needs XV_SLOW_READ (>= 40000 us/64 KB): the load then shows
  the CHOOSE DIFFICULTY screen faded to ~1/24 brightness for ~20 s - the 320x240 loading texture quad is
  not visible; XV_D3D_HIST_SMALL=<n> (trace the frame after an in-level Present with <= n draws) exists,
  needs n ~40 since the faded UI still draws. Hull streaks: vs_58 reads the lightmap uv from stream 1
  (+4, NORMSHORT2); suspect stream-1 binding/stride for that declaration.
  RESULT 19:40: colour mask VERIFIED in Vita3K - bridge lit (walls, floor, consoles), hull lettering
  visible, Pillar of Autumn textured with glowing engines instead of a black silhouette. Blend variants
  now include "7/1 mask 8" (alpha-only lightmap pass). release/xboxvita-recomp-2026-09-02-colormask.vpk,
  git committed. Hull fly-by streaks remain (stream-1 lightmap uv). Not yet on hardware.
- 19:45 HULL STREAKS isolation (Vita3K, XV_SKIP_VS knob = drop draws by vertex program): skipping vs_40
  (9/1 multiply = ship lighting; without it the Autumn blows out white), vs_26 (7/2 bump/spec) or vs_58
  (alpha lightmap pass) leaves the streaks -> they are in the base pass vs_16 + ps 953BD9C7 (t0 0B:512x512
  P8, t1 0C:512x512 DXT1 BC, t2 05:512x512 R5G6B5). XV_TEX_MAXDIM=1024 (no mip skip) still streaky, only
  the tone changes -> not the decode; stream-1 uv bytes are sane. Hypothesis: aliasing - a hugely tiled
  512x512 surface at a grazing angle, and the BC upload path had NO mip chain (mipCount 0, LINEAR min
  filter). Implemented: BC path now reorders and uploads the whole chain down to 4x4 (levels concatenated,
  gxm_unswz per level, mipCount = levels, MIPMAP_LINEAR) - Vita3K check queued (frames_bcmip). New debug
  knobs: XV_SKIP_VS=halo_vs_40[,..], XV_D3D_HIST_SMALL=<draws>[,<min frame>] (loading frames; never fired
  in Vita3K - the faded menu still has >60 draws there), XV_D3D_HIST_TEX=<hdr> (trace the frame after a
  draw binds that texture; hooked from xv_ui_gxm.c's xd3d_r_draw - the xd3d.c xd3d_r_draw/xd3d_r_present
  are WEAK and overridden, don't put logic there). Loading texture header in Vita3K: 03D05600 (fmt 12
  320x240, created at boot in Vita3K, at load time on hardware where it is fmt 3F).
- 20:45 LOADING SCREEN parked: in Vita3K the 320x240 loading texture (hdr 03D05600 / data 01F98000) is
  NEVER bound by any draw (XV_D3D_HIST_TEX by header or data never fires), the load phase only shows the
  difficulty menu faded to 1/24; on hardware the load frames had 2 Begin/End with no new [pspair], so
  those two draws use already-known UI programs. Unknown how the 320x240 image reaches the screen on Xbox
  (CopyRects? PersistDisplay?). Not worth more time before the perf work. BOOT: "short image read
  (3244024 / 3819424)" twice on the card with a byte-identical file -> sceIoRead returned short; loader
  now loops (xv_boot.c) and logs "image read error"/"image incomplete". Card 1 now has the boot-fix
  build (= colormask + BC mips + all knobs), image verified. Hull streaks: stream-0 data and UV constants
  (c[-84..-82] identity) are correct for the vs_16 draws -> the 512x512 textures themselves as uploaded
  must be striped; full-size texdump run in progress (XV_TEXDUMP=512 XV_TEX_MAXDIM=1024).
- 20:50 HULL STREAKS parked: the striped surface samples t2 = fmt 05 (R5G6B5) 512x512 single-mip at data
  02F48000 (guest 0x82F48000, inside the a10 map data - a stored bitmap, not an RT: no SetRenderTarget or
  CopyRects touches it; CopyRects is not even called). Its raw bytes (XV_TEXDUMP_RAW=1 writes
  texdump/*.raw) look like horizontal scanline stripes under a linear interpretation; P8 512x512 normal
  maps from the same material decode perfectly. Either a genuinely odd texture (reflection/space map
  meant for texgen uv) or a layout we don't handle for that one bitmap. Scratchpad tex_hyp.py decodes a
  .raw under layout hypotheses (its swizzle branches are buggy - black). Not worth more time now.
- 20:55 HARDWARE (colormask build, ~20 min play to past the tutorial): cryo bay fully lit on the Vita
  (16 screenshots, scratchpad/vita_shots4), 8-11 fps, game 85-111 ms. GPU CRASH (hard reset, GPU dump
  20:48 in ux0:data/) right after "[pspair] A862CB93 7BC230FC ... linked against halo_vs_66" - first
  draw of a newly linked combiner program (t2/t3 CUBEMAP samplers). Hardening: replay now binds a real
  texture for every stage the program samples: 8x8 grey 2D fallback (tex2d_fallback) when the stage is
  unset or a cube texture sits on a sampler2D, cube fallback for the reverse; logs "draw: stage N
  unset/kind mismatch -> fallback". Not yet confirmed as the cause. Profile with fixed attribution:
  BB060 (vblank-count spin, Sleep(0)=yield so it burns CPU) 22.2%, 53F60 7.6%, 7EDF0 5.3%, 88B80 4.0%,
  53E90 2.8%; wait stats show t20/t12 helper threads blocked on events (normal), scheduler idle 0 ms.
  Card 1 now: texguard build (release/...-texguard.vpk) + xboxvita.cfg "XV_VBLANK_HZ=240" for the A/B.
- 21:00: guarded build showed "stage 0 kind mismatch" 55x in Vita3K = the game binds CUBE textures on
  stages whose combiner samples 2D (PROJECT2D) - almost certainly what faulted the real GPU. Refined:
  such stages get face +X of the cube as a 2D swizzled texture (same data/format/size), grey 8x8 only for
  truly unset stages; Vita3K frame brightness identical to the colormask run. git 2de69e3,
  release/...-texguard2.vpk; watcher pushes it on the next connect (card cfg XV_VBLANK_HZ=240 stays).
- 21:05 RECOMPILER: cmp/test/add/sub/and/or/xor/inc/dec/neg + jcc FUSION (xbe_recomp.py fusable_pairs,
  FLAGS_CAP_RE rewrites X_FLAGS(...) into fk_a/fk_b/fk_r function locals, fused_cond() builds the inline
  condition from XFI_* macros in xv_x86rt.h; skipped for p/np and for inc/dec carry conditions). The ARM
  disassembly of f_00053F60 showed each flag op storing 6 words into the ctx and the jcc reloading them.
  Host harness OK (43 frames, 0 traps). Vita build + Vita3K check running (frames_fuse). Not yet on
  hardware. Generated code still has --trace-funcs (prof); regen without it for release.
- 21:25 HARDWARE (colormask/texguard1 build, XV_VBLANK_HZ=240): user played cryo -> corridors -> bridge ->
  Keyes cutscene (the door hang did not recur). 49 screenshots (scratchpad/vita_shots5): corridors and
  bridge lit; magenta/purple checker surfaces on the bridge = "no program" fallback draws sampling the P8
  bump map as diffuse (5 new missing pairs); the space/ring seen through the bridge windows is the same
  striped 512x512 05 bitmap (so that texture IS the sky/space map). User reports: ring, Elites, Grunts
  render wrong, Cortana missing. 240 Hz vblank was a NET LOSS: 7EDF0 (the per-vblank callback work)
  went to 48-53% of CPU, fps 6.5-9.6; card cfg reset to 60 Hz. ps_pipeline over the new log: 388 pairs /
  374 programs (75 new) - compile + rebuild chained. Fusion: successor-aware liveness (exit_live) fuses
  60k pairs but makes the host hit the CRT x87 exception path (fnsave at 1D4A4, 64 unimpl traps) even
  with conservative dead-flag elision -> exit_live fusion is WRONG somewhere (unfound); reverted to
  block-local liveness (8072 pairs, host+Vita3K clean). VBLANK ON DEMAND implemented (xd3d_vblank_kick:
  NtYieldExecution / KeDelayExecutionThread(0) with a return address in 0xBB060..0xBB100 on the top 24
  stack words -> run the vblank callback immediately; XV_VBLANK_KICK=0 disables) - host shows 0 kicks in
  the menu, untested in-level. Watcher lesson: `make deploy-usb` REBUILDS the VPK if sources changed
  (serial, 15+ min, pushes unverified code) - copy the VPK with cp instead. pkill -f with a pattern that
  appears in the calling command kills the caller (exit 144).
- 21:45 MULTIPLAYER "Unable to load saved game file": z:\saved\hdmu.map is the SAVED-FILE INDEX (518-byte
  records: 256-byte ASCII path + metadata; rows = u:\<id>\blam.sav, playlists\default_playlist\NN\blam.lst
  ...), read twice end to end at the attempt (a read at EOF returned 518 B = the game had appended a
  record; fine). Variants ARE the playlists: saved\playlists\default_playlist\00..12\blam.lst (512 B:
  UTF-16 name "Slayer", "Slayer Pro", ...; valid). lastmpmp.txt = last map ("levels\test\bloodgulch\
  bloodgulch"), lastmpvr.txt = last variant (256 B, my seed "Slayer" was read). The split-screen attempt
  CREATED a second UDATA save dir u:\122A17771B9F (blam.sav "New002" + 16 KB savegame.bin) beside the
  profile's 122A17771B9E, then failed -> the failing read is probably in that udata dir (not visible:
  XV_LOG_READS=saved doesn't match udata paths). Card cfg now XV_LOG_READS=UDATA; kick2 build pushed
  (374 programs + block-local fusion + vblank kick + BC mips + texguard2; Vita3K clean, hardware unknown).
  vblank kick: the Sleep(1) callers seen on host are the cache-streaming waits (ret 331FD/333C3, stack
  word 8 = caller return address); BB060 never slept on the host run, so the kick is unverified.
- 21:52 "crash when opening" = boot image read short again (EOF at 3,244,024 on the Vita while Linux
  read the full 3,819,432 and cmp'd OK) -> FAT inconsistency from an interrupted copy; fixed by rm +
  fresh copy + unmount/remount + direct-read verify. LESSON: after any interrupted USB write, recreate the
  file (new clusters), never trust cmp through the Linux page cache. Card now has kick2-nobcmip (BC mip
  chains behind XV_BC_MIPS=1, default off; git 0fcef08), cfg XV_LOG_READS=UDATA. Second UDATA dir
  122A17771B9F = player 2 profile "New002" the user created for split screen.
- 22:10 (user asleep; goal for the night = Blood Gulch via multiplayer, then rendering). Striped
  512x512 fmt-05 "space" bitmap re-examined from raw bytes: only 260 distinct 16-bit values / 164 distinct
  bytes; neither 16-bit linear/swizzled nor 8-bit interpretations give a clean image -> not a plain
  bitmap layout we know; PARKED (2nd time). In Vita3K the main-menu cursor now defaults to MULTIPLAYER
  (profile remembers), so pad "500:a,700:a,900:a,1100:a" reaches SELECT GAMETYPE (Slayer / Slayer Pro /
  Elimination); "...,1400:a,1700:a,2000:a" should select Slayer and reproduce the error (frames_mp2,
  UDATA read logging on). The mp1 script (640:down) went into Campaign instead and loaded the 3.4 MB
  campaign savegame.bin (reads fine). Bisect of the exit_live fusion bug running: traps present with
  10000-E0000 (43000 fused pairs), narrowing (scratchpad/bisect_log.txt, bisect_fuse.sh).
- 22:40 MULTIPLAYER repro IN VITA3K + HOST: pad "330:down,420:a,520:down,600:a,780:a,960:a,1140:a,1320:a,
  1500:a,..." = main menu (visible from ~frame 250 without pressing A) down -> MULTIPLAYER -> down ->
  SPLIT SCREEN -> profile -> Blood Gulch -> Slayer -> "Unable to load saved game file" (frames_mp7).
  Saved-file index code (3925): 0x2D440 = append record (GetFileSize/518 -> index, max 100),
  0x2D6A0 / 0x2D570 = read/write record N (mutex 2E3360 wait 5000 ms via 12E8F, file name table at
  1F4C98[type], GetFileSize 15455, check (N+1)*518 <= size, seek 509A0, read 50930 / write 508F0),
  0x2EE30 = open saved file by handle (handle packs type in bits 8-15, index in bits 16-27): reads the
  record then REQUIRES u32 at record+4 == 0 and uses record+8 as the path -> but our hdmu.map records
  start with the path at +0 (no 8-byte prefix) -> the check fails -> error. Either the writer wrote
  from record+8 (our bug?) or the reader expects a different struct; host run with XV_WATCH_FN (new:
  logs entry args + 24 bytes at pointer args, and eax on return via XV_FN/XV_FN_BACK) on
  2D6A0,2D570,2EA40,2D440,2EE30 is running (host_mp3_result.txt). Fusion bisect DONE: exit_live
  fusion breaks only with functions in [0x23660,0x23800) = entries 236A5/236E9 (mid-instruction bogus
  entries inside a CRT float-format routine) and 237BC; per-entry test queued (bisect_log.txt FINE_DONE).
- 23:10 MP error hunt: with XV_WATCH_FN the lookup family all SUCCEEDS at the gametype screen (2D6A0
  type 0 idx 1..3 -> al 1; 2EE30 open-by-handle only runs at boot; 2E6F0 read lastmpvr -> 0 = not found,
  normal on a fresh Xbox). CCC50 = variant list init (2EF40 enumerate type-1 records -> handles via
  2D080(idx,...), pad to 3 with -1, 2E6F0 lastmpvr, 2E5A0 name->handle). Handle format: bits 0-3 type
  (0/1), 8-15 category (<9), 16-27 record index (<100). 1F4C98 table has ONE index file (hdmu.map);
  1F4C80 = per-category directory names. Index records: +0 path[256], +256 UTF-16 name, +512 u16 type,
  +514 u16 record index, +516 u16 valid. 2EE30 reads a record and requires u32 at rec+4 == 0 and uses
  rec+8 as path -> contradicts the file layout; unresolved (maybe 2EE30's out struct has an 8-byte
  prefix I misread). NEXT: per-frame function histograms (XV_FUNC_HIST=1250 vs 1340 host runs, queued
  as fh_result.txt) to find the functions first entered at the A press; the halo_image.bin mapping is
  VA = file_offset - 8 + 0x10000 (8-byte header). Fine fusion tests running (bisect_log FINE_DONE3).
  CORRECTION 23:20: 2EE30's "u32 at rec+4 == 0" was a stack-offset misread - it checks the handle TYPE
  == 0 (profile saves) and uses the record path at +0; layout is consistent, no writer bug. Variants
  (type 1) are opened by another function - the histogram diff should name it.
- 23:35 Vita3K full intro cinematic (frames_cin, 280 s): bridge lit, Keyes, marines OK; CORTANA renders
  as an opaque white figure with purple hair (hologram shader falling back). The cinematic log added 26
  programs (415 pairs / 400 programs; 1 fallback pair vs 393556F8 ps C1B115A1). XV_D3D_HIST_LEVEL=2750
  trace of the Cortana frame queued (frames_cort). Fusion fine tests: each single entry (236A5, 236E9,
  237BC) alone = 0 traps -> the mis-fusion needs a COMBINATION; combined-range tests queued (FINE_DONE5).
  Watch results (host_mp6): at the gametype pick none of 2EF40/2E5A0/CCC50/2EE30/2EA40 is the loader;
  per-frame histogram diff (frames 1250 vs 1340) queued to name the functions first entered at the press.
  Chains: bx3ncapm9 (compile 400 programs -> clean regen -> Vita build -> Vita3K -> release/...-sh400.vpk).
- 23:55 FUSION (exit_live) verdict: no single function or the whole [236A5,237BD) range reproduces the
  CRT-x87 traps -> the mis-fusion is a JOINT effect across functions, i.e. flags flowing across a
  call/ret boundary (CRT asm helpers that return status in CF/ZF, or a caller reading flags right after
  `call`). block_flag_use/exit_live assume flags are dead across calls; the old block-local pass only
  ever elided inside a block so it never tripped this. Keep block-local fusion (8072 pairs) for now;
  the fix is a whole-program pass marking functions whose return leaves live flags (callee's last
  flag-writer reaches ret) and treating calls to them as flag producers. Gametype handler: C8790 (looks
  up ustr tag "ui\shell\strings\game_variant_descriptions" via 35210 = tag lookup by group+name in the
  current cache; C7DD0 matches selected handles against the 2FAF80 list). ui.map has that tag (972) and
  "displayed_error_messages" (313). Watch on 35210/C7DD0/C8790/C7070/2E5A0 queued (host_mp7_result).
- 00:20 (09-03) MP ROOT CAUSE FOUND: gametype select handler 0xCD9C0 (list widget, handle = list[sel])
  requires the handle to be NEGATIVE (bit 31) to start the game (calls D59B0); otherwise sets pending
  error id 0x1F = "Unable to load saved game file" (ui.map ustr displayed_error_messages, element =
  {len,0,0,ptr,0} stride 20). Handle = 2D080(index,category,type,flagA,flagB): (index<<16)|(cat<<8)|type,
  bit 30 if record byte +516 == 1 ("default", not deletable: error 0x1A in CD8F0 delete handler), bit 31
  if record byte +517 == 1. Our hdmu.map variant records (written by the game's own default-creation
  path 0x2F450 + 2D440 append) have +516=1, +517=0 -> bit 31 clear -> rejected. Host experiment: copy of
  the save with byte 517 set on the 26 variant rows (host_mp10_result). Fresh-save watch on
  2D440/2F450 with record tail dump (+504..+527) queued (host_fresh_result) to see what the creator
  writes and why +517 stays 0 (maybe set after a step our runtime fails, e.g. XAPI signature).
- 00:45 (09-03) MP FIX (workaround): xk_file.c NtReadFile patches 518-byte hdmu.map records on read:
  type==1 && byte517==0 -> byte517=1 ("valid") so 2EF40 builds handles with bit 31 and CD9C0 accepts
  the gametype. Host: after the press the game now reads playlists 00/01/02 and WRITES lastmpvr.txt
  (choice accepted), then a 398-draw screen (lobby?) - no map load yet with the short script. Why the
  records lack byte 517: the boot-time default-variant path (2FD10 -> 2FE60 file writer, 26 blam.lst
  rewrites every boot, gated by the constant flag [1F668E]=1 in .data) does NOT touch the index; index
  rows for variants were appended once by 2F450 (sets byte 517 only if the 512-byte write succeeded) in
  an early runtime where writes were probably failing; the index is flushed whole from memory at boot
  (30800 -> 508F0 x2) so the 0 persists forever. Proper fix = also repair the file (or delete hdmu.map
  once so 2F450 recreates rows). WriteFile (14F4A) returns 1 with written==size now. No Xc* crypto is
  used by the game at all (no signatures). Vita3K run of the fixed build + longer host script queued
  (mpfix_result / host_fix2_result).

## 2026-09-03 overnight: split screen needs the network stack (loopback sockets, recomp/kernel/xk_net.c)
- Split-screen lobby (SELECT TEAMS) showed "Map: Battle Creek / Searching for available game" with no players:
  Halo runs a network server + client in-process even for split screen. XBE links XNet statically (C++
  object at [0x262A78], thin thunks); game transport code 0x121350..0x122FD0 uses TCP (socket type 1:
  listen 5150 / connect / accept / select / send / recv) and UDP (5150 data, 5151) via sendto/recvfrom.
- HLE'd the *thunk* addresses (regen --hle-addr list, names ws_*/xn_*): 1B102C socket, 1B0D13 bind,
  1B0D1E connect, 1B0D29 accept, 1B0572 listen, 1B14EA send, 1B149E recv, 1B157D recvfrom(!), 1B158C
  sendto(!), 1B0D34 select, 1B01B9 ioctlsocket, 1B0D04 setsockopt, 1B03E2 getsockopt, 1B03FC
  getsockname, 1B03F1 getpeername, 1B1572 closesocket, 1AF688 WSAGetLastError, 1AF3E0 XNetCreateKey,
  1AF420 XNetRegisterKey, 1AF437 XNetUnregisterKey, 1AF44E XNetRandom, 1AF471 XNetXnAddrToInAddr,
  1AF488 XNetGetTitleXnAddr. NOTE: the regen spec names 1B157D "ws_sendto" and 1B158C "ws_recvfrom" but
  the bodies are swapped in xk_net.c (fix the spec at the next regen). 1AF68D (__WSAFDIsSet) stays
  recompiled. XNetGetEthernetLinkStatus must return ACTIVE (0x0B): 0x9D7B0 sets pending error 6 and
  aborts the session otherwise. XV_NET_LOG=1 logs every call with caller.
- Virtual second pad: XV_PAD2=1 -> XGetDeviceChanges reports ports 1+2, XInputOpen port 1 = 0x00777702;
  pad script tokens p2start/p2a/p2b/p2x/p2y/p2up/p2down/p2left/p2right/p2back (parser class is now
  [a-z0-9]); hardware chord L+R+{START,X,O,D-pad} -> player 2.
- With that: Vita3K reaches ENLISTED PLAYERS (Map Blood Gulch, Slayer, FFA, 15 frags, "Waiting for another
  player", players 0) then "A networking error has occurred": server accepts the client TCP connection
  (0x9F820 loop -> 0x1224F0 accept -> 0x9F780 'circ' 32 KB buffer) and closes it right away (investigating).

## 2026-09-03: MP lobby now runs a healthy session; match START still blocked (1-player gate)
- With xk_net.c the split-screen lobby holds a sustained loopback session: server (listen 5150) accepts
  the local client, then a steady bidirectional heartbeat (send 84/recv 84 handshake, 1080-byte state
  push, then 77-byte / 69-byte messages both ways via TCP 2003<->2004 and UDP 2001<->2002). The old
  "A networking error has occurred" is gone on host. Vita3K reaches ENLISTED PLAYERS (Map Blood Gulch,
  Slayer, FFA, 15 frags) "Waiting for another player", Number of Players: 0.
- Match never starts: game never opens d:\maps\bloodgulch.map. Map path builder is 0x33980 (sprintf
  "d:\maps\%s.map" @0x1F7318 -> CreateFile), called by the low-level loader for ALL maps; the lobby
  never reaches it. Scripted START GAME (start/a/x/y held at SELECT TEAMS) does NOT start with 1 player;
  scripted P2 join (XV_PAD2 + p2a pulses / mash across the profile screen) does NOT add a 2nd player.
- d:\hosts.txt (reader 0x17D560) is a DEV auto-connect (fopen, parse lines, 0x173360 addr build,
  0x1213E0 connect) - absence is handled (skips), NOT the blocker. Game also writes z:\last_solo.txt.
- Likely root cause (unconfirmed): the local player is never registered as a game-session member
  ("Number of Players: 0" with a local profile joined), so the min-player/session-ready gate keeps the
  "Game Start In:" countdown from running. Next leads: find where enlisted count is read at the teams
  screen; verify the profile-join actually adds a local player from XInputGetState; the countdown timer
  variable. HARDWARE has ONE controller, so 1-player start (not split 2P) is the only viable path there.

## 2026-09-03: MP start-gate architecture (corrected) - the real blocker is session readiness
- Main game tick 0x56960 (from thread 0x15C5A): -> 0x56A20 (advance sim; guarded by 0x625B0) -> 0xBD420
  runs ONE FRAME of the *current level*. The menus ARE a level: 0xBCE20 loads "levels\ui\ui". So 0xBD420
  is "run current level", not "start a match". NOTE 0x568D0 (called by 0x56A20) is a DEMO-EXPIRY guard
  ("XDEMOS"/"xdemo "/"your build has expired", strings @0x1F8284) that returns 1 - NOT a start gate.
- Starting a match = a MAP SWITCH from levels\ui\ui to the MP map, via 0x1092F0 (map load by name;
  callers 0x9E630/0xBCE20/0xBCF10/0xBD120) or the sibling 0x33CF0 (sprintf d:\maps\%s.map @0x1F7318 ->
  0x33990 open). The lobby's START GAME must set the session's target map and initiate that switch. It
  never fires with 1 local player -> gate is session readiness / min players, still unlocated.
- VERIFIED the input path is fine: XV_PAD2 player 2 IS polled (304 polls in a 170s host run) and the game
  reads A=255 from the poll (caller 0xE3996). So scripted join input reaches the game; the 2nd player
  still does not enlist (Number of Players stays 0). Menus act on button EDGES; even discrete
  press/release p2a pulses at the profile screen did not add a player - the split-screen local-player
  registration (profile-join) needs more RE, OR both virtual pads collapsing to one xk_os_pad poll is
  rejected as a duplicate device. Next: RE the profile-join screen handler (adds a local session player).

## 2026-09-03: MP start is a multi-stage network state machine (full RE) - solo start needs 2 gates forced
- Network stack CONFIRMED working on real hardware (full session send/recv, no "networking error"; the
  earlier hardware error was transient). netstack/diag builds staged in release/. Card image was corrupt
  (VPK bytes in the tail) and was replaced with the clean recomp/halo_image.bin.
- Lobby state machine lives in the game-session object at [sess+0CA6] (u16 state). Handler/advance:
  0xA09D0 = per-message state handler; jumps [state*4 + 0xA0CA0]: state 0/1 -> 0xA0C94 (waiting),
  2 -> 0xA0AE6, 3 -> 0xA0B61, 4 -> 0xA0C02 (each just BROADCASTS the state via 0x173360 build +
  0x1213E0 send, msg subtype eax=0Eh/1Bh/20h). A09D0 sets state=2 on the "begin" message.
- 0xA1240 = THE game-start executor (only caller 0x17E5B0, the "start" message handler via dispatch
  0x17E710 -> trampoline 0x17E0F0/etc.): calls 0x9E630 (broadcast "load map", DEADBEEF msg id 1),
  sets [sess+0CA6]=3, then 0x000D1540 + 0x000FA620 (engine start). So receiving the final "start"
  message is what actually loads the MP map. 0x33CF0/0x1092F0 = load-map-by-name (sprintf d:\maps\%s.map
  @0x1F7318). Host-side heartbeat/lobby ticks: 0xA17F0 (client, [2E362C]), 0xA18A0 (host, [2E3628]),
  both gate on 0x190DC (HLE'd to xv_hle_XNetGetEthernetLinkStatus -> 0x0B active) and set net error 6
  at [2E4028] if link down. Client msg pump: 0xA15B0 (recv 0x9EFF0 / recvfrom 0x9F0C0 -> dispatch 17E710).
- EMPIRICAL (Vita3K): 1 player -> "ENLISTED PLAYERS / Waiting for another player / Number of Players: 1",
  START GAME (A/Cross) never fires A1240 (host never sends start). Virtual 2nd pad (XV_PAD2, tokens p2a
  etc.) DOES enlist a 2nd local player ("Prancer") -> header flips to "Game Start In:" and A09D0 fires
  once (state->2), but A1240 still never fires and bloodgulch never loads (countdown won't complete with a
  non-participating virtual P2). So solo needs BOTH: (1) min-players gate (1<2) to leave WAITING, and
  (2) the countdown->final-start (which waits on 2nd-player ack). Force options for next time: capture the
  session ptr from A17F0/A18A0 (ebx=sess) and either drive A1240 directly on a chord, or inject the start
  message so 17E5B0 runs with proper context; or bypass the lobby and drive 0x1092F0(bloodgulch)+engine
  start (D1540/FA620) directly. Diag knobs added: XV_ERR_LOG (per-frame [2E4028]/[2E4030] dump via
  xv_guest_r16 in xk_net.c), XNetGetEthernetLinkStatus NETLOG re-enabled, P2-poll log in XInputGetState.

## 2026-09-03 09:40: SOLO BLOOD GULCH WORKS (forced start). Commit "Multiplayer: solo Blood Gulch via forced match start".
- XV_FORCE_START=1 + hold L+R+Triangle at ENLISTED PLAYERS (pad.force_start; script token "force") ->
  xv_force_mp_start() in xk_xapi.c: sess=[2E362C], X_PUSH32(sess); X_PUSH32(0); f_000A1240(c). Log
  "[force-start] session 002FB180 state 2 -> calling A1240 / returned, state now 3"; then 1091B0 x2,
  1092F0 x3, sky cube maps + 128x128 RTs + new mesh programs = level rendering. Frames: Warthog,
  red/blue bases, HUD/radar, weapon pickups (AR, shotgun, plasma pistol/rifle). Verified host + Vita3K.
- Chord gotcha: the Vita P2 chord (L+R+...) strips L/R/Cross from P1 BEFORE the XInput HLE sees them,
  so any force chord must be detected inside xk_os_pad_poll (done: Triangle, not claimed by P2 chord).
- Staged release/xboxvita-recomp-2026-09-03-forcestart.vpk; card cfg = XV_VBLANK_KICK=0 + XV_FORCE_START=1
  (debug logs off). Remaining MP polish: over-bright/pale sand terrain, countdown/2P handshake unforced.

## 2026-09-03 10:05: hardware Blood Gulch profile + release build
- Hardware in Blood Gulch: 10.5-13.3 fps (game 74-94 ms, render 1.3-4.4 ms). [prof]: BB060 (vblank-count
  wait loop) 43-51%, 9D7B0 (session tick; calls XNetGetEthernetLinkStatus every frame) 14-17%, B5B40 ~3%,
  88B80 x87 ~2%, 7EDF0 vblank cb ~2%, long tail. [wait] idle 0 ms: t12/t20 event waits busy.
- Emulator is vblank-paced at ~28.6 fps (game 35 ms constant, menu or map, traced or not) -> emulator
  CANNOT measure CPU wins; only hardware can.
- Stripped release regen (no --trace-calls/--trace-funcs): 0 XV_FN, VPK 11.3 MB (was 12.6). Boots +
  force-start OK on Vita3K. NOTE: no profiler attribution in this build (xv_cur_fn never set) - use a
  --trace-funcs regen for profiling. Regen spec still names 1B157D ws_sendto / 1B158C ws_recvfrom
  (bodies swapped in xk_net.c). Also removed the UNCONDITIONAL NETLOG in xk_xapi.c's
  XNetGetEthernetLinkStatus (it logged every session tick to the SD card on hardware).
- Card now: notrace VPK + cfg "XV_FORCE_START=1" only (vblank kick ENABLED). If the gametype screen
  freezes again, add XV_VBLANK_KICK=0 back. Saved hw log: scratchpad/hw_bg_profile.log.

## 2026-09-03 10:30: vblank kick freeze diagnosed; redesigned as yield-to-vblank-thread
- With the INLINE kick (XV_VBLANK_KICK default on), hardware froze at SELECT GAMETYPE again despite the
  yield fix: log = bloodgulch.map preview open -> 5x "vblank kick from 00012E6E" -> no more frames;
  loader thread t16 stuck 48 s on event 832B4038 (1 wait). Cause: running Halo's vblank callback
  (7EDF0) inline on the sleeping game thread while the map-list loader runs. BUT before the freeze the
  menus ran 20-23 fps vs ~10 -> the kick is the biggest perf lever (BB060 wait = 43-51% in-map).
- Redesign (kernel only): xk_thread_kick(t) in xk_thread.c ends t's xk_sleep_us early and sets g_boost
  so pick_next() runs it on the very next switch; xd3d_vblank_kick() now kicks g_vb_thread (saved from
  xk_thread_create_host in SetVerticalBlankCallback) instead of vblank_fire(c) inline. The callback
  therefore always runs on the vblank thread with normal ordering; only the latency is removed. Build
  staged as release/xboxvita-recomp-2026-09-03-kick2.vpk (stripped/untraced). Verification pending
  (Vita3K + host through SELECT GAMETYPE into Blood Gulch). Card currently: notrace + XV_VBLANK_KICK=0.
- 2026-09-03 10:50: a10 intro cinematic now PLAYS THROUGH on Vita3K (bridge, Keyes/Cortana, cryo wake) - the
  "holds forever at first dialogue shot" note is stale (fixed by the DS stream packet-callback pump).
  Hardware already passed it too. First-mission blocker left: hardware "black screen past the door"
  (needs a hardware log). Visible defects to fix (both modes): striped 16-bit textures (hull fly-by,
  Blood Gulch base walls/ground), sky white-out (view-dependent), pale/over-bright sand. Kernel I/O
  completion verified correct (event signal + APC queue wakes alertable waiters, returns STATUS_USER_APC).
- 2026-09-03 10:55 STRIPED TEXTURES ROOT CAUSE: not aliasing. XV_TEXDUMP of the a10 hull fly-by shows the
  R5G6B5 (fmt 05) textures at 02F48000 (512x512) and 02414000 (256x256) decode to garbage while DXT ones
  decode fine; their raw bytes have only 52-191 distinct u16 per 64 KB (not pixel data; not valid DXT
  either) = stale/unstreamed memory. Halo streams bitmap pixels into the game heap asynchronously and
  binds before the read lands; the GXM texture cache (xv_ui_gxm.c) snapshot the first bind and only
  re-hashed "dynamic" textures (hdr >= 0x03D00000). FIX: re-validate all textures - hash source level 0
  at insert (ui_tex_hash strides to <=8192 dwords), re-check dynamic every frame and map textures on a
  1-in-16 rotating cadence until 24 unchanged checks (ui_tex_entry.stable), invalidate+re-decode on
  change ("tex %08X fmt %02X changed (check N): re-decoding" log). NOTE g.texcount is the append index
  (never decrement it). Same defect = Blood Gulch base walls/ground stripes on hardware.
- 2026-09-03 11:15: texture re-validation caused a PURGE REGRESSION (0 -> 30-41 purges/run: every re-decode
  appended to the 32 MB bump pool). Fixed with IN-PLACE refill in xv_ui_gxm.c: `ui_tex_entry *re` marks a
  changed entry; the decode reuses e and sceGxmTextureGetData(&e->tex) as dst, skips the capacity check,
  dec_off bump, texcount++ and hash-chain insert. Verified: purges 0, 12+ in-place re-decodes, no errors.
- The fmt-05 "garbage" textures are NOT stale: offline, the first 32 KB of the 512x512 dump decode as a
  clean 565 128x128 (engine glow/flare gradient) and the 256x256's first 8 KB as a plausible 64x64 -> the
  RESIDENT data is two mip levels below the header size (Halo's texture cache drops the top mips and points
  Data at the first resident level). SetTextureStageStateNotInline is NEVER called (inline path stores
  into D3D__TextureState @0x18F180 [stage*32+type]); reading that at bind (SetTexture HLE diag) to find the
  MAXMIPLEVEL slot (expected 7, value 2). Plan: cache key fmtword|(lod<<28); decode w>>lod, h>>lod, mips-lod.
- 2026-09-03 11:45 LIGHTMAP CONCLUSION: the fmt-05 textures are BSP lightmaps (levels\a10\a10a..g, a10_space
  512x512, x10hangar; R5G6B5 swizzled mips=0, bound on stages 1/2). Runtime bytes == map file bytes
  (100%). Decoded per 32 KB chunk as 128x128 Morton tiles: chunks 0-5 are clean and tile together -> our
  Morton decode is CORRECT; the rest of each page is unused junk the tools never zeroed (no surface
  samples it). NOT the cause of the hull/base-wall streaks. Remaining suspect for streaks: BC (DXT)
  textures uploaded with a single mip level (XV_BC_MIPS default 0 after a suspected GPU fault) ->
  aliasing at grazing angles. Test XV_BC_MIPS=1 (env knob, no rebuild). MAXMIPLEVEL is 0 (not used).
  Committed re-validation + in-place refill ("GXM texture cache: re-validate streamed map textures").
- 2026-09-03 12:05: XV_BC_MIPS=1 runs clean on Vita3K (a10 + Blood Gulch, no textureInitSwizzled failures)
  but the hull fly-by is STILL striped -> the streaks are neither texture data, decode, nor mip aliasing.
  Fine 1-px vertical stripes over a huge close surface = texture COORDINATES nearly constant on one axis
  (texture-transform / vertex-shader texcoord path), i.e. a shader-side bug. Sky white-out + pale sand are
  lighting/combiner issues (also shader-side). Next: XV_D3D_HIST on a fly-by frame to find the hull draw's
  vertex program + texcoord setup. Hardware kick2 (yield-to-vblank) result still unreported by the user.
- 2026-09-03 12:25 STRIPES ROOT CAUSE: XV_D3D_HIST=1900 (a10 fly-by) showed SetVertexShaderConstant(reg -79 =
  hw c[17], n 11) from 0x7E51D carrying -nan in .z. halo_vs_47: c[17]/c[18] are the texcoord-1 transform rows
  (dph oT1.x, v4, c[17]) -> oT1.x NaN, oT1.y varies -> 2nd stage sampled along one column = fine vertical
  stripes. Builder 0x7DBE0: [ecx+0Ch] = 1.0 / (x*x) with x=[eax+34h] (texture animation scale) = 0 for a
  static stage -> inf/NaN. Xbox is fine because NV2A vertex ALU: 0*(inf|NaN)=0; GXM is IEEE. FIX: sanitize
  non-finite vs constants to 0 at upload (xk kernel xd3d.c SetVertexShaderConstant; log "vs const c[N].k
  non-finite"). Likely also behind white sky / pale sand if those constants go non-finite (verify).
- 2026-09-03 12:50 STRIPES still present after the NaN sanitizer AND with XV_FS_FORCE=tex0. Fly-by frame
  1900 structure: the big surface (n=390 prims) is drawn in 4 passes: halo_vs_16 (tex 0B P8 bump 512,
  0C DXT1 base 512, 05 lightmap 512, 07 64x64; blend 5/6), halo_vs_40 (0F DXT5 512 + 4x4s; blend 9/1),
  halo_vs_58 (05 lightmap 512, 05 256, 64x64s; blend 7/1; only v0+v8 bound - v4 color1 legitimately
  unused), halo_vs_26. BSP vertex layout: stream0 32 B = v0 pos FLOAT3 @0, v1 normal NORMPACKED3 @12,
  v2 binormal @16, v3 tangent @20, v4 TEXCOORD FLOAT2 @24 (named color1); stream1 8 B = v7 NORMPACKED3
  @0, v8 lightmap UV NORMSHORT2 @4 (named backcolor1). Pillar of Autumn model + Blood Gulch terrain UVs
  are fine -> not a general decl bug. Remaining hypothesis: our combiner->fragment program for this
  material samples a stage the NV2A combiner ignores (Xbox also computes oT1.x==0 here yet renders fine).
  Next: dump the ps def (psdef hash 953BD9C7 / F1F97FC3 / 91A49E77) and compare which stages the
  combiner references vs what the generated .frag.cg samples. Committed the NaN sanitizer.
- 2026-09-03 13:20 STRIPES - state of the hunt (all INPUTS verified correct): BSP vertex stream0 32 B /
  stream1 8 B strides match the decl; v4 UVs are normal 0..1 (hist XV_DUMP_VS=halo_vs_16); all 4 textures
  of the main pass decode right (P8 bump maps clean, DXT base clean, lightmap clean in used region);
  XV_FS_FORCE=tex0 (3D path, xv_d3d.c:599) still stripes; BC mips don't matter; sampler addr = REPEAT;
  vs constants: halo_vs_16 [12] mul oT0.xy, r5, c[12].xyyy / [13] oT1.xy = r5*c[12].z, and the game
  INTENTIONALLY zeroes c[12] (0x676C0: mov [esp+120h..134h],0 then SetVertexShaderConstant(-84,n3) @0x677BB;
  also 0x68147) for this material path -> so Xbox also has oT0/oT1 == 0 for that pass and must not be
  sampling those stages. Our generated frag (ps_953BD9C7_3D.frag.cg) samples t0..t3 unconditionally and
  its combiner math references t0/t1 -> NEXT: decode the psdef PSTextureModes / PSInputTexture for hash
  953BD9C7 (and F1F97FC3, 91A49E77 = other passes of the same surface) and check which stages the NV2A
  combiner truly consumes; also 0x67640 shows Halo's x87 clamp idiom (fcom; fnstsw ax; test ah,5; jp /
  test ah,41h; jne) - worth a unit test of our fcom/fnstsw/SAHF flag translation. Staged build:
  release/xboxvita-recomp-2026-09-03-texfix.vpk (refill + NaN sanitizer + kick2).

## 2026-09-03 13:05 HARDWARE: Warthog driven on the Vita. Kick2 verified. Direction = make Blood Gulch 100%.
- User drove the Warthog in Blood Gulch on hardware (texfix build: stripped + yield-to-vblank kick ON +
  texture re-validation + NaN sanitizer; cfg XV_FORCE_START=1). NO gametype-screen freeze -> kick2 is safe.
- In-map fps avg 12.9 (min 7.9, max 16.2) vs 10.5-13.3 with kick off; game 60-90 ms/frame (CPU-bound
  recompiled code), render 1.5-12 ms with spikes when textures stream in (CPU decode+upload on the
  render thread). Lobby (ENLISTED PLAYERS, network heartbeat) is very slow: 5.7-10 fps, 175 ms frames.
  [wait]: t20 event waits 174x (~67 ms each), t12 event 16x - worker threads busy each frame.
- User direction: focus on Blood Gulch until 100% playable (perf + sky white-out + pale sand + striped
  base walls). Next perf levers: flat guest memory (X_G page-table lookup removal), x87 fast paths, wider
  fusion, -O3/ARM mode; texture decode off the render thread. Need a traced profile build (kick ON) for a
  fresh in-map profile (BB060 gone -> what dominates now?).
- 2026-09-03 13:15: card now has release/xboxvita-recomp-2026-09-03-profile.vpk (TRACED regen, kick ON,
  texfix, cfg XV_FORCE_START=1 + XV_PROF=1) for a fresh in-map Blood Gulch [prof]. Hardware shots 12:58-13:00:
  sky mostly correct (planet/stars), Warthog fully rendered, base interior clean (wall stripes likely were
  the stale-texture bug), a whitish haze in one view (A8 haze layer, cmd 5 of frame 2300: 19:256x256).
  Blood Gulch frame 2300 draw list: sky = first 5 depth-off halo_vs_47 draws (0E 512 DXT3 dome, 0C/06
  clouds, A8 haze); then halo_vs_09/27/16 BSP passes. Emulator always-white sky is likely a Vita3K quirk.
  Committed e952ddd: per-frame texture decode count/time in the frame-time log (for the render spikes).
  Next: read the profile -> choose between flat-memory refactor (needs image aliasing: Halo allocs phys
  0x61000.. overlapping the image VA; use arithmetic remap of phys aliases inside [IMG_LO,IMG_HI)), x87
  fast paths, wider fusion, decode-off-render-thread.
- 2026-09-03 16:30 HARDWARE REPORT: Warthog wheels+turret missing, some 1st-person weapons (AR, plasma
  rifle) missing, far terrain missing ("can't see far"), no Scorpion/Ghost. Findings: (a) hardware log
  had "[pspair] no program for vs X ps Y: fallback texmod" for 4 material combos AND 97 ps_*.frag.cg
  had never been compiled (400 .gxp vs 497 sources) -> ran tools/ps_pipeline.py over all logs +
  `tools/vita3k.sh shaders shaders` (compiles .cg -> .gxp inside Vita3K; the "Trace/breakpoint trap" at
  the end is normal) -> 499 programs, shaders/xv_ps_table.h regenerated. New combos keep surfacing per
  run (iterative: rerun pipeline on new logs). (b) FOG COLOUR was never tracked: xv_d3d.c uploaded
  xv_fogcolor=(0,0,0,0) -> vertex fog faded distant geometry to BLACK. Added rs_method case 0x2A8
  (NV097_SET_FOG_COLOR) -> xd3d_state.fog_color -> xd3d_fog_color() -> ARGB floats (verify channel order
  from the "fog color %08X" log vs Blood Gulch's bluish haze). Build staged release/…-fog.vpk.
- 2026-09-03 17:10 FOG ROOT CAUSE: Halo never uses the generic render-state table for fog (no 0x2A0-0x2B8
  entries in it); it calls the inline helper D3DDevice_SetRenderState_FogColor (0x1821E0), which was an
  RS1() no-op stub in xd3d.c -> fog colour 0 -> distant terrain faded to black ("can't see far").
  Now stored in xd3d_state.fog_color; Blood Gulch sets 00FFE6C4 (warm sand haze, ARGB with A=0) and
  toggles to 00000000 within the frame, so xv_d3d.c captures it PER DRAW at record time (cmd.fog_color)
  rather than reading it at replay. Ruled out for missing wheels/turret/AR/plasma rifle: NaN constants
  (no non-finite reports in Blood Gulch runs), constant-window (skinned vs use full -96..95 window),
  arl floor rounding of PBYTE2 node indices (float32 exact for k/255*765). Emulator renders the
  plasma rifle fine; hardware log had 4 missing fragment programs -> most likely those parts. Blood
  Gulch netgame equipment has NO vehicle entries (weapons/powerups only); vehicles come from scenario
  placements gated by the variant's vehicle set -> need reference to know if Scorpion/Ghost expected.
- 2026-09-03 17:40 VERIFIED on Vita3K (release/xboxvita-recomp-2026-09-03-fog3.vpk, commit after e952ddd):
  Blood Gulch far terrain fades to sand haze, 0 "no program" fallbacks, Ghosts + Warthog spawn at the
  base. Pending hardware check of wheels/turret/AR/plasma rifle with the 4 previously-missing programs.
- The 4 hardware "no program" pairs: 3 with vs AC1984DB = halo_vs_47 (SKINNED, a0-indexed node matrices)
  + 1 with vs_01 -> the skinned model parts (wheels/turret/1st-person weapons) drew via the texmod
  fallback on hardware. Emulator never hit them (different weapons picked up). Fixed in fog3 build.
- XV_BC_MIPS=1 on Vita3K (Blood Gulch): distant sand loses the coarse speckle (mips-off aliasing), no
  faults; but close-up ground/base-wall views show large blocky diagonal streaks (low mip sampled at
  close range, or per-level layout) - undecided, cannot A/B with random spawns. Default stays 0;
  hardware test via cfg knob (no rebuild). Square-only BC path; non-square DXT decodes at capped mip.
- 2026-09-03 17:49 ROOT CAUSE of mangled/missing Warthog wheels+turret, giant garbage rocket launcher,
  broken main menu (commit 111ccb1): xv_d3d.c stream setup did xv_guest_ptr(vb->Data) with Data = a
  PHYSICAL address (map-resident vertex objects live in the tag region, e.g. phys 0x005A5A00). As a bare
  low VA that hits Halo's heap (NtAllocateVirtualMemory from 0x00465000 up) -> zeros / live heap bytes
  (bytes changed per frame). Fix: xv_guest_ptr(0x80000000u | Data) (texture path already did). D3D-created
  VBs (arms, weapons held, BSP) sat at high phys -> unaffected, which is why "some things render fine".
  Debug method that found it: XV_SKIN_DUMP=1 (per-signature node-byte/matrix dump, vb_1372.bin dump)
  + compare with the map (Xbox 'mode' tag: parts 0x68 B, vertex count @+0x58, VB object VA @+0x64;
  vertex-object table = index header +0x14, 359 x 12 B, Data VA at +4; compressed vertex 32 B: node0
  byte@28 = node*3, node1@29 (253 = none), weight s16@30; Halo cache files z:\cacheNNN.map = decompressed
  maps, cache004 = bloodgulch, identical to HaloMap output). Also: a0-relative c[] reads must be clamped
  (node1 = 253 -> c[313] out of range -> NaN * 0 flung vertices); c7.w = 255.9375 so floor is exact.
  Fog: deferred rs 82=FOGENABLE(1), 92=LIGHTING(0), 93=SPECULARENABLE(1); no FOGTABLEMODE/START/END ->
  vertex fog pass-through (fog.a = saturate(oFog.x)); ramp still far too heavy on Blood Gulch (sky reads
  as sand) -> next: compare with the sky tag's fog params / c[12] values.
- 2026-09-03 18:55 "MISSING MAP CHUNKS" ROOT CAUSE (release/…-skyclip.vpk): the pale razor-edged regions
  were exactly the CLEAR colour (FFE6C4) = nothing drawn: Halo draws the SKY dome with clip z == w (on
  the far plane; hist "zoff clip" dump showed z/w = 1.00 for every sky vertex). NV2A keeps z == w, GXM
  clips z > w, so rounding dropped whole sky triangles -> polygon holes that "load" as you move. Fix:
  every vertex program's epilogue emits OUT.position = float4(oPos.xy, oPos.z * 0.9999, oPos.w)
  (shader_recomp_gen.py + sed over the 68 shipped .cg, recompiled). Ruled out along the way: cluster
  culling (draw counts vary legitimately), far plane (proj near 0.063 / far 1024), fog pass (vs_06 +
  ps B5DD2FEE: density = 0.4*u, ramp tex fmt 01 linear, c8 = 1/97 per unit: all correct), texture
  streaming (terrain bitmaps DO load; the 4x4 fmt 07 placeholders on stages 1-2 are Halo's 'default 2d'
  for faded detail maps). Also fixed the same day: IB Data physical alias (xv_ui_gxm.c draw path).
  Debug knobs added: XV_SKIN_DUMP, XV_FOG_DUMP, ux0:data/xboxvita/hist.now (on-demand XV_D3D_HIST),
  XV_LOG_READS=<substr> now prints buf + a stack for the first big reads, draws/frame + bsp in the
  frame-time log, [hist] drawcall stacks + tiny-texture raw bytes + zoff clip coords.
- 2026-09-03 21:00 MISSING TERRAIN/BASE CHUNKS = Halo's portal visibility, not fog/LOD/textures. Chain
  (3925): frame render f_0005D410 -> f_000539C0 (resets visible list count @0x38BE10, list @0x38BE14)
  -> f_00053540 -> f_000532E0 = RECURSIVE cluster/portal walker (root = camera cluster from leaf lookup
  f_0017A8B0 + leaf table [[0x39BE58]+0xE4] entry 16B, cluster i16 @+8). Per portal: f_00053280 wraps
  f_00052BE0 (classify portal polygon vs frustum @0x2FEB90: 0 = needs clipping, 1 = OUTSIDE, 2 = fully
  inside); side byte = (front cluster == current) -> sign of camera-plane distance. Fully-inside portals
  recurse with the parent frustum; partial ones go through f_000B7F10 (clip polygon f_000B71C0 /
  f_000B7B40, normalize f_000117B0) which RETURNS 0 ("clipped away") for nearly all partially visible
  portals -> traversal stops after 1 level (4 clusters/frame in Blood Gulch). XV_VIS_ALL=1 (patch in
  code_009.c at the f_00053280 call: force result 2 = flood fill) renders the WHOLE canyon correctly ->
  geometry intact. Counters: xv_dbg_count[] in frame-time log ("vis2 32E0=... B7F10=..."). c20 docs:
  debug flags structures_use_pvs_for_vs / debug_no_frustum_clip exist in the engine (globals) - the
  flood fill mimics debug_no_frustum_clip. NOTE code_*.c patches are not tracked: the permanent fix must
  go through xbe_recomp.py (only --hle-addr exists) or be the real math fix in f_000B7xxx.
- 2026-09-03 22:20 ROOT CAUSE of the culled map chunks (commit after fba4e3e, release/…-bspvis.vpk):
  xk_mem.c virt_commit assigned physical pages TOP-DOWN one page at a time -> consecutive virtual pages
  (heap, thread stacks) mapped to DESCENDING arena pages. Anything copying a block across a 4 KB boundary
  with a host-contiguous pointer (x_str_movs fast-path memmove, HLE bulk readers/NtReadFile) scrambled the
  tail. Halo's portal classifier f_00052BE0 copies the camera-space polygon in place (stack buffer at
  0x451FFC crossing 0x452000) -> polygon emptied -> "outside" -> walker f_000532E0 never recursed past
  fully-visible portals -> only 4-6 pieces drawn. Fix: virt_commit backs each commit with one ascending
  contiguous run (fallback to per-page). Making rep movs page-correct ALONE broke boot (disc error): the
  HLE readers were consistently wrong the same way, so both sides had to agree - keep host-contiguous
  fast paths, guarantee contiguity instead. Result: 16-17 clusters/frame, full canyon, culling intact.
  Lesson: never trust X_G(ptr)+len across pages unless the region is a contiguous commit.
- 2026-09-03 22:40 HARDWARE: bspvis build on the Vita = Blood Gulch fully rendered (both walls, bases,
  Warthog w/ wheels+turret, weapons, effects, ring in sky); user: "works great". 9-11 fps -> 11-14 with
  cfg XV_VBLANK_HZ=240 (driving 8). Hardware frame: game 88-118 ms, render 3 ms, texture re-decode
  13-16 tex/s (~2 ms/frame). Hardware-only: sky renders DARK (emulator: daylight blue) - unexplained.
  Profile build pushed (release/…-bspvis-profile.vpk, cfg XV_PROF=1) - awaiting the user's run.
  OPEN after the virt_commit change: a10 cutscene stuck (director camera fixed ~4 min, loop alive,
  BB060 65%), cryo-bay green banding on walls (vs_58 light pass c0=(0.5,0.69,0.31); lightmap pass binds
  0E:512x256). Not yet A/B'd against the afternoon build (release/…-fog.vpk).
- 2026-09-03 23:05 PERF PROFILE (hardware, 13 in-map windows, 9.8 fps, XV_VBLANK_HZ=240): BB060 vblank
  wait loop 35%, our D3D HLE via Halo's thin wrappers ~26% (7EDF0 Present path 10%, 7A130/7A3D0/7A2F0
  draw wrappers ~13%, 7E5D0 state 3%), recompiled game logic ~40% FLAT (top fn 3%). => attack the HLE
  layer + the wait first, not individual game functions. Sleep(1) in BB060 is turned into a yield by the
  vblank kick (KeDelayExecutionThread) so the game thread SPINS until the counter hits its target.
  Added per-frame timers (kicks, fires, vbcb ms, draw-hle ms, present ms) to the frame-time log
  (release/…-profile2.vpk; card copy NOT verified - I/O error on remount, re-copy needed).
  a10 scripted run of the current build: campaign resumes from checkpoint (cryo bay), renders clean
  (no green banding) -> the user's banding is state-dependent (after Blood Gulch?). Cutscene "bug" still
  undescribed by the user; "director on" with a fixed cam in the cryo bay is the tutorial's scripted shot.
- 2026-09-03 23:05 release/…-profile2.vpk on the card (md5 verified; cfg XV_FORCE_START=1 XV_PROF=1
  XV_VBLANK_HZ=1000): frame log now prints per-frame "kicks N fires N vbcb ms draw-hle ms present ms".
  Texture cache purge on map load committed (42f198e). Awaiting: the user's hardware run with those
  timers (decides HLE vs wait vs game-logic split), cutscene-bug description, banding-after-Blood-Gulch
  check. Emulator sanity: draw-hle 0.3 ms, present 0.1 ms/frame at 18 draws (Vita hardware will differ).
- 2026-09-03 23:25 PERF on hardware: XV_VBLANK_HZ=1000 -> 16-20 fps walking / 12-13 driving (was 9-11).
  Timers: draw-hle 6-11 ms/frame (~70 us/draw: 3 dcache-clean syscalls per draw), present 1-10 ms,
  vblank callback ~0. Profile still BB060 28.6%: Halo's wait is a BUSY loop (sleep flag [0x2E2CE0]=0,
  kicks 0) that only lets the vblank thread fire at cooperative yields every 200k back-edges (~10 ms).
  perf1 build (commit after 42f198e): merged dcache cleans once per frame at Present
  (xv_gpu_flush_pending), preempt slice 20000 (XV_PREEMPT_SLICE). GXM has NO fixed-function alpha test
  (discard only); clocks already 444/222/222/166 (main.c:1063); GPU submit ~3 ms -> CPU-bound.

## 2026-09-04 a10 "camera didn't go back to Chief" (cryo-tube exit)
- It is the CRYO tutorial, not the bridge: a10.hsc `tutorial_action` (press X) -> fade_out, cinematic_start, camera_control true, camera_set tutorial_action_2/1 (low third-person shot of the Chief in the tube = the stuck view), unit_exit_vehicle player0, sleep 170, fade_out, sleep 35, object_teleport player0 tutorial_exit_cryotube_flag, camera_control false, cinematic_stop. Script source: github Dwood15/HaloScripts a10.hsc (scratch copy $S/a10.hsc).
- hs evaluators (XDK 3925): camera_set F33E0, cinematic_stop F3770 (impl 11A8E0), camera_control F2B40 (impl 11FFD0: flag byte [[2FA31C]], director @0x271100 vtable [0x271100] = 120A90 scripted / 11E750 first person / 11DF50 seat camera, chooser 11F6C0(unit) returns 1 when parent seat forces a camera), object_destroy F2850 (impl 8DC60), unit_exit_vehicle F37E0 (impl 428F0), object_teleport F3E20, fade_out EF340, fade_in EF3C0, sleep E62A0, sleep_until E6070, cinematic_skip_start_internal ED250. hs function table entries: 4 bytes name ptr, +4 parse, +8 evaluate.
- Byte 0x2331D9 is NOT "director on": camera_control sets it to 1 on both true/false (a "camera changed" flag). Frame-stats `act` = [[0x276794]] player action bits. XV_WATCH_FN=<hex,hex> logs enter args (+leave via XV_FN_BACK).
- Checkpoint resume (pad_campaign.txt 500:a,700:a,900:a,1100:a) lands at "Reveille" cryo tutorial; pad.txt units ~30-35 polls/s; buttons a/b/x/y/l/r, sticks lup/ldown/lleft/lright/rup/rdown/rleft/rright, `*N` hold.
- Director/observer (3925): director_update f_00120160(dt) per player: dir @0x2710F8+idx*0xF8 (+8 mode fn, +4 transition secs, +0x54 seat mode, +0xC4 speed), mode fns 11E750 first person (builds cmd via 11E480: flags bit0 valid, +4 pos, +0x20 fov), 11DF50 seat, 120A90 scripted, 120730 dead, 11F230/11E310 debug cams; command copied to observer cmd @0x271150 (+0 flags, +4 pos, +0x60 transition, +0x64 mode). observer_update f_0011DD30(dt) dt=[0x2E36BE ticks]*[0x2E3680 s/tick] @0x2714D8, state @0x2714DC+idx*0x29C. camera_control impl 11FFD0, chooser 11F6C0, per-tick mode refresh 11FD50/11FE60. Frame stats now print "director:"/"director2:" lines with all of it.
- Emulator checkpoint resume of a10 (2026-09-04): after the tube-exit shot the observer cmd is a valid first-person camera at the exit flag (-55.6,-3.8,0.62) but the player unit ignores look/move input (also during the look tutorial: camera fwd never changes though player_action_test passes). On hardware (new game) the user could look/walk but saw the Chief biped from outside. Not yet explained; pad recorder added to replay a real playthrough (XV_PAD_REC=1 -> ux0:data/xboxvita/pad_rec.txt; copy to pad_play.txt to replay; per-frame index).
- 2026-09-04 black contour bands on Blood Gulch cliffs/base floors (hardware AND emulator, not mips): Halo's bump-lighting pass (ps 59FF9FF7, vs_16) dots expand(bump t0) with expand(texCUBE t3 = normalisation cube of the tangent-space light vector oT3) and writes lightmap*lighting opaquely; our cube-map face upload (xv_ui_gxm.c cube path: 6 faces twiddled in Xbox order, no orientation fix) does not match GXM's cube convention, so the light vector came back wrong -> strata shaded from the wrong side. Fix: pixelshader_recomp_gen.py is_normalisation_cube(): cubes read only as expand_normal operands of a dot get `normalize(oT).xyz*0.5+0.5` (18 programs). Reflection cubes (colour uses) still go through texCUBE with the unverified orientation - probable cause of the Warthog hood / base wall stripes; needs a synthetic-face test to fix the upload.
- XV_FS_FORCE=tex0|lm|texmod|color debug views; tools/vita3k.sh shaders <dir> compiles every .cg in <dir> and pulls the .gxp back into it (run it on shaders/ directly); tools/ps_pipeline.py regenerates all .cg + xv_ps_table.h from shaders/psdefs.
- CORRECTION (2026-09-04 10:40): the cube-map bug is the GXM memory LAYOUT, not face order/orientation. Halo's cube data decodes to the standard RenderMan/D3D/GL face order+orientation (verified with the normalisation cube dump cube_02D54000_07: face corners match the table exactly; Vita3K: "GXM's cube map index is same as OpenGL"). GXM expects each face laid out with the FULL mip chain to 1x1 (space reserved even if mip_count < full) and faces 2 KB aligned (32bpp, >=16x16); only mip_count 0xF (InitCube mipCount=0) packs faces tightly. We packed level-0 faces back to back -> faces 1..5 fetched from wrong offsets. Fix in xv_ui_gxm.c cube path: full box-filtered chain per face, 2 KB face stride, InitCube(levels) + mip filter. Vita3K source (sparse clone in $S/vita3k_src): vita3k/renderer/src/texture/cache.cpp ~L353-612, modules/SceGxm/SceGxm.cpp init_texture_base L1483 (mip_count = min(15, mipCount-1)).
- a10 cryo-bay green scan-lines after the intro (fresh game only; checkpoint resume is clean): the texture cache stops re-hashing map textures after 24 stable checks, and Halo reuses its bitmap streaming slots - after the x10 cinematic the cryo-bay bitmaps land where Cortana's hologram texture was -> stale entry served. Fix: xk_file.c calls xv_ui_gxm_invalidate_range(buf, got) after every NtReadFile; overlapping cache entries get dirty=1 and re-hash at next bind; cube entries now carry bytes/sum too (re path drops the old entry). Also: pad.txt loader was capped at 1 KB / 64 events (silently truncated long scripts -> "pad script: 7 events"); now 16 KB / 512.
- Scripted emulator runs without touching the real save: second pref path ~/.local/share/Vita3K/Vita3K-scripted (haloce symlinked, profile copied, no savegame.bin) + cfg/config.yml with pref-path; run with VITA3K_PREF=<p2> VITA3K_ARGS="-c <p2>/cfg/config.yml" tools/vita3k.sh run ... ; fresh-game pad path 500:a,700:a,...,1700:a (extra a's harmless).
- Green cryo bay RESOLVED (2026-09-04 12:xx): the per-read overlap invalidation was not enough; purging the whole texture cache on any read > 1 MB (xk_file.c, was only for the 0x803A6000 tag read) fixed it - a BSP switch streams several MB into the region the previous BSP's lightmaps occupied. Verified in a fresh-game emulator run (frames_new7). Build: release/xboxvita-recomp-2026-09-04-cryofix.vpk (has cube fix + cache fixes + pad recorder); card still holds perf1 - deploy this next with XV_BC_MIPS=1 in xboxvita.cfg.
- Fresh-game emulator run: after the tube exit the player CAN walk/look (unlike checkpoint resume); user's hardware symptom (camera left at the exit shot) still unreproduced. Budget note: user has ~15% weekly quota until Sunday; keep runs minimal.
- BLACK LINES/TILES ROOT CAUSE (2026-09-04 13:xx, commit 2ed84e5): GXM LINEAR textures pad each mip level's row width to 8 texels (Vita3K cache.cpp align_width=8 for SCE_GXM_TEXTURE_LINEAR). Our RGBA path packed levels with stride w, so 4x4 stage dummies (Halo binds 0xFF808080 4x4 "neutral" textures on detail stages) and every chain's 4/2/1-wide tail levels were read from the NEXT cache entry's pixels. The environment base pass (vs_40 + ps F1F97FC3: 2*base*lerp(t2,t1,t2.a)*2*t3, blend DESTCOLOR/ZERO over the opaque lighting passes) multiplied by that junk -> black floor tiles / wall bands in a10, blotchy ceilings on hardware, and the residual thin strata lines on Blood Gulch cliffs. Fix: LIN_PAD(w) strides for every level (+ row spread for w<8). Verified a10 cryo bay + Blood Gulch clean on the emulator.
- Diagnostics that found it: XV_SKIP_PS=<hash,..> (drop draws by combiner hash) showed the pass; a hand-edited .cg with t1/t2 forced grey isolated the stages; hist "textures:" line now prints the GXM-side [gWxH dPTR tTYPE] per stage. Also fixed on the way: cube_fallback layout (16x16, 2 KB faces), NaN-safe analytic normalise (zero incident light), no generated mips for single-level bitmaps, tiny textures re-hashed every frame, offscreen render-target clears/draws dropped (xd3d_offscreen_rt).
- Halo Xbox environment pass order per frame: vs_09 (8-stage "generic" env, props) and vs_16 (bump: 59FF9FF7/32F4D803 lightmap*bump, 953BD9C7 specular) all opaque; then offscreen RT pass (SetRenderTarget 03D05800, Clear F0 = colour only; D3DCLEAR_TARGET_R/G/B/A = 0x10..0x80); then vs_40 F1F97FC3 base*detail modulate (blend 9/1 DESTCOLOR/ZERO); then decals/effects/HUD. rs methods: 0x304 blend enable, 0x344/0x348 factors (GL enums), 0x358 colour mask (bits 0/8/16/24 = B/G/R/A).
- REBRAND (2026-09-04 13:xx): project renamed. Runtime = "Xita": Makefile PROJECT xita (RECOMP=1) -> xita.vpk, TITLE_ID XITA00001, TITLE Xita; skeleton build = xita-skel. Recompiler = "xita-recomp": xita_recomp.py (was xbe_recomp.py); doc xita-architecture.txt. Data dir is now ux0:data/xita (xita.cfg, xita.log, pad.txt, env.txt, haloce/, save/); xv_log.c and tools/shadercomp/main.c migrate ux0:data/xboxvita -> ux0:data/xita once at first run (rename; xboxvita.cfg -> xita.cfg; nothing deleted). Emulator: install with tools/vita3k.sh install xita.vpk XITA00001, run XITA00001; the Vita3K main pref already migrated; Vita3K-scripted pref migrates on its first run. Card: the old XVIT00001 bubble stays until the user installs xita.vpk; the card's ux0:data/xboxvita will be renamed by the first Xita launch. Remember to rm build/param.sfo when TITLE/TITLE_ID change (not a make dependency). xv_ prefixes, XV_* knobs and [xk]/[xv] log tags were left as-is (not legacy names).

**Game clock = vblank count (2026-09-04).** Halo's vblank callback (3925: 0xBB4E0) adds 1 per call to the 64-bit counter at 0x1F8C80; the frame-end fn 0xBB060 converts vblanks-elapsed into the frame's seconds (0x2E3680 = n/60, clamped) and waits for the target set at the previous frame end (0x2E3660). So `XV_VBLANK_HZ=1000` and the early "kick" (fabricated vblank when the game waits) made the game run FAST (2x at 60 fps on Vita3K, "20 fps feel" at 10 fps on hardware). Fix (commit e90aeaf): vblank thread fires exactly 60/real second, catching up after stalls, never ahead. Card cfg still had XV_VBLANK_HZ=1000 - remove it when the Vita is plugged in. The frame-pacing roadmap item is now "honest vblank clock"; a real 30 fps pacing loop is still open.
**Side effect for replay determinism**: recordings made with HZ=1000 have dt pinned at the clamp max every frame -> frame-indexed replay (pad_play.txt) is deterministic; with the honest clock the sim dt varies with real frame time, so replays of movement will drift. Recording of the user's cutscene-skip run: scratchpad `rec_cutscene_skip_20260904.txt` (29k frames, ~16 min; made on build 2026-09-04c-pipelined with HZ=1000).
**Codex branches (2026-09-04, all pushed)**: `psdef-hash` (canonical combiner hash 571->191 programs; verified offline by tools/psdef_verify.py; NOT merged - needs emulator check + gxp rename via tools/psdefs_rename_gxp.sh), `input-touch` (merged: rear halves Black/White, front corners L3/R3, XV_DEADZONE/LOOK_SENS/LOOK_CURVE/INVERT_Y), `adhoc-test` (merged: tools/adhoctest VPK), `dashboard` (merged: dashboard/ software-rendered Xbox-style home screen, standalone xita_dash.vpk + host PPM test). Codex cannot commit inside worktrees (read-only .git) - commit for it. LiveArea icon: the Vita drops icon0.png with an alpha channel -> keep it 8-bit RGB.

**Cutscene-skip camera bug — mechanism found (2026-09-04).** Reproduced from the user's faithful recording. The director's mode vtable pointer lives at guest 0x271100 (values: 0x11E750 first-person, 0x11DF50 seat, 0x120A90 scripted). The camera_control script command is f_0011FFD0 (code_025.c): arg!=0 writes vt=0x120A90 (scripted) + sets flag byte at [*0x2FA31C] and 0x2331D9=1; arg==0 re-derives vt to 0x11E750/0x11DF50 (first-person/seat). After a cutscene SKIP the log shows vt stuck at 0x120A90 with camera_control byte (ctl, =X_M8([0x2FA31C])) still 1, while the "camera_control invoked" flag 0x2331D9 has gone back to 0 ("director on 0"). So the skip clears 0x2331D9 but never runs camera_control(0), so the director mode pointer is never reset out of scripted -> camera stays at the cutscene shot (Keyes/Chief). Normal (unskipped) cutscene end DOES restore first-person. Leading hypothesis for why the closing camera_control(off) doesn't run on skip: the cinematic's end script waits on dialogue completion and DSoundVoiceIsPlaying returns a constant (known audio-desync issue) so the skip fast-forward stalls before the restore line — NOT yet confirmed. director_update = f_00120160 (code_025.c, large). Fix not yet made. Diagnostic fields printed by xd3d.c ~L360-376: vt@0x271100, ctl=X_M8([0x2FA31C]), 0x2331D9 = "director on", observer cmd @0x271150, observer state @0x2714DC.
**Recording/replay determinism (2026-09-04).** Recorder (XV_PAD_REC, xk_os_vita.c ~L444) writes pad_rec.txt = "presentframe lx ly rx ry buttonsHex" on every state change; verified it captures full analog (0-255 all axes) + buttons faithfully. Replay = pad_play.txt, each line held until the next. XV_LOCKSTEP=1 (xd3d.c): exactly 2 vblanks/Present + 1 whenever the game spins in the 0xBB060 vblank wait (detected in xv_preempt via weak xd3d_lockstep_preempt reading the wait loop's stack frame return addr == a `call 0xBB060`), 30 fps cap by yielding in Present -> normal speed + deterministic sim. BUT frame index was boot-relative, and level load takes a run-dependent number of Present frames, so replays drifted. Fixed: xd3d_pad_frame() anchors the index at gameplay start (player0 unit at [[0x276794]+0x10] first != 0xFFFFFFFF); menus still boot-indexed. NOTE: recordings made before this commit are boot-indexed and won't line up with the anchored replay -> a fresh recording on the post-anchor build is needed to validate the replay. Saved recordings: scratchpad rec2/rec3_lockstep_*.txt (lockstep, boot-indexed).

**Camera-skip fix CONFIRMED on hardware (2026-09-04).** xd3d_cam_recover (xd3d.c, XV_CAM_FIX default on): when player unit valid + director vt==0x120A90 + observer transition (float @0x271150+0x60)<=0 for ~60 frames, calls guest camera_control(off) f_0011FFD0 via call_guest(c,0x11FFD0,0). User played most of a10 first-person after skip. Also fixed same session: multiplayer freeze (fragment_for now negative-caches failed sceGxmShaderPatcherCreateFragmentProgram; patcher heaps grown to buffer 512K/vusse 256K/fusse 256K in xv_shader.c - MP maps overflowed 64K fusse, 0x805B0024), LiveArea icon (8-bit indexed PNG; Vita rejects 24/32-bit), VPK size (dropped mock assets/ui_scene.bin from RECOMP, 11.5->10.8MB).
**HARDWARE gameplay profile (a10, 5-21 fps, XV_PROF, 95 dumps).** Hot path is the D3D HLE, not recompiled logic: DrawIndexedVertices (0x1842D0) ~10% (biggest, per-draw record cost -> xd3d_r_draw in xv_d3d.c), NtReadFile(0x1D66F0)~3.3% + NtWriteFile(0x1D66EC)~1.6% during gameplay, DrawVertices 0x184230, SetRenderState_Simple 0x1820A0 + SetRenderStateNotInline 0x1820F0, Present 0x185670 ~1.3%, SetTexture 0x181950, SetTextureState 0x182150, End 0x184700, KeDelayExecutionThread 0x1D6674. Non-HLE recompiled logic ~23% of samples. Next fps levers in order: xd3d_r_draw per-draw record cost, D3D state-call batching, the img-flat memory win (helps the 23%). Profiler H<addr> buckets set by XV_HLE_CALL (bit31|call-site addr).
**img-flat memory optimization (committed).** Constant image-address globals now use X_IMG* (g_img_base+addr, no page-table load); 31,593 sites; g_img_base=g_xram+XRAM_SIZE-g_image_lo set in xk_mem_bind_arena; boot warns if runtime image bounds disagree with the recompiler. Correctness verified (full a10 gameplay identical on emulator). Perf gain expected on hardware, not yet measured. release/xita-2026-09-04i-imgflat.vpk, on card.
**Campaign save-resume (2026-09-04): user quit+saved mid-a10, resume restarted the mission.** savegame.bin is a valid 3.4MB in save/udata/UDATA/4d530004/122A17771B9E/ and save/cache/. XV_SAVE_LOG (xk_file.c NtQueryDirectoryFile) shows the game DOES enumerate the save containers and playlists fine. Leading theory: Halo only saves at CHECKPOINTS; no mid-mission checkpoint fired (checkpoints need combat to resolve), so the save stayed at mission start -> resume restarts. Tied to: NPCs weren't shooting (user report) - AI/weapon-fire not functioning, so encounters never resolve, no checkpoint. NPC-not-shooting is the root gameplay bug to chase next (AI timing vs game clock, or a recompiler bug in the AI/fire path); user suspects framerate. File-op logging in NtReadFile/NtWriteFile caps at 40 lines so checkpoint writes aren't visible - raise the cap or add a savegame-specific log to see the actual checkpoint write/read.

**ALPHA TEST DONE (2026-09-04 late).** Root cause of the user's HUD complaints (shield/ammo meters never update, no reticle, plasma effects invisible, alpha textures wrong): Halo fills HUD meters by alpha-testing a gradient; xd3d captured alpha_test/func/ref but nothing used them. Now: every fragment program (499 combiners via pixelshader_recomp_gen.py + xv_texmod/tex0/lm fallbacks) has `uniform float4 xv_atest=(ref/255, func 0..7, enable, 0)` and discards per NV097 func (0x200+n: NEVER LESS EQUAL LEQUAL GREATER NOTEQUAL GEQUAL ALWAYS); xd3d_alpha_test() packs state, cmd_t.atest records it per draw, xv_d3d sets the uniform (fs->p_atest). Shaders compiled via the EMULATOR's libshacccg (tools/vita3k.sh shaders shaders/) = same compiler as hardware, so .gxp go straight into the VPK, no on-device compile step. GOTCHA: the emulator's installed xv_shadercomp (XVSC00001) was the pre-rename one looking in ux0:data/xboxvita/shaders -> "no .cg files found"; reinstall tools/shadercomp/xv_shadercomp.vpk first. Also fixed generator bug: analytic-normalize cube path emits `float4 tN;` (no =) so the missing-decl pass double-declared tN -> 18 normalization-cube shaders had FAILED to compile (stale .gxp) since the cube fix; detection now matches `float4 tN;` too. 583/583 compile. load_gxp prefers ux0:data/xita/shaders/<name>.gxp over app0. Build: release/xita-2026-09-04l-alphatest.vpk (on card). NOT yet verified on hardware.
**Hardware fps after img-flat (session 20:00-20:20):** sampled frames 20-25fps:57, 25+:16, 15-20:5, 10-15:5, <10:10. Real improvement vs earlier (5-9fps dominant), but combat spikes to 6fps (172ms frames) remain - that's what the user feels as "same fps". Next: DrawIndexedVertices record cost (xd3d_r_draw). "Look down = black" reported (floor near player renders black) - not yet diagnosed; need a hist dump of a look-down frame. shot_watch fired 0 dumps in that session (XV_SHOT_DUMP=1 was set) - verify it works. Resume still restarts a10; XV_SAVE_LOG was accidentally dropped from the card cfg so no trace captured - now restored, need one resume attempt. Loading screen: maps have stripped tag paths (no named loading bitmap findable); game blocks render thread during load -> black; only fix is runtime-drawn.
