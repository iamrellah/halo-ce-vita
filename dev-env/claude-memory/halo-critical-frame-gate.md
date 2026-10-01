---
name: halo-critical-frame-gate
description: "Sept 2026 Halo native frame audits — the flare gate turned out to mark a GPU throughput limit (~79 ms of service); the next step is a per-scene census, not CPU work"
metadata: 
  node_type: memory
  type: project
  originSessionId: c5d82b61-1af4-4118-914b-585b10033f82
  modified: 2026-09-17T07:05:52.843Z
---

**2026-09-17 audit.** Native campaign period ≈ previous frame's final GPU completion (~81 ms) + ~5 ms. The owner parks ~11.7 ms/frame at the flare brightness barrier. Recommended the startup `XV_QUERY_BOUNDARY`.

**Follow-up (same day).** Root shipped the query boundary (runtime 4d84eb02, not matched views).
- Before: 60/60 prefixes observed, pending 1 → 2, final latency ~78 → ~132 ms, flare wait still ~11.6 ms.
- After: period ~79 ms ≈ GPU service. The limit is GPU throughput, not a latency-only gate.
- Queue model: ~20 ms of GPU service before the query boundary, ~59 ms after it (8 scenes; offscreen pool 2×64² ARGB8 + 2×128² RGB565).
- Source can't attribute the tail: backbuffer store/reload vs fragment shading vs offscreen passes that sample the partly rendered backbuffer (`previous_frame`).
- Proposed the passive `XV_SCENE_CENSUS`: fragment notifications on existing EndScenes plus per-scene sample masks.
- Reports are in `claude-critical-frame-audit/` and `claude-critical-frame-followup/`.

**Notification capacity (2026-09-17).** No SDK/source contract proves more than the 8 words already in use (region 0x70026420 inside a 1 MiB mapping; Vita3K's 1 MiB is emulator-only). Proposed `XV_SCENE_CENSUS_PROGRESS`:
- borrow the query word of slot (t+2)&3 when t−done ≤ 2;
- added scenes write values t−1−k, which can never equal t+2;
- never share final words.
Report: `claude-scene-notification-followup/report.md`.

**Why:** explains why CPU helpers show no gain at native resolution, and why the next step is GPU per-scene attribution.

**How to apply:** check `max pending` and prefix/tail before crediting CPU or queue changes. The query boundary added ~54 ms of queue latency, so watch input lag. Related: [[halo-collision-collection-audit]].
