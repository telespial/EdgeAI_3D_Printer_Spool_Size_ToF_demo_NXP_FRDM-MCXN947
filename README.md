# ToF Demo (FRDM-MCXN947)

This firmware drives a real-time spool state UI from a TMF8828 8x8 ToF sensor on NXP FRDM-MCXN947.

Hardware target:
- FRDM-MCXN947
- LCD-PAR-S035 display (J8)
- TMF8828_EVM_EB_SHIELD (I2C)

Core runtime outputs:
- 8x8 ToF heatmap + debug panel (left column)
- spool roll visualization + 8-step bargraph (right side)
- four-state classification: `FULL`, `MEDIUM`, `LOW`, `EMPTY`
- warning popups (forced top layer)

## What This System Does

1. Reads live TMF8828 8x8 distance frames.
2. Computes robust roll-distance metrics from frame statistics.
3. Maps those metrics to a deterministic state machine.
4. Renders:
- fixed brown core TP tube
- white paper thickness from 8 discrete levels (`0..8`)
- roll status banner and warning popup

Additional UI features:
- popup warning layer is always frontmost.
- upper-right watermark: `©Richard Haberkern` (white text/symbol).

## Measurement Pipeline

Main path in `src/tof_demo.c` (`tof_update_spool_model`):

1. Frame ingest:
- uses live 8x8 frame from TMF8828 read path (`src/tmf8828_quick.c`).

2. Derived measurements:
- `closest_mm`: nearest valid cell
- `curve_mm`: row-wise robust curve distance
- `avg_mm`: average of valid cells
- `valid_count`: count of valid cells

3. Deterministic detection rules:
- sparse-full candidate detection
- hard-empty candidate detection
- sparse/no-surface empty fallback
- explicit mm hysteresis around empty boundary

4. Model output:
- `actual_mm` decision first
- smoothed model (`s_tp_mm_q8`) second
- fullness converted to Q10 (`0..1024`)
- bargraph quantized to 8 segments

5. State mapping:
- segment-based + hysteresis + short consensus
- outputs only `FULL`, `MEDIUM`, `LOW`, `EMPTY`

## AI Usage and How AI Works Here

AI in this firmware has three concrete roles:

1. `AI runtime assist (when AI is ON)`:
- confidence-weighted fusion is applied to TP model distance (`actual_mm`) and fullness.
- this uses estimator confidence and learned bounds to reduce jitter and improve transition stability.
- safety overrides still win first (hard-empty and sparse-full rules are preserved).

2. `AI heatmap denoise path (when AI is ON)`:
- spatial/temporal denoise and hold logic for the 8x8 grid.
- used for cleaner live measurements in the UI and metric extraction path.

3. `AI data logging / debug`:
- UART telemetry for analysis:
  - `AI_CSV,...` compact per-frame features
  - `AI_F64,...` full 64-cell frame dumps

Important boundary:
- this is still a deterministic embedded pipeline, not a deployed neural-network classifier.
- AI ON adds data-driven fusion and denoise behavior; AI OFF uses pure baseline deterministic path.

## Roll-Size Training Data Workflow

Roll-size datasets were captured from real hardware scenarios:
- full roll
- medium roll
- low roll
- empty tube
- no roll

How that data was used:
- capture features and full-frame samples with `AI_CSV` and `AI_F64`.
- compare AI ON/OFF behavior across the same physical states.
- tune thresholds, hysteresis, sparse/full-empty overrides, and confidence fusion weights.
- validate final mappings against the 8-segment bargraph and four user-visible states.

How that data was **not** used:
- no on-device ML training is running in production firmware.
- no opaque model inference replaces the deterministic state machine.

## TP Roll Rendering Model (8 Renders)

The roll renderer now behaves as:
- fixed brown core size across all levels.
- white paper thickness scales with discrete bar level `0..8`.
- level `0`: brown tube only (no white paper).
- level `8`: maximum white paper thickness.

This gives visually deterministic roll states aligned with the bargraph.

## Project Layout

- `src/tof_demo.c`:
  - TP measurement/state pipeline
  - render logic and warning popup layering
  - UART AI logs
- `src/tmf8828_quick.c`:
  - TMF8828 init/read and packet assembly
- `src/par_lcd_s035.c`:
  - LCD panel driver glue
- `src/platform/display_hal.*`:
  - display abstraction
- `tools/`:
  - setup/build/flash scripts
- `docs/`:
  - restore points, runbooks, status
- `failsafe/`:
  - pinned ELF recovery images

## Build and Flash

```bash
./tools/bootstrap_ubuntu_user.sh
./tools/setup_mcuxsdk_ws.sh
./tools/build_frdmmcxn947.sh debug
./tools/flash_frdmmcxn947.sh
```

If normal flash path fails during rebuild step, use:

```bash
west flash -d build -r linkserver --skip-rebuild
```

## Debug UART

```bash
LinkServer probe '#1' dapinfo
ls -l /dev/serial/by-id
screen /dev/ttyACM0 115200
```

Useful markers:
- `TOF demo: live 8x8 frames detected`
- `AI_CSV,...`
- `AI_F64,...` (when enabled)
- `TOF AI: ON/OFF`
- debug tail:
  - `AI:<0|1> A:<mm>`
  - `CONF:<confidence%>`

## Restore and Release Hygiene

Canonical references:
- `docs/START_HERE.md`
- `docs/PROJECT_STATE.md`
- `docs/OPS_RUNBOOK.md`
- `docs/RESTORE_POINTS.md`
- `docs/failsafe.md`
- `STATUS.md`

Always use:
- one `GOLDEN_*` tag
- one matching `GOLDEN_LOCK_*` tag
- one pinned failsafe ELF in `failsafe/`

## Current Baseline

- Golden tag: `GOLDEN_2026-02-13_v9_ai_confline_runtime_assist`
- Lock tag: `GOLDEN_LOCK_2026-02-13_v9_<commit>`
- Failsafe: `failsafe/FAILSAFE_2026-02-13_v9_ai_confline_runtime_assist.elf`
