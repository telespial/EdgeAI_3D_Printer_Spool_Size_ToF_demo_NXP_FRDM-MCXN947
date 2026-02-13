# Project State

Last updated: 2026-02-13
Project: `ToF__demo_NXP_FRDM-MCXN947`

## Current Baseline
- Release baseline: v5
- Golden tag: `GOLDEN_2026-02-13_v5_full_reacquire_alertoff`
- Lock tag: `GOLDEN_LOCK_2026-02-13_v5_920a5d8`
- Golden commit: `920a5d8528a59bb328bcbc6f5b5b2692b7bb9843`
- Failsafe image: `failsafe/FAILSAFE_2026-02-13_v5_full_reacquire_alertoff.elf`

## Firmware Behavior (Current)
- 8x8 heatmap + tiny terminal in left column (Q1).
- spool roll render and bargraph across merged right-side area.
- Roll calibration window:
  - full anchor at `<=35 mm`
  - empty threshold at `>60 mm`
- Alert runtime default is OFF at boot.
- TP model uses closest-valid pixel path for roll state decisions.
- Full-roll reacquire path widened after medium/low transitions.

## Runtime State Classification
- Empty: `model_mm > 60`
- Full: fullness `>=75%`
- Medium: fullness `35%..74%`
- Low: fullness `<35%` and not empty

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
