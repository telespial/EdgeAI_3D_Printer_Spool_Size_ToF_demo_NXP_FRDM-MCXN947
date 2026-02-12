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

If you see fallback markers instead:
- `TOF demo: TMF8828 fallback mode`
- `TOF: TMF8828 not responding at 0x41 on FC2/FC3`
- `TOF demo: live stream timeout, using fallback`

then live ToF data is not currently reaching the app pipeline.

## Expected Binary
- `mcuxsdk_ws/build/tof_demo_cm33_core0.bin`
