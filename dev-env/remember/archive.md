# Archive

## Week of 2026-09-22
Perf campaign (perf128–170): tested texture memo, scheduler-parking, vertex snapshots, alpha-zero shaders (577 variants); Vita stabilized 56.6ms mean. Ported 4 native functions (collision-ray, audio queries; 2–2.2× speedups verified 498k frames); completed Halo 2 harness (x86/Pi4), deployed Halo2-Vita3K D3D build; handed off to Codex.

## Week of 2026-09-15
Shader optimization trials ruled out (1158 shaders, 2.5% reduction, no frame-time gain). Identified 25–45ms unaccounted GPU latency. Implemented Discord infrastructure (Moderator role, channels, hierarchy, guides). Developed draw-HLE analysis framework (10-stage breakdown) with hold-child suspend/resume support. Built perf49–50 with observer logs; began performance analysis revealing polling-window mismatch and math-mutex contention.

## Week of 2026-09-01
Deployed Vita infrastructure pipeline (x86 recompiler 8.1K funcs, D3D→GXM HLE layer, DirectSound mixer, networking/P2 gamepad support) reaching 67fps functional Blood Gulch co-op on hardware. Fixed 68+ shader rendering issues (fog/sky/vertex), cubemap lighting artifacts, texture swizzles, and TLS corruption; optimized portal-culling. Addressed critical runtime issues: vblank timing (60fps→2x speed bug), 30fps frame clock implementation, LiveArea icon rendering, and replay playback. Rebranded XboxVita→Xita (612 files); finalized ROADMAP (4-phase gameplay/dashboard/multiplayer); merged dashboard branch; profiler identified 84% HLE overhead with staged critical bugs.