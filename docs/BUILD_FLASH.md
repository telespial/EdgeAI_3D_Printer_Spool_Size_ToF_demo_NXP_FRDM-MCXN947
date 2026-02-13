# Build And Flash

Current baseline tag: `GOLDEN_2026-02-13_v6_detection_rewrite_stable_states`

## Prerequisites
- Ubuntu toolchain bootstrap complete
- NXP LinkServer installed and on `PATH`

## Setup
```bash
./tools/setup_mcuxsdk_ws.sh
```

## Build
```bash
./tools/build_frdmmcxn947.sh debug
```

## Flash
```bash
./tools/flash_frdmmcxn947.sh
```

## Verify ToF Through Built-In Debug Port
After flashing, keep the board connected on the debug USB port and open the
virtual COM port:

```bash
ls -l /dev/serial/by-id
screen /dev/ttyACM0 115200
```

Look for these runtime markers:
- `TOF: TMF8828 chip=0x.. rev=.. ready on ... (8x8 mode)`
- `TOF demo: live 8x8 frames detected`
- Terminal line `LIVE:1` in the lower half of Q1 confirms live frame flow.

If you see fallback markers instead:
- `TOF demo: TMF8828 fallback mode`
- `TOF: TMF8828 not responding at 0x41/0x42/0x43 on FC2/FC3`
- `TOF demo: live stream timeout, waiting for stream`

then live ToF data is not currently reaching the app pipeline.

## AI Dataset Logging (Optional)
Compile-time flags in `src/tof_demo.c`:
- `TOF_AI_DATA_LOG_ENABLE` (`1` in current debug baseline)
- `TOF_AI_DATA_LOG_FULL_FRAME` (`1` in current debug baseline)

When enabled, UART emits:
- `AI_CSV,...` compact feature rows
- `AI_F64,...` full 64-cell rows (if full-frame flag enabled)

## Expected Binary
- `mcuxsdk_ws/build/tof_demo_cm33_core0.bin`

## Restore Points
- Golden restore index: `docs/RESTORE_POINTS.md`
- Failsafe pointer: `docs/failsafe.md`

Flash pinned failsafe image:
```bash
./tools/flash_failsafe.sh "$(sed -n '1p' docs/failsafe.md)"
```
