# Project State

Last updated: 2026-02-13
Project: `ToF__demo_NXP_FRDM-MCXN947`

## Current Baseline
- Release baseline: v9
- Golden tag: `GOLDEN_2026-02-13_v9_ai_confline_runtime_assist`
- Lock tag: `GOLDEN_LOCK_2026-02-13_v9_<commit>`
- Golden commit: `<commit>`
- Failsafe image: `failsafe/FAILSAFE_2026-02-13_v9_ai_confline_runtime_assist.elf`
- Lifecycle: active baseline checkpoint (popup-front + fixed-core render + readable brand font)

## Firmware Behavior (Current)
- 8x8 heatmap + tiny terminal in left column (Q1).
- spool roll render and bargraph across merged right-side area.
- spool model path is AI-mode invariant for state decisions.
- Roll-state output constrained to 8-segment bargraph and four states (`FULL/MED/LOW/EMPTY`).
- Empty detection uses MM hysteresis (`enter >=62`, `exit <=58`) plus sparse/no-surface fallback.
- AI ON path applies confidence-weighted estimator fusion to `actual_mm` and fullness for improved runtime stability; AI OFF remains deterministic baseline path.
- Debug terminal lines now end with:
  - `AI:x A:mm`
  - `CONF:%`
- Alert runtime default is OFF at boot.
- Full/sparse and empty/sparse overrides stabilize physical swaps.
- Warning popup redraws as frontmost layer while active.
- TP render uses fixed-size brown core and discrete 8-level white paper thickness.
- Brand line uses one-line readable glyph set: `(C)RICHARD HABERKERN`.

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
