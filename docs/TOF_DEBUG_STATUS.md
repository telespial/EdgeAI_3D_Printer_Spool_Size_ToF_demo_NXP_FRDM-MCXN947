# ToF Debug Status (TMF8828 + FRDM-MCXN947 + LCD-PAR-S035)

Date: 2026-02-12
Project: ToF__demo_NXP_FRDM-MCXN947

## Goal
- Show stable, responsive 8x8 distance heatmap on LCD.
- All cells should react coherently for broad/flat targets.

## Current Observed Behavior
- Sensor initializes and app reports live stream.
- Status bar is green (live path active).
- Main bug still present: pixels remain black / non-reactive in user test.

## Confirmed Working Pieces
- Board boots and runs app binary.
- TMF8828 probe/selection and measurement start path works.
- UART startup logs show:
  - short-range mode enabled
  - TMF8828 detected on FC2 @ 0x41
  - "live 8x8 frames detected"

## Implemented Changes So Far
1. Bus robustness and startup
- Added FC2/FC3 recovery and reattach logic.
- Added forced EN handling.
- Files: `src/tmf8828_quick.c`, overlay pin/hardware init files.

2. 8x8 accumulation and rendering flow
- Added capture-mask accumulation logic for 4 subcaptures.
- Added complete-frame indication in API:
  - `tmf8828_quick_read_8x8(uint16_t out_mm[64], bool *out_complete)`
- Files: `src/tmf8828_quick.h`, `src/tmf8828_quick.c`, `src/tof_demo.c`.

3. Display persistence/smoothing attempts
- Added filtered frame path, hole fill, persistence, and spatial smoothing.
- Added composed display buffer (`s_display_mm`) and age tracking.
- File: `src/tof_demo.c`.

4. Reference alignment with TMF8828 Arduino v14 package
- Config page write aligned to v14 style block write at `0x24..0x2F`.
- SPAD map forced to 15 in 8x8 mode.
- ALG setting set to `0x84` (log confidence mode), histogram dump disabled.
- File: `src/tmf8828_quick.c`.

5. Critical parser fix applied
- Corrected object-byte layout to match v14:
  - object record bytes are `confidence, distance_lsb, distance_msb`
- Updated decode offsets accordingly.
- File: `src/tmf8828_quick.c`.

## What Is Still Broken
- User reports no reactive pixels despite green status/live logs.
- This indicates data path mismatch remains between measurement payload and displayed frame values.

## Most Likely Remaining Root Causes
1. Zone extraction mapping mismatch
- Current code assumes a specific 8x8 subcapture-to-zone mapping and skips raw slots 8 and 17.
- If this mapping does not match the running firmware payload format, values can collapse to invalid/zero.

2. Result validity gating too strict for live payload
- Invalid/zero handling plus smoothing may still suppress sparse but valid samples into black output.

3. Measurement payload semantics mismatch
- `result_number`/capture sequencing may differ from current assumptions on this firmware revision.

## Recommended Next Debug Pass (deterministic)
1. Add temporary UART diagnostics per received result page:
- result number, capture id, valid-count before filtering, min/max raw distance, confidence stats.

2. Add bypass mode (compile-time) to draw raw decoded distances directly:
- no persistence/fill/smoothing, no hotspot logic.
- Confirms sensor decode independently from UI filtering.

3. Verify and correct zone index mapping by comparing sequential captures against known v14 output ordering.

4. Once raw mode is confirmed, re-enable smoothing incrementally (one layer at a time).

## Repo State Note
- This status reflects the current debug branch state in this repo and is intended as a checkpoint before next instrumentation pass.
