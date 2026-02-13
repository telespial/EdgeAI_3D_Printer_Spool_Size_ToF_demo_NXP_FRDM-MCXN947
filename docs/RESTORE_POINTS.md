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
git checkout GOLDEN_2026-02-13_v5_full_reacquire_alertoff
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

### 2026-02-13 Golden (35-65mm Calibrated Roll States + Reset Recovery)
- Tag: `GOLDEN_2026-02-13_v4_mm35_65_calibrated_reset`
- Lock tag: `GOLDEN_LOCK_2026-02-13_v4_d29464d`
- Commit: `d29464da7ec1725b95cb2044b9be62544387657b`
- Hardware: FRDM-MCXN947 + LCD-PAR-S035 + TMF8828_EVM_EB_SHIELD
- Behavior:
  - Roll calibration mapped to physical thresholds:
    - full at `<35 mm`
    - empty at `>65 mm`
  - Bargraph and TP roll geometry rescaled to match the same `35..65 mm` range.
  - Roll state bands updated for consistent status transitions:
    - full `<=35 mm`, medium `36..50 mm`, low `51..65 mm`, empty `>65 mm`.
  - AI on/off path aligned with no extra AI-only distance bias.
  - Empty-hold recovery improved so new-roll detection clears and rearms reliably.
- Failsafe artifact: see `docs/failsafe.md`.

### 2026-02-13 Golden (Full-Roll Reacquire + Alert Default OFF)
- Tag: `GOLDEN_2026-02-13_v5_full_reacquire_alertoff`
- Lock tag: `GOLDEN_LOCK_2026-02-13_v5_920a5d8`
- Commit: `920a5d8528a59bb328bcbc6f5b5b2692b7bb9843`
- Status: `FINAL UPDATE` (frozen handoff baseline)
- Hardware: FRDM-MCXN947 + LCD-PAR-S035 + TMF8828_EVM_EB_SHIELD
- Behavior:
  - Alert runtime default changed to OFF at boot/reset.
  - TP model switched to closest-valid pixel for roll state decisions (not range-gated closest).
  - Full-roll reacquire path hardened after medium/low transitions with wider capture + consensus windows.
  - Bargraph/roll geometry and status derive from the same calibrated 35..60 mm mapping path.
- Failsafe artifact: see `docs/failsafe.md`.

## Template

### YYYY-MM-DD Name
- Tag: `TAG_NAME`
- Lock tag: `LOCK_TAG_WITH_SHA`
- Commit: `FULL_SHA`
- Hardware:
- Behavior:
- Notes:
