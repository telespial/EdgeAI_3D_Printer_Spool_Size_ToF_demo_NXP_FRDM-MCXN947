# Restore Points

This file is an append-only index of known-good restore points.

Rules:
- Do not delete old entries.
- Do not move an existing `GOLDEN_*` tag.
- For each golden tag, create a lock tag with the commit baked into the name.
- Keep a file-based failsafe artifact in `failsafe/` and keep `docs/failsafe.md` updated.

## How To Restore

```bash
cd /path/to/ToF__demo_NXP_FRDM-MCXN947
git fetch --tags
git checkout GOLDEN_2026-02-12_v1_q1_grid_terminal_stable
./tools/setup_mcuxsdk_ws.sh
./tools/build_frdmmcxn947.sh debug
./tools/flash_frdmmcxn947.sh
```

## Golden Restore Points

### 2026-02-12 Golden (Initial Stable Q1 Grid + Terminal)
- Tag: `GOLDEN_2026-02-12_v1_q1_grid_terminal_stable`
- Lock tag: `GOLDEN_LOCK_2026-02-12_v1_66ccdbe`
- Commit: `66ccdbed3a348b6c7384a20b2320835a34b78e01`
- Hardware: FRDM-MCXN947 + LCD-PAR-S035 + TMF8828_EVM_EB_SHIELD
- Behavior:
  - 8x8 grid and tiny terminal are both in Q1 with aligned width.
  - Terminal shows live range/avg/actual/status fields.
  - Draw pacing capped for responsive updates; corner dead-cell mitigation applied.
- Failsafe artifact: see `docs/failsafe.md`.

## Template

### YYYY-MM-DD Name
- Tag: `TAG_NAME`
- Lock tag: `LOCK_TAG_WITH_SHA`
- Commit: `FULL_SHA`
- Hardware:
- Behavior:
- Notes:
