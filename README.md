# ToF Demo (FRDM-MCXN947)

Quick 8x8 ToF heatmap demo for FRDM-MCXN947 using:
- FRDM-MCXN947 board
- LCD-PAR-S035 display (J8)
- TMF8828_EVM_EB_SHIELD (I2C)

This repo now includes:
- `src/tof_demo.c`: main loop that reads 8x8 ToF values and renders color cells.
- `src/tmf8828_quick.*`: lightweight TMF8828 init/read path for rapid bring-up.
- `src/par_lcd_s035.*`: ST7796S + FLEXIO LCD glue for LCD-PAR-S035.
- `src/platform/display_hal.*`: display abstraction used by the demo.
- `sdk_example/`: MCUX SDK overlay for `demo_apps/tof_demo`.
- `tools/`: setup/build/flash scripts.

## Quickstart
1. `./tools/bootstrap_ubuntu_user.sh`
2. `./tools/setup_mcuxsdk_ws.sh`
3. `./tools/build_frdmmcxn947.sh debug`
4. `./tools/flash_frdmmcxn947.sh`

## Session Read Order
1. `docs/START_HERE.md`
2. `docs/PROJECT_STATE.md`
3. `docs/OPS_RUNBOOK.md`
4. `docs/HARDWARE_SETUP.md`

## Current Restore Baseline
- Golden tag: `GOLDEN_2026-02-13_v5_full_reacquire_alertoff`
- Lock tag: `GOLDEN_LOCK_2026-02-13_v5_920a5d8`
- Failsafe image pointer: `docs/failsafe.md`

## Key Paths
- Application source: `src/tof_demo.c`, `src/tmf8828_quick.c`, `src/par_lcd_s035.c`
- MCUX overlay app: `sdk_example/mcuxsdk_examples_overlay/demo_apps/tof_demo`
- Board overlay: `sdk_example/mcuxsdk_examples_overlay/_boards/frdmmcxn947/demo_apps/tof_demo`
- Workspace (generated): `mcuxsdk_ws/`

## Notes
- If TMF8828 data is unavailable, the demo shows an animated fallback pattern so the LCD path can still be verified.
- Color mapping is RGB565 and scales near/far ranges into a blue-to-red heatmap.

## AI Bootstrap (In Progress)
- Firmware includes AI dataset logging scaffolding in `src/tof_demo.c`.
- Enable CSV logging by building with:
  - `TOF_AI_DATA_LOG_ENABLE=1`
- Optional full 64-cell frame dump per sample:
  - `TOF_AI_DATA_LOG_FULL_FRAME=1`
- Log marker format:
  - `AI_CSV,...` for compact feature rows
  - `AI_F64,...` for full frame rows (when enabled)

## Restore
- Golden restore points: `docs/RESTORE_POINTS.md`
- Pinned failsafe artifact: `docs/failsafe.md`
- Failsafe flash command:
  - `./tools/flash_failsafe.sh "$(sed -n '1p' docs/failsafe.md)"`

## Debug Port Checks (Recommended)
FRDM-MCXN947 has a built-in MCU-Link debug port. Use it to verify ToF data
presence first:

```bash
LinkServer probe '#1' dapinfo
ls -l /dev/serial/by-id
screen /dev/ttyACM0 115200
```

Runtime interpretation:
- `TOF demo: live 8x8 frames detected`: live ToF stream is active
- left column UI: top half is 8x8 grid, bottom half is compact runtime terminal
- `LIVE:1` on the terminal means live frame flow; `LIVE:0` means stream gap/fallback state
- `ACT:<mm>` and `RNG:50-150` report the effective measured distance and display lock range
