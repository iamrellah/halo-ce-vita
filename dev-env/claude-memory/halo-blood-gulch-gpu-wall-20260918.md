---
name: halo-blood-gulch-gpu-wall-20260918
description: "2026-09-18 acceptance of 0.2.0-test.1: Blood Gulch frame = GPU completion latency (46-81 ms) + ~4 ms, CPU stream prep only 1-7 ms; half-precision shaders tried and rolled back (no gain); GPU cost is NOT proportional to pixels or fragment instructions; next is XV_GPU_PACKET_TIMING attribution"
metadata:
  type: project
---

**Acceptance 2026-09-18 (installed runtime bd502b1d, build V0.2.0-test.1 / ed9af19, native 544p, GPU 222 MHz).**
Blood Gulch 60-frame windows from `[xv/ui] frame time`: cliff wall 42 draws = 49.5 ms game / 20 FPS / draw-HLE 2.2 ms; red base 66 draws = 60 ms / 16 FPS / 4.4 ms; heavy base 192 draws = 66 ms / 15 FPS / 14.2 ms. `[frame-retire]` completion 46-81 ms. `[frame-query-boundary]` in the cliff view: query notification 49.2 ms after submission, only 1.2 ms of GPU after it, one backbuffer scene per frame. `[flare-defer]` shows the guest parked 23.5 ms/frame at the flare brightness barrier. Guest fiber ~29% of a core, all cores 20-30%.

**Conclusion:** the frame is GPU fragment-bound even for a bare cliff wall; CPU stream/validation work (streams 1.4-6.7 ms/frame, capture reuse 100% hits) cannot reach 20 FPS. Receipts: `xita-backups/2026-09-18-tester-release/acceptance-claude/` (screenshots, run-current.log, acceptance-summary.json).

**Half-precision trial (same day): NULL RESULT, rolled back.** Runtime b2e4333f (e21e948) deployed as runtime-only update, rendering identical, windows not faster (menu 34.1 vs 35.0 ms; 40-49 draws 56.7 vs 49.8 ms). Instruction count fell only 2.5%. Device rolled back to slot 0 (bd502b1d) and confirmed. Branch `work/2026-09-18-ps-half-precision` (generator change + doc `docs/ps-half-precision-20260918.md`), staging `T/ce-build-half`, package `T/ce-half-update.vpk` kept as evidence only.

**Key structural fact:** the 7-draw main menu already shows 25 ms GPU completion latency and 35 ms frames; 360p (Sept 14) was no faster than 544p; heavy 200-draw frames show *lower* query latency (25-32 ms) than 42-draw cliff frames (45 ms). So the GPU-side time is not shaded-pixel or instruction work. Frame period in light views = query latency + ~4 ms because the guest waits at the flare brightness barrier for the previous world scene's query. Next: build with XV_GPU_PACKET_TIMING=1 (compiled off in release) and capture the same views to split GPU execution vs submission/observation/display-sync delay.

**Why:** prevents re-spending effort on CPU geometry paths while the GPU is the limiter.

**How to apply:** for any CE FPS work, read `[xv/ui] frame time`, `[frame-retire]`, `[frame-query-boundary]` and `[flare-defer]` first. Do not assume fragment shading; do not repeat precision/instruction-count experiments. Shader-only changes ship as runtime-only updates (embedded programs win over app0 files); emulator-hosted shader compile works via `acceptance-claude/compile_in_vita3k.sh`. Related: [[halo-critical-frame-gate]], [[xboxvita-stage4-recompiler]].
