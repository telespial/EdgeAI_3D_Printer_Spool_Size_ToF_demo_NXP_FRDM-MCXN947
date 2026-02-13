# Project State

Last updated: 2026-02-13
Project: `ToF__demo_NXP_FRDM-MCXN947`

## Current Baseline
- Release baseline: active working mainline (no pinned restore tag)
- Lifecycle: active development

## Firmware Behavior (Current)
- 8x8 heatmap + tiny terminal in left column (Q1).
- 3D printer spool render and bargraph across merged right-side area.
- 3D printer spool model path is deterministic with AI-assisted runtime fusion when AI is ON.
- Roll-state output constrained to 8-segment bargraph and four states (`FULL/MED/LOW/EMPTY`).
- Empty detection uses MM hysteresis plus sparse/no-surface fallback.
- AI ON path applies confidence-weighted estimator fusion to `actual_mm` and fullness.
- Debug terminal tail:
  - `AI:x A:mm`
  - `CONF:%`
- Alert runtime default is OFF at boot.

## Handoff Notes
- Primary source files:
  - `src/tof_demo.c`
  - `src/tmf8828_quick.c`
- Next work queue: `docs/ToDo.md`
