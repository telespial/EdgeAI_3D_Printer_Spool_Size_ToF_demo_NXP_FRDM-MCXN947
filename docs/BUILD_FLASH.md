# Build And Flash

## Prerequisites
- Ubuntu toolchain bootstrap complete
- NXP LinkServer installed and on `PATH`

Current baseline tag: `GOLDEN_2026-02-14_v1_spool_recovered_baseline`

## Setup
```bash
./tools/setup_mcuxsdk_ws.sh
```

## Build
```bash
BUILD_DIR=mcuxsdk_ws/build_spool ./tools/build_frdmmcxn947.sh debug
```

## Flash
```bash
BUILD_DIR=mcuxsdk_ws/build_spool ./tools/flash_frdmmcxn947.sh
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

## Timestamped AI CSV Capture
To collect timestamped host logs for dataset review:

```bash
./tools/capture_ai_csv.sh --port /dev/ttyACM0 --marker AI_CSV --raw
```

Outputs are written under `captures/` with UTC timestamped filenames.

## Restore Points
- Golden restore index: `docs/RESTORE_POINTS.md`
- Failsafe pointer: `docs/failsafe.md`

Flash pinned failsafe image:
```bash
./tools/flash_failsafe.sh "$(sed -n '1p' docs/failsafe.md)"
```
