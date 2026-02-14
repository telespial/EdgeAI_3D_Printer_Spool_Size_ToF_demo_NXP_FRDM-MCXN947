# Project Status

- Name: ToF Demo (FRDM-MCXN947)
- State: recovered spool baseline published with restore tags and failsafe image
- Last update: 2026-02-14

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
- Host logging helper available:
  - `./tools/capture_ai_csv.sh --port /dev/ttyACM0 --marker AI_CSV --raw`
- Spool asset pack available:
  - `assets/spool_level_8.png` -> `assets/spool_level_1.png`
  - regenerated with logo/text removed and texture-preserving filament reduction

## Last Run
- Date: 2026-02-14
- Result: PASS (build + flash)
- Build: `BUILD_DIR=mcuxsdk_ws/build_spool ./tools/build_frdmmcxn947.sh debug`
- Flash: `BUILD_DIR=mcuxsdk_ws/build_spool ./tools/flash_frdmmcxn947.sh`

## Restore Baseline
- Golden restore index: `docs/RESTORE_POINTS.md`
- Golden tag: `GOLDEN_2026-02-14_v1_spool_recovered_baseline`
- Lock tag: `GOLDEN_LOCK_2026-02-14_v1_27eb7fa`
- Failsafe pointer: `docs/failsafe.md`
- Failsafe flash command: `./tools/flash_failsafe.sh "$(sed -n '1p' docs/failsafe.md)"`

## Hardware Targets
- FRDM-MCXN947
- LCD-PAR-S035
- TMF8828_EVM_EB_SHIELD
