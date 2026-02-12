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
git checkout GOLDEN_2026-02-12_v3_tp_fast_persistent_ai_bootstrap
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

### 2026-02-12 Golden (spool Tube UI Across Segments 2+3)
- Tag: `GOLDEN_2026-02-12_v2_spool_tube_segments23`
- Lock tag: `GOLDEN_LOCK_2026-02-12_v2_28f9659`
- Commit: `28f965992ac85df82d25cf77d50d72b0d01a7dfb`
- Hardware: FRDM-MCXN947 + LCD-PAR-S035 + TMF8828_EVM_EB_SHIELD
- Behavior:
  - Q1 keeps the 8x8 grid (upper half) and tiny runtime terminal (lower half).
  - Divider between segments 2 and 3 is removed and both segments are rendered as one canvas.
  - A colorful 3D spool tube is rendered across full segment 2+3 height.
  - Tube diameter is distance-driven and set to full roll when distance is `<=50 mm` or `>=100 mm`.
  - `docs/spool.png` is ignored and not tracked.
- Failsafe artifact: see `docs/failsafe.md`.

### 2026-02-12 Golden (Fast Persistent TP + AI Bootstrap)
- Tag: `GOLDEN_2026-02-12_v3_tp_fast_persistent_ai_bootstrap`
- Lock tag: `GOLDEN_LOCK_2026-02-12_v3_89e625d`
- Commit: `89e625dc427de268a8cb0ffdf3a4a819a0530cc8`
- Hardware: FRDM-MCXN947 + LCD-PAR-S035 + TMF8828_EVM_EB_SHIELD
- Behavior:
  - TP roll + bargraph response path tuned for smoother `~100 ms` updates.
  - Fixed enlarged brown core with variable white paper thickness.
  - Bargraph white tick/marker artifacts removed.
  - Persistent last-valid TP/bar state across short live gaps.
  - AI data logging scaffold added in firmware (`TOF_AI_DATA_LOG_ENABLE`, `TOF_AI_DATA_LOG_FULL_FRAME`).
- Failsafe artifact: see `docs/failsafe.md`.

## Template

### YYYY-MM-DD Name
- Tag: `TAG_NAME`
- Lock tag: `LOCK_TAG_WITH_SHA`
- Commit: `FULL_SHA`
- Hardware:
- Behavior:
- Notes:
