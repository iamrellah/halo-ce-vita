---
name: halo-codegen-qlocals-20260929
description: "Sept 29 2026 takeover from Codex: program-wide codegen (guest regs+flags in C locals) gave first whole-frame b30 gain (perf323 117.6 ms vs 123.4); perf324 layout/hints and perf325 bias page table followed"
metadata:
  node_type: memory
  type: project
  originSessionId: 04c2f4a9-0351-4c83-915d-867705611b62
  modified: 2026-09-29T15:03:48.250Z
---

Took over Halo CE perf work from Codex on 2026-09-29 (authoritative checkout xita-backups/2026-09-18-unified-games/source, branch work/2026-09-18-packet-followup, uncommitted, no commits made). Codex's narrow feature-walk path (perf313-322) never moved b30 frame time (~123-128 ms); Pi ARM qualification of its staged walk harness passed and is recorded in ~/xita-backups/xita-walk-integration-20260929/STATUS.md, but it is worth <0.1 ms/frame.

**Why:** Vita disassembly showed generated code keeps guest regs/lazy flags in memory (c->r, c->f_*) because may_alias guest accesses defeat restrict. tools/qlocals.py (post-pass after hooks) keeps them in locals with dirty-mask syncs at barriers; flags published at returns (base code lets callee flags reach callers). Bugs found: prologue goto skipping local init (f_00056670), 8/16-bit mul helpers writing EAX/EDX, flags at returns.

**Results:** Pi A/B v2: tick -10%, scene -8%. Vita perf323: 117.57 ms (8.51 FPS), p99 169, owner FA920 -5.6%, AI -11%. Private stages qlocals-323/324/325 next to source; workbench qlocals-20260929 (x86+ARM harnesses, XV_STATE_PAGES page oracle, compare scripts, qbisect.py). Doc: source/docs/qlocals-codegen-20260929.md.

**Later same day:** perf324 (float-access branch hints + hot layout) 111.5 ms; perf325 (page-table bias array) 110.4 ms = best, installed slot 1. Rejected on Vita: perf326 x87 splice (Pi said -6.8% tick, Vita +1%), perf327 -Os (+7%), perf328 -Os+inline helpers (+3%). Pi A/Bs are directional only; Pi runs must use the Vita normal env. Next target: native BSP sphere query ~17.5 ms/frame, ~60K cycles/call (memory-latency bound). perf330 (semantic BSP query, tools/patch_native_4b9d0_semantic.py) = 101.9 ms / 9.81 FPS, installed slot 0 (perf325 slot 1). Upstream halo-ce-universal decomp port runs the user's NTSC 2276 maps (b30 verified on Linux, ~/github/halo-ce-vita-spike); user considering a separate 'Halo CE Vita' project; proposed next: Pi null-renderer tick timing vs Xita harness. 500 MHz is not available (user confirmed 2026-09-29): plan at 444 MHz.

**How to apply:** Judge every candidate on the Vita (aligned b30 windows, tools/frame_times.py final 10 windows); the Pi only filters for correctness/direction. When copying objects into a harness out dir, touch changed sources or host_build skips them. Never `pkill -f` a pattern present in your own command. Build command pins `-o build/recomp/query-fusion.generated.json` so transformed shards are not regenerated. Related: [[halo-hierarchy-assist-ceiling]], [[halo-host-harness-workbench]].
