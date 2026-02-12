# To Do

## Scope
- Project: `ToF__demo_NXP_FRDM-MCXN947`
- Current state: live heatmap + terminal layout are stable in user validation.
- Primary goal: preserve this baseline and formalize restore points.

## P0: Decode And Mapping Validation

- [x] Add compile-time raw render mode in `src/tof_demo.c` (no smoothing, no persistence, no fallback blending).
  - Suggest flag: `TOF_DEBUG_RAW_DRAW`.
  - Acceptance: when live data is present, screen reflects raw zone values directly.

- [x] Add deterministic UART diagnostics in `src/tmf8828_quick.c` for each result packet:
  - `sequence`, `capture`, `result_number`
  - raw slot count used/skipped
  - valid-zone count before and after fill logic
  - min/max distance and confidence
  - Acceptance: one log line per packet during debug runs.
  - Checkpoint evidence: `/tmp/tof_uart_drop0_20260212_062234.log`

- [x] Verify capture-to-zone mapping in `tmf_zone_index_8x8()` against observed packet ordering.
  - Validate assumption that raw slots `8` and `17` are always unused.
  - Acceptance: broad flat target produces coherent spatial pattern, not sparse/random points.
  - Current checkpoint:
    - synthetic subcapture map sweep completed (`/tmp/tof_synth_sweep_20260212_064024.tsv`)
    - live map sweep completed (`/tmp/tof_live_maps_20260212_064230.tsv`)
    - live remap sweep + controlled retest completed (`/tmp/tof_live_remap_m1_20260212_064438.tsv`, `/tmp/tof_live_verify_20260212_065740.tsv`)
    - locked firmware mapping now `TMF8828_ZONE_MAP_MODE=1`, `TMF8828_CAPTURE_REMAP=[1,3,0,2]`
    - dual-range follow-up instrumentation added (`ob=... ds=...`) and latest trace shows low dual-split rate (`/tmp/tof_dualrange_fix_20260212_070916.log`)
    - close-range stabilization patch applied for 50..150 mm target window with complete-cycle-only draw + anti-wipe hold (`/tmp/tof_stability_patch_20260212_071837.log`)
    - close-range stabilization patch v2 applied: strict `obj0` selection + raw-mode auto-recovery disabled to suppress wipe/blank transitions (`/tmp/tof_stability_patch2_20260212_073705.log`)
    - burst-read complete-frame overwrite fix applied in `src/tof_demo.c` (preserve last complete packet frame for draw) to remove partial 8x2 artifacts (`/tmp/tof_complete_frame_fix_20260212_074255.log`)
  - Parser checkpoint: dist-first trial regressed in live traces and was reverted; current decode is confidence-first (`conf, dist_lsb, dist_msb`) in `src/tmf8828_quick.c`.
  - Final sign-off: user-confirmed working behavior after live physical tuning pass (including corner-hole mitigation).

## P0: Minimal Regression Harness

- [x] Add a temporary debug mode that replays a fixed synthetic 8x8 frame.
  - Keep this mode independent from TMF8828 read path.
  - Acceptance: verifies rendering path alone (color map + layout) is correct.
  - Implemented via `TOF_DEBUG_INPUT_MODE=TOF_INPUT_MODE_SYNTH_FIXED` in `src/tof_demo.c`.

- [x] Add a second synthetic mode with 4 subcaptures to exercise accumulation logic.
  - Acceptance: assembled frame is stable and complete after all 4 captures.
  - Implemented via `TOF_DEBUG_INPUT_MODE=TOF_INPUT_MODE_SYNTH_SUBCAP` with configurable synthetic map variants (`TOF_SYNTH_MAP_MODE`).

## P1: Filtering And Robustness Tuning

- [ ] Re-enable filtering in layers after raw mode is validated:
  - hold-last-valid
  - sparse fill
  - spatial smoothing
  - Acceptance: each layer can be toggled independently and does not collapse valid data to black.

- [ ] Tune stale/restart/reinit thresholds using live runs.
  - Acceptance: no oscillation between live and fallback for normal operating conditions.

## P1: Documentation Sync

- [x] Keep runtime marker strings in docs aligned with firmware prints.
- [x] Update `STATUS.md` after each debug checkpoint with:
  - exact behavior observed
  - build/flash command used
  - next deterministic step

## P2: Product Feature Track (After Live Heatmap Is Stable)

- [ ] Implement 2D circle fit from 8x8 points for roll-radius estimation.
- [ ] Add calibration flow (`R_empty`, `R_full`) and percent estimator.
- [ ] Optional: add tiny mask model (or rules-based mask) for occlusion robustness.

## Definition Of Done (Current Milestone)

- [x] Green live status with responsive 8x8 cells on broad/flat targets.
- [x] No persistent all-black frame when live stream is active.
- [x] Deterministic debug logs prove packet decode and mapping are correct.
- [x] `docs/TOF_DEBUG_STATUS.md` and `STATUS.md` updated with final evidence.
