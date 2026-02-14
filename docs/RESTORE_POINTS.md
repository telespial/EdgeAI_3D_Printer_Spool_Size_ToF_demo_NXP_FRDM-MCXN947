# Restore Points

This file is an append-only index of known-good restore points.

Rules:
- Do not delete old entries.
- Do not move an existing `GOLDEN_*` tag.
- For each golden tag, create a lock tag with the commit baked into the name.
- Keep a file-based failsafe artifact in `failsafe/` and keep `docs/failsafe.md` updated.

## How To Restore

```bash
cd /path/to/EdgeAI_3D_Printer_Spool_Size_ToF_demo_NXP_FRDM-MCXN947
git fetch --tags
git checkout GOLDEN_2026-02-14_v1_spool_recovered_baseline
BUILD_DIR=mcuxsdk_ws/build_spool ./tools/build_frdmmcxn947.sh debug
BUILD_DIR=mcuxsdk_ws/build_spool ./tools/flash_frdmmcxn947.sh
```

## Golden Restore Points

### 2026-02-14 Golden (Recovered Spool Baseline)
- Tag: `GOLDEN_2026-02-14_v1_spool_recovered_baseline`
- Lock tag: `GOLDEN_LOCK_2026-02-14_v1_<commit>`
- Commit: `<commit>`
- Status: `CURRENT BASELINE`
- Hardware: FRDM-MCXN947 + LCD-PAR-S035 + TMF8828_EVM_EB_SHIELD
- Behavior:
  - Restores spool-oriented renderer behavior from known working code state.
  - Includes red filament spool visuals and `Replace Spool To Reset` alert hint.
  - Build and flash revalidated on target using project-local scripts.
- Failsafe artifact: `failsafe/FAILSAFE_2026-02-14_v1_spool_recovered_baseline.elf`
