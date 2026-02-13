# Project Status

- Name: ToF Demo (FRDM-MCXN947)
- State: active spool-focused baseline on `main`
- Last update: 2026-02-13

## Current Working Baseline
- Build/flash path is stable via project-local scripts.
- Live TMF8828 stream is stable with locked mapping:
  - `TMF8828_ZONE_MAP_MODE=1`
  - `TMF8828_CAPTURE_REMAP=[1,3,0,2]`
- 3D printer spool output model:
  - 8-segment bargraph (`0..8`)
  - four states (`FULL`, `MEDIUM`, `LOW`, `EMPTY`)
- AI ON adds confidence-weighted estimator fusion for runtime stability.
- Debug terminal tail:
  - `AI:x A:mm`
  - `CONF:%`

## Last Run
- Date: 2026-02-13
- Result: PASS (build + flash)
- Build: `./tools/build_frdmmcxn947.sh debug`
- Flash: `./tools/flash_frdmmcxn947.sh`

## Hardware Targets
- FRDM-MCXN947
- LCD-PAR-S035
- TMF8828_EVM_EB_SHIELD
