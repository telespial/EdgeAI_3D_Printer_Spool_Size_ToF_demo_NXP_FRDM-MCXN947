# ToF Demo (FRDM-MCXN947)

Build-ready scaffold for a Time-of-Flight demo on FRDM-MCXN947 using:
- FRDM-MCXN947 board
- LCD-PAR-S035 display (J8)
- TMF8828_EVM_EB_SHIELD (I2C)

This repo is intentionally initialized as a project skeleton:
- `src/tof_demo.c` provides a minimal firmware entrypoint.
- `sdk_example/` provides an MCUX SDK overlay for `demo_apps/tof_demo`.
- `tools/` contains setup/build/flash scripts.

## Quickstart
1. `./tools/bootstrap_ubuntu_user.sh`
2. `./tools/setup_mcuxsdk_ws.sh`
3. `./tools/build_frdmmcxn947.sh debug`
4. `./tools/flash_frdmmcxn947.sh`

## Key Paths
- Application source: `src/tof_demo.c`
- MCUX overlay app: `sdk_example/mcuxsdk_examples_overlay/demo_apps/tof_demo`
- Board overlay: `sdk_example/mcuxsdk_examples_overlay/_boards/frdmmcxn947/demo_apps/tof_demo`
- Workspace (generated): `mcuxsdk_ws/`

## Notes
- This skeleton uses the same FRDM board init and pinmux baseline as existing local demos.
- The ToF sensor application logic can be added incrementally in `src/`.
