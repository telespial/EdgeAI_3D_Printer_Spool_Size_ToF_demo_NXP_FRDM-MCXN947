# Ops Runbook

## Purpose
Canonical operations for setup, build, flash, restore, and release tagging.

## Environment Setup
```bash
./tools/bootstrap_ubuntu_user.sh
./tools/setup_mcuxsdk_ws.sh
```

## Build
```bash
./tools/build_frdmmcxn947.sh debug
```

## Flash Current Build
```bash
./tools/flash_frdmmcxn947.sh
```

## Flash Pinned Failsafe
```bash
./tools/flash_failsafe.sh "$(sed -n '1p' docs/failsafe.md)"
```

## Restore A Golden Baseline
```bash
git fetch --tags
git checkout GOLDEN_2026-02-13_v8_brand_font_readable
./tools/setup_mcuxsdk_ws.sh
./tools/build_frdmmcxn947.sh debug
./tools/flash_frdmmcxn947.sh
```

## Release / Restore-Point Procedure
1. Build and flash candidate firmware.
2. Confirm behavior on hardware.
3. Copy ELF into `failsafe/` with dated name.
4. Update `docs/failsafe.md` to point at new artifact.
5. Append entry in `docs/RESTORE_POINTS.md`.
6. Commit changes.
7. Create tags:
```bash
git tag GOLDEN_<name> <commit>
git tag GOLDEN_LOCK_<name>_<shortsha> <commit>
```
8. Push commit and tags.

## Logging Requirements
- After command execution, append a concise entry in `docs/COMMAND_LOG.md`.
- After code/config/build/flash/release changes, update:
  - `docs/PROJECT_STATE.md`
  - `STATUS.md`
