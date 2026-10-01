---
name: halo2-host-harness-20260923
description: Sept 23 2026: Halo 2 runs headless on x86 Linux and the Pi 4 (boot -> menus -> split-screen Slayer on Ivory Tower), H2 worktree commits bc49709/fd74e13/55418c0, ARM profile: runtime not guest code dominates
metadata:
  type: project
---

A background agent (Sept 23 afternoon) built a Halo 2 host harness: the H2 stage's own units compiled unchanged against `games/halo2_5849/host/psp2_host.c` (Linux stand-ins for every sce* call; GXM is a null backend, renderer CPU work runs in full). Worktree `/home/birchwoodgod/xita-backups/2026-09-15-halo2-menu-claude/source`, branch work/halo2-menu-claude-20260915, commits bc49709, fd74e13, 55418c0 (not pushed); doc `docs/halo2-host-harness.md`.

**Results:** x86: title 18 s, match 65 s, 37-60 flips/s; Pi (taskset -c 2,3, from ~/xita-h2 only): title 41 s, match 323 s, 8.8-9.1 flips/s, 40 min soak clean, no [h2/blocked] stops. Fixes on the way: empty save/cache4 each run (the game re-formats n: every boot); fresh codegen `private/h2host1` (the mp292 codegen predated d0b7e63 write tracking -> attract-movie texture stop).

**ARM profile (main thread):** only 4.6% recompiled game code; ~27% texture binds (three-colour DXT scan before the unchanged check 13%, linear 512-entry key search ~7%), ~11% decode_attr, 6.4% h2_audio_backend_fixed_commit_ready (hashes ~45 KB DSP memory under both audio locks for a fault flag), ~5% 64-bit division helpers; audio DSP interpreter pegs another core (44% real time on the Pi). These are the H2 optimization leads; not yet applied (need a Vita check). Not done: Vita VPK of the new codegen, campaign path.

**How to apply:** rebuild/run commands in the doc (`tools/h2_host_build.py`, `tools/h2_host_run.sh`, `tools/h2_pi_run.sh`, `H2_DRIVE=1` menu driver). CE Pi runs use cores 0-1 (PI_CPUS=0,1), H2 cores 2-3. See [[halo2-menu-lab]], [[halo-host-harness-workbench]].
