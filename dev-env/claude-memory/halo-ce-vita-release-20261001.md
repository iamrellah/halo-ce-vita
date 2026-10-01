---
name: halo-ce-vita-release-20261001
description: "Halo CE Vita 1.0 release state (Oct 1 2026) - repo, branches, draft release, deploy, backups, open items"
metadata:
  node_type: memory
  type: project
  originSessionId: c3d84c3b-7dc8-40eb-b890-e282ea2f49bb
  modified: 2026-10-01T21:57:39.545Z
---

Decomp port tree: ~/github/halo-ce-vita-spike/port-arm, branch `release` pushed as `main` to remote `vita`
= https://github.com/BirchWoodGod/halo-ce-vita (private). Local `main` there is upstream halo-ce-universal, not ours.
- main = bb4f3ff (effect batching 1c42e50 + README screenshot docs/screenshots/blood-gulch.png). Deployed to the Vita Oct 1 14:45.
- Draft GitHub release v1.0 "Halo CE for PS Vita 1.0" with halo.vpk; notes in ~/github/halo-ce-vita-spike/release/notes-v1.0.md. User publishes.
- Effect batching (HALO_IMMEDIATE_MERGE=0 disables) checked in Vita3K only, not yet eyeballed on hardware.
- Backups Oct 1 (after two power cuts): backup/dev-env (scripts, devtools, notes, Vita screenshots, Claude memory);
  halo-ce-vita backup/port-tree-*, backup/unified-games-*, backup/wt-maps-*; Xita repo backup/worktree-2026-10-01,
  backup/local/* (43 local-only branches), backup/wt-2026-10-01/*, backup/unified-games-worktree-2026-10-01.
  Snapshot script (temp index, skips game data/>50MB): dev-env/snapshot.sh. Game data/saves/dumps NOT uploaded (one local disk).
- Open: Invader GPL-3.0 vs repo CC0 before going public; d20 hang in Vita3K; user's hardware checks; the user's house needs a new breaker (power cuts).
- zsh gotcha: `$c:refs/...` applies the :r modifier; quote refspecs or use ${c}.

See [[halo-vita-movies-water-20261001]], [[halo-ce-vita-native-port]].
