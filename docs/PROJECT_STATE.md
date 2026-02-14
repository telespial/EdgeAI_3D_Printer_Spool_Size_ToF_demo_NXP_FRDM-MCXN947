# Project State

Last updated: 2026-02-14
Project: `EdgeAI_3D_Printer_Spool_Size_ToF_demo_NXP_FRDM-MCXN947`

## Current Baseline
- Release baseline: v1 recovered spool baseline
- Golden tag: `GOLDEN_2026-02-14_v1_spool_recovered_baseline`
- Lock tag: `GOLDEN_LOCK_2026-02-14_v1_27eb7fa`
- Golden commit: `27eb7fae1a5915252ecda34f4ab6a7652aca32cb`
- Failsafe image: `failsafe/FAILSAFE_2026-02-14_v1_spool_recovered_baseline.elf`
- Lifecycle: active baseline checkpoint

## Firmware Behavior (Current)
- 8x8 heatmap + tiny terminal in left column (Q1).
- 3D printer spool render and bargraph across merged right-side area.
- 3D printer spool model path is deterministic with AI-assisted runtime fusion when AI is ON.
- Roll-state output constrained to 8-segment bargraph and four states (`FULL/MED/LOW/EMPTY`).
- Empty detection uses MM hysteresis plus sparse/no-surface fallback.
- AI ON path applies confidence-weighted estimator fusion to `actual_mm` and fullness.
- Debug terminal tail:
  - `AI:x A:mm`
  - `CONF:%`
- Alert runtime default is OFF at boot.

## Handoff Notes
- Primary source files:
  - `src/tof_demo.c`
  - `src/tmf8828_quick.c`
- Next work queue: `docs/ToDo.md`

## Update 2026-02-14
- Recovered spool renderer code path from known working source state (`ToF__demo` commit `23a3372`) into this repository.
- Revalidated build+flash against FRDM-MCXN947 using:
  - `BUILD_DIR=mcuxsdk_ws/build_spool ./tools/build_frdmmcxn947.sh debug`
  - `BUILD_DIR=mcuxsdk_ws/build_spool ./tools/flash_frdmmcxn947.sh`
- Published v1 spool failsafe and restore metadata in this repository.

## Update 2026-02-13
- Added host-side timestamped capture utility: `tools/capture_ai_csv.sh`.
- Updated runbook docs with capture workflow:
  - `docs/OPS_RUNBOOK.md`
  - `docs/BUILD_FLASH.md`
- Marked capture-helper task complete in `docs/ToDo.md`.

## Update 2026-02-13
- Added spool image asset set for 8 discrete levels in `assets/`:
  - `spool_level_8.png` (full) -> `spool_level_1.png` (near-empty)
- Source image retained as `assets/ornage.webp`.

## Update 2026-02-13
- Regenerated `assets/spool_level_8.png` -> `assets/spool_level_1.png`:
  - removed lower-right logo/text area,
  - kept source resolution (`1200x1200`) across all levels,
  - used texture-preserving depletion (no flat black paint fill).
