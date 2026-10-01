---
name: halo2-vita3k-menu-20260924
description: "Halo 2 main menu verified in Vita3K with no env.txt (MENU_RUNTIME_DEFAULTS=1), dashboard->H2->dashboard selector chain verified, combined hardware candidate built (exec-only H2 update vs perf208c); CE hardware builds cannot run in Vita3K (TPIDRURW)"
metadata:
  node_type: memory
  type: project
  originSessionId: a052a6d4-de78-46d9-a715-dd38bb415cac
  modified: 2026-09-25T04:03:58.559Z
---

Goal (user /goal, Sept 24 2026): Halo 2 boots to a working main menu in Vita3K, Pi validates ARM; no physical Vita while Codex works on Halo CE; no push.

- Worktree `~/xita-backups/2026-09-24-halo2-vita3k/source`, branch `work/halo2-vita3k-20260924` (from unified ea21f84 + merge of the H2 harness branch 06922d2). Private dir `.../private` (stages h2v1 / h2v1r standalone candidate / h2v1b bundled, runs/, scripts). Handoff: `docs/halo2-vita3k-menu-20260924.md`.
- Commits: c3099c3 (recompiler: XV_HLE_PROXY before the object-jobs ifdef; CE expansion unchanged), 6f0551d (H2 hle_timing.c stubs for CE's HLE-timing symbols), ea40725 (H2 Makefile MENU_RUNTIME_DEFAULTS=1 -> XV_MENU_VBLANK=1/YIELD=0/THREADS=3 compiled in), 77d84a7 + e153521 (Pi/host runner knobs PI_APP0, H2_HOST_BARE).
- The real blocker for hardware packages: without XV_MENU_VBLANK=1 thread 8 spins in f_0012B450 (from f_0022324C) waiting for the frame counter 0x485AB0, which only flip-completion vblanks advance, and the menu submits no flips. Earlier lab runs worked only because lab `ux0:data/xita/env.txt` set it.
- Vita3K evidence: h2v1r-n2..n4 cold launches with the lab `ux0:data/xita` moved aside (`private/noenv_run2.sh`) reach title -> profile -> main menu; D-pad/A/B work; 0 error lines. The error count misses silent stalls; always look at screenshots.
- Lab trap: `$LAB/run_lab.py` installed a fixed file list and skipped the packaged `h2menu_*.gxp` (fixed Sept 24). Before the fix, packages without lab ux0 shaders took the slow software path.
- Selector: CE hardware builds (XV_THREAD_PAGE_TABLE=1) abort in Vita3K on the TPIDRURW write (Unhandled CP15 MCR c13,c0,2). A Vita3K-only CE stand-in (`private/ce-vita3k`, page table off) proved the chain dashboard -> `eboot.bin --xita-game=halo2` -> `halo2-a.self` -> menu -> Start+Select -> dashboard.
- Hardware candidate `private/combined/xita-perf208c-h2ea40725.vpk` (sha 51f989b5...): perf208c with only halo2-a.self and boot-halo2.txt changed, so contracts match and it is an executable-only `update --game halo2`. NOT deployed. Rebuild against whatever CE package is installed at test time.

See [[halo2-menu-lab]], [[halo2-host-harness-20260923]], [[halo-a30-benchmark-20260924]].
