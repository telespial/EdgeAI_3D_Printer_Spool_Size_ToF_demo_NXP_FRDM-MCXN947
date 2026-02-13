# AGENTS

This repository is a standalone firmware deliverable.

Read order for any agent session:
1. `docs/START_HERE.md`
2. `docs/PROJECT_STATE.md`
3. `docs/OPS_RUNBOOK.md`
4. `docs/HARDWARE_SETUP.md`
5. `docs/BUILD_FLASH.md`
6. `docs/RESTORE_POINTS.md`
7. `docs/COMMAND_LOG.md`

Operational rules:
- Keep this repository self-contained and portable.
- Do not add references to external private workspace paths.
- Use project-local scripts from `tools/` for setup/build/flash.
- After any code, config, build, flash, or release change, update:
  - `docs/PROJECT_STATE.md`
  - `STATUS.md`
- After command execution, append an entry to `docs/COMMAND_LOG.md`.
- When creating a new baseline restore point, update:
  - `docs/RESTORE_POINTS.md`
  - `docs/failsafe.md`
  - `failsafe/` artifact
  - git tags (`GOLDEN_*` and matching `GOLDEN_LOCK_*`)
