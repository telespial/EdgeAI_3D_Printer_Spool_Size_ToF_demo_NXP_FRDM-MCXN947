# Project State

Last updated: 2026-02-13
Project: `ToF__demo_NXP_FRDM-MCXN947`

## Current Baseline
- Release baseline: v7
- Golden tag: `GOLDEN_2026-02-13_v7_popup_front_fixedcore_8step`
- Lock tag: `GOLDEN_LOCK_2026-02-13_v7_<commit>`
- Golden commit: `<commit>`
- Failsafe image: `failsafe/FAILSAFE_2026-02-13_v7_popup_front_fixedcore_8step.elf`
- Lifecycle: active baseline checkpoint (popup-front + fixed-core discrete render)

## Firmware Behavior (Current)
- 8x8 heatmap + tiny terminal in left column (Q1).
- spool roll render and bargraph across merged right-side area.
- spool model path is AI-mode invariant for state decisions.
- Roll-state output constrained to 8-segment bargraph and four states (`FULL/MED/LOW/EMPTY`).
- Empty detection uses MM hysteresis (`enter >=62`, `exit <=58`) plus sparse/no-surface fallback.
- Alert runtime default is OFF at boot.
- Full/sparse and empty/sparse overrides stabilize physical swaps.
- Warning popup redraws as frontmost layer while active.
- TP render uses fixed-size brown core and discrete 8-level white paper thickness.

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
