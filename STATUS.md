# Project Status

- Name: ToF Demo (FRDM-MCXN947)
- State: stable v5 baseline published with restore tags and failsafe image
- Last update: 2026-02-13

## Current Working Baseline (v5)
- Build/flash path is stable via project-local scripts.
- Live TMF8828 stream is stable with locked mapping:
  - `TMF8828_ZONE_MAP_MODE=1`
  - `TMF8828_CAPTURE_REMAP=[1,3,0,2]`
- Roll calibration window currently used by UI:
  - full anchor `<=35 mm`
  - empty threshold `>60 mm`
- UI layout:
  - Q1 upper half: 8x8 heatmap grid
  - Q1 lower half: tiny runtime terminal (`LIVE`, `AVG`, `A`, confidence/noise fields)
  - right-side merged render area: TP roll + bargraph + status banner
- State logic:
  - empty when model mm `>60`
  - full when fullness `>=75%`
  - medium when fullness `35..74%`
  - low when fullness `<35%` and not empty
- v5 behavior updates:
  - alert default changed to OFF
  - closest-valid pixel path used for TP roll state
  - full-roll reacquire window widened for repeated roll-size swaps

## Last Run
- Date: 2026-02-13
- Result: PASS (build + flash + golden/failsafe release packaging)
- Build: `./tools/build_frdmmcxn947.sh debug`
- Flash: `./tools/flash_frdmmcxn947.sh`

## Restore Baseline
- Golden restore index: `docs/RESTORE_POINTS.md`
- Golden tag: `GOLDEN_2026-02-13_v5_full_reacquire_alertoff`
- Lock tag: `GOLDEN_LOCK_2026-02-13_v5_920a5d8`
- Failsafe pointer: `docs/failsafe.md`
- Failsafe flash command: `./tools/flash_failsafe.sh "$(sed -n '1p' docs/failsafe.md)"`

## Hardware Targets
- FRDM-MCXN947
- LCD-PAR-S035
- TMF8828_EVM_EB_SHIELD

## Next Checkpoint
- Validate long-cycle repeated swap stability (`full -> medium/low -> full`) over extended runs.
- Fine-tune remaining roll-state calibration edge cases in AI on/off parity mode.
- Continue AI/data pipeline tasks from `docs/ToDo.md`.
