# Handoff — 2026-09-18 (Claude, Remote Control session)
Read /home/birchwoodgod/xita-backups/2026-09-18-tester-release/CLAUDE-SESSION-20260918.md first (device state, receipts, open question, options A/B/C).
- Vita: slot 0 = qualified 0.2.0-test.1/ed9af19 (bd502b1d…), booted, dashboard, lease cleared. Slot 1 = half-precision trial e21e948 (not qualified).
- Branch work/2026-09-18-ps-half-precision pushed (generator half mode + doc). No change to main. Repos private.
- Finding: Blood Gulch frame = GPU query latency + ~4 ms; CPU stream prep 1–7 ms; half-precision shaders gave no gain (rolled back); GPU cost not pixel- or instruction-proportional. Next: XV_GPU_PACKET_TIMING attribution or same-view resolution A/B — user decides.
