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

## Key Paths
- Application source: `src/tof_demo.c`, `src/tmf8828_quick.c`, `src/par_lcd_s035.c`
- MCUX overlay app: `sdk_example/mcuxsdk_examples_overlay/demo_apps/tof_demo`
- Board overlay: `sdk_example/mcuxsdk_examples_overlay/_boards/frdmmcxn947/demo_apps/tof_demo`
- Workspace (generated): `mcuxsdk_ws/`

## Notes
- If TMF8828 data is unavailable, the demo shows an animated fallback pattern so the LCD path can still be verified.
- Color mapping is RGB565 and scales near/far ranges into a blue-to-red heatmap.

## Debug Port Checks (Recommended)
FRDM-MCXN947 has a built-in MCU-Link debug port. Use it to verify ToF data
presence first:

```bash
LinkServer probe '#1' dapinfo
ls -l /dev/serial/by-id
screen /dev/ttyACM0 115200
```

Runtime interpretation:
- `TOF demo: live 8x8 frames detected` and green top bar: live ToF stream is active
- fallback messages and red top bar: display is running, but no valid live ToF frame stream
