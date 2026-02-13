# Project State

Last updated: 2026-02-13
Project: `ToF__demo_NXP_FRDM-MCXN947`

## Current Baseline
- Release baseline: v6
- Golden tag: `GOLDEN_2026-02-13_v6_detection_rewrite_stable_states`
- Lock tag: `GOLDEN_LOCK_2026-02-13_v6_12d2789`
- Golden commit: `12d27895319a5527e146ac65526baf9993f236cf`
- Failsafe image: `failsafe/FAILSAFE_2026-02-13_v6_detection_rewrite_stable_states.elf`
- Lifecycle: active baseline checkpoint (post-detection rewrite)

## Firmware Behavior (Current)
- 8x8 heatmap + tiny terminal in left column (Q1).
- spool roll render and bargraph across merged right-side area.
- spool model path rewritten to be AI-mode invariant for state decisions.
- Roll-state output constrained to 8-segment bargraph and four states (`FULL/MED/LOW/EMPTY`).
- Empty detection has MM hysteresis (`enter >=62`, `exit <=58`) plus sparse/no-surface fallback.
- Alert runtime default is OFF at boot.
- Full/sparse and empty/sparse overrides added to stabilize physical swaps.

## Runtime State Classification
- State source: segment-based (`0..8`) with consensus/hysteresis.
- Empty: hard-empty conditions or state thresholds (`mm` + low segment) with hysteresis.
- Full: segment `6..8` plus sparse-full override path.
- Medium: segment `3..5`.
- Low: segment `1..2`.

## Known Risk To Re-Verify
- Repeated swap sequence (`full -> medium/low -> full`) should continue to re-lock to full after long test cycles.

## Handoff Notes
- Primary source files:
  - `src/tof_demo.c`
  - `src/tmf8828_quick.c`
- Baseline restore procedure:
  - see `docs/RESTORE_POINTS.md`
  - see `docs/failsafe.md`
- Next work queue: `docs/ToDo.md`
