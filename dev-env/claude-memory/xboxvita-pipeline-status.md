---
name: xboxvita-pipeline-status
description: XboxVita (Xbox->PS Vita static recomp) offline toolchain — stage files, target title, tooling gaps
metadata:
  type: project
---

XboxVita offline pipeline lives in ~/github/xboxvita (not a git repo as of 2026-09-01):
- Stage 1 `xbe_parse.py` (XBE header/sections/libs, `--json` manifest)
- Stage 2 `dx8_shader_parse.py` (D3DVSD decl tokens + NV2A vertex microcode disassembly, `--scan`)
- Stage 3 `shader_recomp_gen.py` (Stage 2 JSON -> Vita Cg vertex shader; ff and microcode modes)
- Design doc: `xboxvita-architecture.txt`

Target title is Halo (haloce/default.xbe, XDK 3925): Stage 2 finds 67 vertex-shader blobs + 9 decls; Stage 2/3 validated on all 67 (2026-09-01). Shader table lives at VA 0x1F91E0 (.data), 16-byte records {handle, func_ptr, -1, size}.
vitasdk installed 2026-09-01 at ~/vitasdk (vitasdk-core 2026.08.1, GCC 15.2.0; env exported in ~/.zshrc; vdpm clone at ~/vdpm). `psp2cgc` is proprietary/unavailable; shaders are compiled ON the Vita by tools/shadercomp, which drives SceShaccCg (ur0:data/libshacccg.suprx, SDK 3.0.0 r13225, extracted by ShaRKF00D) DIRECTLY via psp2/shacccg.h. Do NOT use vitaShaRK/SceShaccCgExt: its binary patches make every compile fail with 'fatal internal error' and crash on this module. All 67 Halo vertex programs + the FF test shader compiled on-device with 0 failures and are packed into xboxvita.vpk (2026-09-01); Decl<->shader pairing solved by xbe_shader_pairs.py (table @0x1F91E0 + initializer store pattern): 67/67 paired, 8 have SetVertexData4f constant-fed inputs, #33 flagged suspect. All 67 regenerated with real decls and compiled 0 errors (SceShaccCg is strict: no implicit float3->float4 widening).

User's Vita: PSVSD setup - ux0: = 238 GB PSVSD, uma0: = official memory card; VitaShell USB mode exposes whichever is selected (auto-mounts under /run/media/birchwoodgod/<id>). FTP is not available; use `make shaders-usb` / `make shaders-pull-usb` / `make vita-eject`. main.c builds and packs to a .vpk with the real SDK.

Pixel shaders: Halo has NO static D3DPIXELSHADERDEF structs (runtime-built from tags). Stage 2c dx8_pixelshader_parse.py + Stage 3b pixelshader_recomp_gen.py exist and are validated on synthetic defs; runtime plan = hash->gxp cache + on-device SceShaccCg compile fallback (see architecture addendum). Fog varying = TEXCOORD7 on both sides.

Runtime: main.c + xv_shader.c/.h (GXM shader patcher, .gxp loading from app0:shaders/, vertex programs from gen_layouts.py tables in shaders/xv_layouts.h, const stream for SetVertexData4f attrs) build clean; first-draw path (mock triangle from XRAM via halo_shader_0 + xv_solid.frag) is wired but UNTESTED on hardware: xv_solid.frag.gxp + pstest_*.frag.gxp still need the on-device compile.

Vita3K (v0.2.1 Qt build) is installed at ~/vita3k/ubuntu/Vita3K with firmware 3.74; pref path ~/.local/share/Vita3K/Vita3K; config ~/.config/Vita3K/config.yml (launch with -f -w or it resets config). libshacccg.suprx in its ur0:data lets xv_shadercomp compile shaders INSIDE the emulator (tools/vita3k.sh shaders; Makefile vita3k-shaders/vita3k-run) - no console needed. xboxvita.vpk runs in Vita3K: 600 frames @60fps, first triangle rendered from XRAM through halo_shader_0 + xv_solid.frag (2026-09-01). GXP struct inputs are named 'IN.<member>'.

HARDWARE GOTCHA: Vita kernel rejects syscalls when SP is outside the thread's registered stack (Vita3K doesn't check) -> fiber stacks must be slices of the guest thread's own stack (done in xk_init). D3D HLE (xv_d3d.c) draws textured quad in Vita3K; CONFIRMED ON REAL VITA 2026-09-01: textured quad via D3D HLE renders correctly (zero-copy swizzled texture from XRAM works on SGX, no dcache kernel module needed for this test).

EXIT PROTOCOL gotcha: tearing down GXM objects at exit caused a GPU crash + hard reset on hardware; now drain + detach framebuffer + exit (XV_FULL_TEARDOWN for explicit teardown). User works remotely -> emulator-only workflow (tools/vita3k.sh; high-accuracy + surface sync enabled in Vita3K config).

Scene-viewer track (asset replay, no recompiler): halo_map.py parses Xbox cache files (zlib body, tag data @0x803A6000, 'mode' parts 104 B w/ pre-baked D3D VB/IB structs, bitmap entries 48 B), halo_scene_export.py -> assets/ui_scene.bin, xv_scene.c replays via HLE. ui.map's BSP is a 2 KB stub; menu = sky_ui + scenery\halo\halo models. MILESTONE 2026-09-01: Halo main-menu backdrop (ring + galaxy + stars, real ui.map assets) renders in Vita3K via the scene viewer (screens in assets/screenshots/). Camera is a synthetic one (origin-ish, along the ring); Halo's real UI camera not yet located. Next: recompiler (Stage 4) / kernel translator.

**Why:** Later stages (Cg fragment/combiner translation, GXM binding, runtime) build on these files and contracts (uniform `c[]` array with C_BASE, AppIn member names = GXM parameter names).
**How to apply:** Reuse the Stage 2 JSON schema and Stage 3 uniform/attribute contract rather than inventing new ones; flag that .cg output is unverified until psp2cgc is available.
