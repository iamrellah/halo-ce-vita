---
name: xita-github-repo
description: Private GitHub repo for the project (Xita), remote origin, what must never be pushed
metadata:
  type: reference
---

Repo: https://github.com/Xita-Project/xita (moved from BirchWoodGod/xita; org remote confirmed Sept 18 2026) (private, default branch main, remote `origin`), created 2026-09-04.
The user will make it public once the Vita holds a stable 25 fps. Never push game data: haloce/, *.xbe, *.map, ISOs,
halo_image.bin, psp2core dumps, and the recompiled code_*.c are all gitignored and must stay untracked. Before going
public, review the tracked game-derived artefacts: shaders/psdefs/*.bin and recomp/host/ps_*.bin (captured combiner
defs) and shaders/halo_vs_*.cg / halo_shaders.json (translations of the game's vertex shaders). See [[xboxvita-stage4-recompiler]].
- Release policy (2026-09-04): no GitHub releases of xita.vpk - eboot.bin is the recompiled game engine. Builds stay local in release/ (git-ignored). Users build from their own XBE with tools/recomp.sh.
