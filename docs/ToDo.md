# To Do

## Priority 0 (Stability / Calibration)
- [ ] Run extended repeated swap validation:
  - sequence: `full -> medium -> low -> full`
  - minimum 20 full cycles
  - verify full always re-locks after low state
- [ ] Capture terminal snapshots at each state transition (`AVG`, `A`, `FQ`, status label).
- [ ] Tune remaining edge thresholds only if repeated-swap test fails.

## Priority 1 (State Consistency)
- [ ] Confirm AI ON and AI OFF produce equivalent state boundaries under same physical setup.
- [ ] Verify bargraph percentage and status label always move together.
- [ ] Verify empty-hold clears on confirmed new full roll across repeated cycles.

## Priority 2 (AI/Data Workflow)
- [ ] Keep lightweight AI denoise influence path for grid stability and log review.
- [ ] Collect clean UART dataset (`AI_CSV`) for full/medium/low/empty checkpoints.
- [ ] Add host capture helper script under `tools/` for timestamped CSV logging.

## Priority 3 (Docs / Release Hygiene)
- [ ] Keep `docs/PROJECT_STATE.md`, `STATUS.md`, and `docs/COMMAND_LOG.md` current after each session.
- [ ] Create v6 golden/failsafe restore point after stability test acceptance.
