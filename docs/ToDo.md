# To Do

## Scope
- Project: `ToF__demo_NXP_FRDM-MCXN947`
- Baseline: TP roll + bargraph UI is locked and restorable via golden/failsafe tags.
- New goal: start AI-based roll estimation pipeline and transition rendering to model output.

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

## Definition Of Done (AI Milestone)
- [ ] AI estimation stable enough to replace direct distance mapping for TP/bar UI.
- [ ] Dataset capture + training procedure documented and repeatable.
- [ ] New golden + failsafe restore point created for first AI-enabled release.
