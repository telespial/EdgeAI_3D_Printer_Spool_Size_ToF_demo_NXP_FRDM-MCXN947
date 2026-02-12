# Project Status

- Name: ToF Demo (FRDM-MCXN947)
- State: stable baseline locked (active debug features retained)
- Last update: 2026-02-12

## Current Working Baseline
- Build/flash path is stable via project-local scripts.
- Live TMF8828 stream is stable with locked mapping:
  - `TMF8828_ZONE_MAP_MODE=1`
  - `TMF8828_CAPTURE_REMAP=[1,3,0,2]`
- Close-range target window is locked to `50..150 mm`.
- UI layout:
  - full LCD split into 3 vertical columns
  - Q1 upper half: 8x8 heatmap grid
  - Q1 lower half: tiny runtime terminal (`LIVE`, `RNG`, `AVG`, `ACT`, status counters)
  - segments 2 and 3 merged: colorful 3D spool tube render (full height)
- spool tube distance behavior:
  - full roll diameter at `<=50 mm`
  - reduced diameter inside `50..100 mm`
  - full roll diameter again at `>=100 mm`
- Render stability updates in place:
  - complete-cycle draw preference + max draw gap cap (`500 ms`)
  - reduced wipe/blank transitions
  - display-hole backfill for isolated invalid cells in raw mode

## Last Run
- Date: 2026-02-12
- Result: ok (build + flash + user-confirmed LCD behavior)
- Build: `./tools/build_frdmmcxn947.sh debug`
- Flash: `./tools/flash_frdmmcxn947.sh`
- Notes: user requested spool tube UI in segments 2+3; build/flash validated on board.

## Restore Baseline
- Golden restore index: `docs/RESTORE_POINTS.md`
- Golden tag: `GOLDEN_2026-02-12_v2_spool_tube_segments23`
- Lock tag: `GOLDEN_LOCK_2026-02-12_v2_28f9659`
- Failsafe pointer: `docs/failsafe.md`
- Failsafe flash command: `./tools/flash_failsafe.sh "$(sed -n '1p' docs/failsafe.md)"`

## Hardware Targets
- FRDM-MCXN947
- LCD-PAR-S035
- TMF8828_EVM_EB_SHIELD

## Next Checkpoint
- Collect one fresh UART trace set with the locked baseline and attach it to `docs/TOF_DEBUG_STATUS.md` as post-golden evidence.
- Optionally re-enable filtering stages incrementally behind compile flags for A/B validation.
