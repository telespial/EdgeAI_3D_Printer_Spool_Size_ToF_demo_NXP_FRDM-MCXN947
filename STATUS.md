# Project Status

- Name: ToF Demo (FRDM-MCXN947)
- State: stable TP baseline locked; AI data pipeline bootstrap started
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
- spool tube + bar behavior:
  - full roll diameter at `<=50 mm`
  - reduced diameter toward `>=100 mm` (empty condition)
  - fixed-size enlarged brown core; only white paper thickness changes
- Response/visual stability updates in place:
  - TP/bar update target around `100 ms`
  - local redraw region for TP area to reduce wipe/blink
  - persistent last valid TP/bar state across short live gaps
  - removed white tick/marker artifacts from bargraph
- AI bootstrap updates:
  - compile-time dataset logging scaffold added (`AI_CSV` / optional `AI_F64`)
  - feature extraction fields emitted for downstream model training

## Last Run
- Date: 2026-02-12
- Result: ok (build + flash + user-confirmed LCD behavior)
- Build: `./tools/build_frdmmcxn947.sh debug`
- Flash: `./tools/flash_frdmmcxn947.sh`
- Notes: baseline locked after TP/bar responsiveness pass and AI data-log scaffold bring-up.

## Restore Baseline
- Golden restore index: `docs/RESTORE_POINTS.md`
- Golden tag: `GOLDEN_2026-02-12_v3_tp_fast_persistent_ai_bootstrap`
- Lock tag: `GOLDEN_LOCK_2026-02-12_v3_89e625d`
- Failsafe pointer: `docs/failsafe.md`
- Failsafe flash command: `./tools/flash_failsafe.sh "$(sed -n '1p' docs/failsafe.md)"`

## Hardware Targets
- FRDM-MCXN947
- LCD-PAR-S035
- TMF8828_EVM_EB_SHIELD

## Next Checkpoint
- Capture first AI dataset run (`AI_CSV`) with known roll states (empty/half/full).
- Add host-side data capture helper script and training notebook skeleton.
