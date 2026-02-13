# Project Status

- Name: ToF Demo (FRDM-MCXN947)
- State: stable v8 baseline published with restore tags and failsafe image
- Last update: 2026-02-13

## Current Working Baseline (v8)
- Build/flash path is stable via project-local scripts.
- Live TMF8828 stream is stable with locked mapping:
  - `TMF8828_ZONE_MAP_MODE=1`
  - `TMF8828_CAPTURE_REMAP=[1,3,0,2]`
- Roll measurement/detection pipeline rewritten for deterministic state behavior.
- Roll output model:
  - 8-segment bargraph (`0..8`)
  - four states only (`FULL`, `MEDIUM`, `LOW`, `EMPTY`)
- Empty hysteresis active:
  - enter empty at `>=62 mm` with low segment evidence
  - exit empty below `58 mm`
- UI layout:
  - Q1 upper half: 8x8 heatmap grid
  - Q1 lower half: tiny runtime terminal (`LIVE`, `AVG`, `A`, confidence/noise fields)
  - right-side merged render area: TP roll + bargraph + status banner
- State logic uses segment/hysteresis + sparse override rules in `src/tof_demo.c`.
- v8 behavior updates:
  - AI on/off parity for state path (AI toggle no longer changes TP state input path)
  - hard-empty fallback on sparse/no-surface removal patterns
  - sparse-full override for close/full sparse-valid geometry
  - model updates continue while popup is visible (prevents freeze/stale lock)
  - warning popup forced top layer while active
  - TP render now uses fixed brown core with 8 discrete white-paper thickness levels
  - upper-right white branding watermark rendered with readable one-line font (`(C)RICHARD HABERKERN`)

## Last Run
- Date: 2026-02-13
- Result: PASS (build + flash + v7 golden/failsafe release packaging)
- Build: `./tools/build_frdmmcxn947.sh debug`
- Flash: `./tools/flash_frdmmcxn947.sh`

## Restore Baseline
- Golden restore index: `docs/RESTORE_POINTS.md`
- Golden tag: `GOLDEN_2026-02-13_v8_brand_font_readable`
- Lock tag: `GOLDEN_LOCK_2026-02-13_v8_1dccefd`
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
