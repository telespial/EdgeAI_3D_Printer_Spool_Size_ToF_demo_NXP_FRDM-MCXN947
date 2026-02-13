# ToF Debug Status (TMF8828 + FRDM-MCXN947 + LCD-PAR-S035)

Date: 2026-02-12
Project: ToF__demo_NXP_FRDM-MCXN947

## Update 2026-02-13 (v9 Golden AI Runtime Assist + CONF Line)

- New baseline restore point published:
  - `GOLDEN_2026-02-13_v9_ai_confline_runtime_assist`
  - `GOLDEN_LOCK_2026-02-13_v9_e7e2432`
  - failsafe image: `failsafe/FAILSAFE_2026-02-13_v9_ai_confline_runtime_assist.elf`
- AI ON now applies confidence-weighted estimator fusion to TP decision/fullness path.
- Deterministic safety overrides remain in force (`hard-empty` and `sparse-full`).
- Debug terminal tail now ends with:
  - `AI:x A:mm`
  - `CONF:%`

## Update 2026-02-13 (v8 Golden Readable Brand Font Finalization)

- New baseline restore point published:
  - `GOLDEN_2026-02-13_v8_brand_font_readable`
  - `GOLDEN_LOCK_2026-02-13_v8_1dccefd`
  - failsafe image: `failsafe/FAILSAFE_2026-02-13_v8_brand_font_readable.elf`
- Added a dedicated readable 5x7 brand glyph renderer for the top-right mark.
- Finalized text style/size/spacing for one-line `(C)RICHARD HABERKERN`.
- Preserves v7 TP state/render baseline behavior.

## Update 2026-02-13 (v7 Golden Popup-Front + Fixed-Core 8-Step Render)

- New baseline restore point published:
  - `GOLDEN_2026-02-13_v7_popup_front_fixedcore_8step`
  - `GOLDEN_LOCK_2026-02-13_v7_97aac50`
  - failsafe image: `failsafe/FAILSAFE_2026-02-13_v7_popup_front_fixedcore_8step.elf`
- Warning popup is forced to redraw as the frontmost layer while active.
- TP roll render now uses:
  - fixed-size brown core at every bar level
  - discrete white-paper thickness in 8 levels (`0..8`)
  - tube-only empty render at level `0`.
- Added upper-right white brand watermark (`©Richard Haberkern` symbol + text).
- Preserves v6 detection rewrite behavior:
  - AI on/off parity for TP state decisions
  - deterministic four-state output (`FULL`, `MEDIUM`, `LOW`, `EMPTY`).

## Update 2026-02-13 (v6 Golden Detection Rewrite)

- New baseline restore point published:
  - `GOLDEN_2026-02-13_v6_detection_rewrite_stable_states`
  - `GOLDEN_LOCK_2026-02-13_v6_12d2789`
  - failsafe image: `failsafe/FAILSAFE_2026-02-13_v6_detection_rewrite_stable_states.elf`
- TP measurement/detection path in `src/tof_demo.c` was rewritten.
- Output model now targets:
  - 8-segment bargraph
  - four states only: `FULL`, `MEDIUM`, `LOW`, `EMPTY`
- Empty-state hysteresis and sparse/full-empty fallback logic were added for removal/reinsert transitions.
- TP state input path is now AI-mode invariant to reduce AI ON/OFF divergence.

## Update 2026-02-13 (v5 Golden Handoff)

- New baseline restore point published:
  - `GOLDEN_2026-02-13_v5_full_reacquire_alertoff`
  - `GOLDEN_LOCK_2026-02-13_v5_920a5d8`
  - failsafe image: `failsafe/FAILSAFE_2026-02-13_v5_full_reacquire_alertoff.elf`
- Alert runtime default changed to OFF.
- Roll state path now prioritizes closest-valid pixel for TP model decisions.
- Full-roll reacquire guard was widened for repeated swap sequences (`full -> low -> full`).
- Calibration baseline in current code:
  - full anchor `<=35 mm`
  - empty trigger `>60 mm`
- Remaining verification focus:
  - extended repeated-swap runs to confirm full-state re-lock over long cycles.

## Goal
- Show stable, responsive 8x8 distance heatmap on LCD.
- All cells should react coherently for broad/flat targets.

## Current Checkpoint
- Sensor init and live markers are stable:
  - short-range mode enabled
  - TMF8828 ready on FC2 @ `0x41`
  - raw draw mode enabled
  - live 8x8 frames detected
- Raw render mode is active (`TOF_DEBUG_RAW_DRAW=1`) so LCD draws unfiltered frame values.
- Per-packet UART diagnostics are active in compact format:
  - `TOF PKT r=<res> s=<seq> c=<cap> ... d=<min>-<max> ... u=<updated>`

## Latest Run Evidence
- Build: `./tools/build_frdmmcxn947.sh debug`
- Flash: `./tools/flash_frdmmcxn947.sh`
- Synthetic sweep artifact: `/tmp/tof_synth_sweep_20260212_064024.tsv`
  - map 1 (quadrants), map 2 (row bands), and map 3 (column bands) all show expected geometric signatures in `TOF SYN` traces.
- Live map sweep artifact: `/tmp/tof_live_maps_20260212_064230.tsv`
  - best map mode under current score metric: `TMF8828_ZONE_MAP_MODE=1` (quadrants).
- Live remap sweep artifact: `/tmp/tof_live_remap_m1_20260212_064438.tsv`
  - best one-pass score: remap `[1,3,0,2]`.
- Controlled retest artifact: `/tmp/tof_live_verify_20260212_065740.tsv`
  - retested top candidates in same environment:
    - `[1,3,0,2]` score `1.1438` (best in retest)
    - `[3,0,2,1]` score `0.9979`
    - `[3,2,1,0]` score `0.9378`
    - `[3,1,2,0]` score `0.8075`
- Final lock confirmation trace: `/tmp/tof_live_final_lock_20260212_070016.log`
  - startup marker: `TOF: map mode=1 remap=[1,3,0,2]`
  - parsed packet metrics (20s): `rows=535`, `full_ratio=0.7031`, `avg_upd=2.1944`, `std_upd=1.3155`, `cap2_zero_frac=0.3790`, `score=1.1541`
- Dual-range conflict follow-up trace: `/tmp/tof_dualrange_fix_20260212_070916.log`
  - startup marker: `TOF: map mode=1 remap=[1,3,0,2]`
  - parsed packet metrics (24s): `rows=698`, `full_ratio=0.9219`, `avg_upd=5.0702`, `std_upd=0.5685`, `cap2_zero_frac=0.0000`, `score=5.6080`
  - object-selection diagnostics: `obj1_sel_frac=0.6431`, `dual_split_frac=0.0238` (large dual-return splits are rare in this run)
- 10cm hand-cover stability patch trace: `/tmp/tof_stability_patch_20260212_071837.log`
  - startup marker confirms lock: `TOF: map mode=1 remap=[1,3,0,2]`
  - raw mode now draws on complete cycles only and keeps last frame across short live gaps to remove wipe/blank flashes.
- 10cm hand-cover stability patch v2 trace: `/tmp/tof_stability_patch2_20260212_073705.log`
  - startup marker confirms lock: `TOF: map mode=1 remap=[1,3,0,2]`
  - no timeout/restart/reinit markers observed in 28s capture window.
- Complete-frame burst preservation fix trace: `/tmp/tof_complete_frame_fix_20260212_074255.log`
  - startup marker confirms lock: `TOF: map mode=1 remap=[1,3,0,2]`
  - no timeout/restart/reinit markers observed in 20s capture window.
- Tiny debugger UI layout trace: `/tmp/tof_debugpanel_20260212_075415.log`
  - 8x8 grid placed in top region.
  - bottom third of LCD reserved for compact runtime text debug panel.

## Firmware Changes In This Checkpoint
1. Throughput / trace robustness
- Reduced default measurement period for debug tracing pressure control (`TMF8828_MEAS_PERIOD_MS` default now `80`).
- Compacted packet log line format to reduce UART bandwidth.

2. Read-path robustness
- Added burst read drain loop in `src/tof_demo.c` (`TOF_READ_BURST_MAX`) to consume multiple pending packets before rendering.

3. All-zero packet suppression
- Added `drop0` behavior in `src/tmf8828_quick.c`:
  - if a subcapture packet is entirely zero-signal while current sequence already has valid updates, preserve last-good zones instead of aging/clearing.
  - this specifically mitigates intermittent cap-2 all-zero packet collapses.

4. Parser and map investigation
- Dist-first object-entry decode was tested, then reverted due worse live behavior.
- Current object-entry decode is confidence-first (`conf, dist_lsb, dist_msb`).
- Added object-selection policy in `src/tmf8828_quick.c` to reduce perceived range swapping:
  - `TMF8828_OBJECT_SELECT_POLICY=2` (strict obj0 only).
  - packet diagnostics now include object-source and dual-range split counters (`ob=... ds=...`).
- Added expected-range stabilization for close-hand operation:
  - expected range window set to `50..150 mm` (`TMF8828_EXPECTED_MIN_MM`, `TMF8828_EXPECTED_MAX_MM`)
  - out-of-range update rejection when previous zone is in-range (`TMF8828_RANGE_GLITCH_REJECT=1`).
- Close-saturation substitute raised to near-range floor (`TMF8828_TOO_CLOSE_MM=50`).
- Object selection hardened for close-range consistency:
  - `TMF8828_OBJECT_SELECT_POLICY=2` (strict obj0 only; ignore obj1 fallback).
- Added compile-time map controls in `src/tmf8828_quick.c`:
  - `TMF8828_ZONE_MAP_MODE` (0 interleaved, 1 quadrants, 2 row-bands, 3 column-bands)
  - `TMF8828_CAPTURE_REMAP_0..3` for capture-order remap tests.
- Added map-mode print at init for deterministic trace attribution.
- Locked current mapping in firmware: `map mode=1`, `remap=[1,3,0,2]`.

5. Synthetic regression harnesses
- Added independent synthetic input modes in `src/tof_demo.c`:
  - fixed 8x8 frame: `TOF_DEBUG_INPUT_MODE=TOF_INPUT_MODE_SYNTH_FIXED`
  - 4-subcapture assembly replay: `TOF_DEBUG_INPUT_MODE=TOF_INPUT_MODE_SYNTH_SUBCAP`
- Synthetic subcapture mode includes `TOF_SYNTH_MAP_MODE` for render/assembly map A/B checks independent of sensor read path.

6. Render pacing and blanking control
- Updated UI locked range to `50..150 mm` (`TOF_LOCKED_NEAR_MM`, `TOF_LOCKED_FAR_MM`).
- Enabled complete-cycle-only draw in live mode (`TOF_DRAW_ON_COMPLETE_ONLY=1`) to suppress visible per-subcapture banding.
- In raw mode, short live gaps no longer clear frame to black; last frame is held until fresh data arrives.
- Disabled auto restart/reinit while in raw debug mode (`TOF_ENABLE_AUTO_RECOVERY=0`) to avoid wipe/blank transitions during close-hand placement tests.
- Fixed burst-read complete-frame overwrite hazard in `src/tof_demo.c`:
  - when multiple packets arrive in one burst, the last complete frame is now preserved and used for draw.
  - this prevents rendering a newer partial packet (8x2-looking artifacts) immediately after a complete packet.
7. On-screen debug UX
- Reworked layout so heatmap is top-aligned and compact.
- Added a tiny text debugger panel in the bottom third of the screen with live metrics (`LIVE/GL/GC`, valid/min/max/avg, lock range, mode flags, hotspot index).

## Remaining Gap
- Final physical capture-to-zone orientation coherence still needs explicit broad/flat target sign-off.
- Cap-2 subcapture remains sparser than other captures in some runs; mapping is finalized for now, but filter tuning still pending.

## Next Deterministic Steps
1. Run broad/flat physical orientation sweep on LCD with locked map (`mode=1`, remap `[1,3,0,2]`) to validate the reported odd 8x4 sector behavior.
2. Re-enable filtering stages one-by-one (hold-last-valid, sparse fill, smoothing) with toggles and verify they do not reintroduce black collapse.
3. Tune stale/restart/reinit thresholds after filter re-enable and collect one final acceptance trace set.

## Update 2026-02-12 (Post-Layout Stabilization)

- Added 500 ms response cap for both draw-gap and terminal refresh pacing in `src/tof_demo.c`.
- Reworked Q1 layout sizing so the 8x8 grid and terminal use the same width footprint.
- Added raw-mode display-hole backfill (`tof_fill_display_holes`) to suppress isolated dead cells at edges/corners.
- Relaxed sparse-zone corner recovery threshold in `src/tmf8828_quick.c` (`count >= 2`) to avoid persistent corner dropouts.
- Restored tiny terminal text scale for readability while keeping the Q1 width alignment.

Observed behavior after patch sequence:
- 8x8 grid updates are responsive and stable under close-hand tests.
- Q1 terminal remains readable and aligned under the grid.
- User confirmation in-session: working baseline accepted.

Restore artifacts:
- Golden and failsafe restore points are now tracked in `docs/RESTORE_POINTS.md` and `docs/failsafe.md`.

## Update 2026-02-12 (TP Responsiveness + AI Bootstrap Kickoff)

- TP roll and bargraph rendering path was optimized for lower latency and smoother persistence:
  - local redraw region replacement instead of full segment repaint
  - reduced draw density in body/ring passes to lower render cost
  - retained target TP/bar update cadence around `100 ms`
- Bargraph white tick/marker artifacts were removed.
- Added AI data pipeline scaffolding in `src/tof_demo.c`:
  - compile flags: `TOF_AI_DATA_LOG_ENABLE`, `TOF_AI_DATA_LOG_FULL_FRAME`
  - new log rows:
    - `AI_CSV` feature rows (valid/min/max/avg/actual/center/edge/fullness)
    - `AI_F64` full 8x8 frame rows (optional)
- This establishes the first firmware-side step for model training data capture while preserving current deterministic UI behavior.
