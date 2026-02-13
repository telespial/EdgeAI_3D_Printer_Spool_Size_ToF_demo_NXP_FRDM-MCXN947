# Ops Runbook

## Purpose
Canonical operations for setup, build, and flash.

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

## Logging Requirements
- After command execution, append a concise entry in `docs/COMMAND_LOG.md`.
- After code/config/build/flash changes, update:
  - `docs/PROJECT_STATE.md`
  - `STATUS.md`
