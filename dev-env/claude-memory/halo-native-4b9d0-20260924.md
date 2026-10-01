---
name: halo-native-4b9d0-20260924
description: "Sept 24 2026: XV_NATIVE_4B9D0 natives under f_0004B9D0 (BSP sphere query 88110 subtree + solver feature test 864C0 subtree), exact, verified host+Pi, branch work/native-4b9d0-20260924, docs/native-4b9d0.md"
metadata:
  node_type: memory
  type: project
  originSessionId: a052a6d4-de78-46d9-a715-dd38bb415cac
  modified: 2026-09-24T15:01:23.104Z
---

Worktree `/home/birchwoodgod/xita-backups/2026-09-18-unified-games/native-4b9d0-wt`, branch work/native-4b9d0-20260924 (commits d727cbb, cfae659, 2cdd282, da9f8ea; not pushed). Work dir `native-4b9d0-work` (stage-n4 host stage with the natives installed, objs-n5-x86/arm harnesses, private captures in captures/, scripts/run.sh / pirun.sh / pi_pair.sh / timing.py [features]). Pi dir ~/xita-4b9d0 (cores 2-3).

**What:** `recomp/kernel/xk_native_4b9d0.c`: (1) f_00088110 + 87EA0/87E10/86F50/B0CB0 in place of the fused query `query_fused_172c95_171f94` (hook: #define in kernel/xk_query_reuse.c); (2) f_000864C0 + 85D10/85A00/85720/11120/111A0 at the fused solver's 170CD1 call (hook edits recomp/solver_fusion.c). tools/patch_native_4b9d0_hooks.py, tools/install_native_4b9d0.py <stage>. Env XV_NATIVE_4B9D0 0/1 verify/2, _PARTS 1/2/3, _TIME=1.

**Shares (probe):** query 41 % (Pi) / 56 % (x86) of 4B9D0, feature test 17 %; next candidates 868F0 (9.5 % Pi), 172040 (6.7 %).

**Speed:** query 2.09x x86 / 1.57x Pi replay vs fused guest, 1.46x x86 in-game lockstep pair; feature test 3.2x replay vs plain translation, 1.94x x86 in-game vs fused solver copy. Lockstep pairs must use matched physical cores (CPU n and n+16 are SMT siblings; 0-7 and 8-15 are different CCDs).

**Evidence:** 3,591,664 in-game verify calls (x86+Pi) 0 mismatches, 360k differential cases, 35/39 mutants (4 explained: query `dirty` fallback never meets an aliased read in generated scenes). Vita estimate 2.1-2.4 ms/frame inside 4B9D0 (+ up to ~1.7 from other objects' 172BF0 calls).

**Gotcha found:** frame words read back as integer counts make control flow depend on NaN payload bits (host-compiler operand order) -> the feature test declines when the feature block overlaps its frames (never in game: features at E+0xD4).

**How to apply:** see docs/native-4b9d0.md for Vita integration (install into a copy of overlap-candidate/build-x87, XV_NATIVE_4B9D0=1 make var, runtime verify then native). Not yet on hardware. See [[halo-native-92330-20260924]], [[halo-host-harness-workbench]].
