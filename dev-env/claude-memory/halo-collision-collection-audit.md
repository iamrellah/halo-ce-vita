---
name: halo-collision-collection-audit
description: "Sept 2026 Claude audit of Halo CE 171F10 collision collection; opt-in native object-walk helper, review fixes, host/ASan pass"
metadata: 
  node_type: memory
  type: project
  originSessionId: c5d82b61-1af4-4118-914b-585b10033f82
  modified: 2026-09-17T03:45:24.804Z
---

On 2026-09-16, the Claude worktree `claude-collision-collection-20260916` received a report (`docs/claude-collision-collection-20260916.md`) and an opt-in `XV_NATIVE_OBJECT_COLLECT` helper. The helper hooks 172034 and skips 1716F0 calls that exit early. The differential test is `tools/test_object_collect.py`. It must run with the private venv python at `2026-09-12-halo2-initial-profile/private/venv`, which has iced_x86.

The first draft had three review findings, all fixed the same day:
- It omitted call-frame stores, which inputs can alias; the helper now performs all 12 stores.
- It re-tested EAX after a yield; the loop now resumes unconditionally.
- Its counters and config were plain shared variables; they are now atomic.

After the fixes, host and ASan/UBSan pass 3000 cases in each mode, and the negative controls fail. Receipts are under `validation/.../claude-collision-validation/followup-*`. The worktree changes are still uncommitted, because that session could not run `git add`.

An ARM qualification (`tools/test_arm_object_collect.py`, VitaSDK at `~/vitasdk/bin`, Unicorn) passes 544 fixtures. Instruction counts are about −70 to −90 per skipped object but +120 to +340 per fallback object, and small walks lose. It also found an FPSCR NZCV-only difference; a proposed diff is under `arm-qualification-nzcv-proposal`.

**Why:** Codex owns the Vita, integration and measurement. Physical sampling showed collection 171F10 at about 81–83% of 172BF0.

**How to apply:** The next ranked target is a native 86F50 surface test, which nested object BSP queries through 172F40 also use. Not yet done: a VitaSDK compile, a TSan run of the counters, benchmark selector wiring, and hardware. That session's permissions denied compound or env-prefixed shell commands, tee, and git add. Related: [[xboxvita-stage4-recompiler]].
