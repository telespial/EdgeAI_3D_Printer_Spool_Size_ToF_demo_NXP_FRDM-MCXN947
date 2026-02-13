# To Do

## Scope
- Project: `ToF__demo_NXP_FRDM-MCXN947`
- Baseline: TP roll + bargraph UI is locked and restorable via golden/failsafe tags.
- New goal: prioritize low-latency deterministic TP estimation with lightweight self-calibration, while keeping AI logging available.

## Backlog Reset
- Prior low-level decode/mapping tasks are complete and archived in:
  - `docs/TOF_DEBUG_STATUS.md`
  - `docs/RESTORE_POINTS.md`

## AI Workstream

### P0: Data Pipeline Bring-Up
- [x] Add firmware-side AI dataset log scaffold in `src/tof_demo.c`:
  - compile flags:
    - `TOF_AI_DATA_LOG_ENABLE`
    - `TOF_AI_DATA_LOG_FULL_FRAME`
  - periodic `AI_CSV` log with features (`valid/min/max/avg/actual/center/edge/fullness`).
- [ ] Add host capture helper script to collect timestamped UART datasets into `artifacts/ai_data/`.
- [ ] Define labeling template (`distance_mm`, `paper_percent`, scene notes, occlusion flag).
- [ ] Collect first calibration set:
  - empty roll, half roll, full roll
  - hand/no-hand
  - near-mid-far placements

### P1: Feature + Model Prototyping (Host)
- [ ] Build Python training notebook/script for baseline models:
  - linear regressor (sanity baseline)
  - tiny MLP regressor/classifier (candidate deployment model)
- [ ] Evaluate model error and stability:
  - MAE for `paper_percent`
  - frame-to-frame jitter under static target
- [ ] Freeze first deployable int8 model and export metadata.

### P1: Firmware Inference Integration
- [ ] Add compile-time inference hook in firmware (`AI_MODE=off|log|infer`).
- [ ] Integrate model runtime (TFLM/CMSIS-NN path) and feed extracted features.
- [ ] Add confidence gating:
  - when confidence low, hold last good value
  - fallback to deterministic estimator if model unavailable.

### P2: UX and Validation
- [ ] Drive TP paper thickness + bargraph from AI-estimated `paper_percent`.
- [ ] Add on-screen tiny AI debug fields:
  - `AI%`, `CONF`, `SRC` (`AI` vs fallback)
- [ ] Validate latency target:
  - TP/bar update at `~100 ms`
  - no visible wipe/flicker during hold.

## Adaptive Filter + Self-Training Workstream (Primary)

### P0: Estimator Bring-Up
- [x] Add robust TP estimator using 8x8 trimmed-mean signal (invalid rejection + center weighting).
- [x] Add confidence score from valid-count and spread.
- [x] Add adaptive temporal smoothing (fast response on real changes, slower on steady signal).
- [x] Add bounded self-calibration of near/far TP references during high-confidence stable windows.

### P1: Runtime UX
- [x] Add tiny terminal AI/estimator fields (`AI`, estimated mm, confidence, fullness, adaptive range).
- [x] Add touch-toggle `AI ON/OFF` pill below terminal in Q1 debug region.
- [ ] Tune estimator thresholds to reduce jitter at close-range hand occlusion.

### P2: Validation
- [ ] Validate `50..150 mm` operating behavior for persistent TP roll + bargraph updates.
- [ ] Record acceptance trace for steady hold and fast hand motion.

## Definition Of Done (AI Milestone)
- [ ] AI estimation stable enough to replace direct distance mapping for TP/bar UI.
- [ ] Dataset capture + training procedure documented and repeatable.
- [x] New golden + failsafe restore point created for first AI-enabled release.
