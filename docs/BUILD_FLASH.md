# Build And Flash

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

## Expected Binary
- `mcuxsdk_ws/build/tof_demo_cm33_core0.bin`
