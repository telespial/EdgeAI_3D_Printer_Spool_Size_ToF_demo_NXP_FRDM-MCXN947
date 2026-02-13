# Command Log

Format:
- `YYYY-MM-DD` | command group | result | notes

## 2026-02-13
- `2026-02-13` | `./tools/build_frdmmcxn947.sh debug` | PASS | Built current v5 firmware binary and ELF.
- `2026-02-13` | `./tools/flash_frdmmcxn947.sh` | PASS | Flashed FRDM-MCXN947 with v5 baseline.
- `2026-02-13` | `git tag/push` | PASS | Published `GOLDEN_2026-02-13_v5_full_reacquire_alertoff` and lock tag.
- `2026-02-13` | doc compliance audit commands (`ls`, `sed`, `rg`, `git status`) | PASS | Verified required files, restore metadata, and repository hygiene.
- `2026-02-13` | docs compliance updates (`cat`, `apply_patch`) | PASS | Added runbook/state/log docs and synchronized `README`, `START_HERE`, `STATUS`, `ToDo`, and debug status for handoff.
- `2026-02-13` | `git commit` + `git push origin main` | PASS | Published compliance/handoff documentation sync commit `8914dbe`.
- `2026-02-13` | docs finalization update | PASS | Marked v5 golden restore point as `FINAL UPDATE` for handoff freeze.
- `2026-02-13` | iterative `build` + `flash` during TP detection rewrite | PASS | Reworked TP measurement/state pipeline and validated on hardware with AI ON/OFF parity checks.
- `2026-02-13` | `cp mcuxsdk_ws/build/tof_demo_cm33_core0.elf failsafe/FAILSAFE_2026-02-13_v6_detection_rewrite_stable_states.elf` | PASS | Published v6 failsafe artifact.
- `2026-02-13` | docs baseline sync (`START_HERE`, `PROJECT_STATE`, `STATUS`, `BUILD_FLASH`, `RESTORE_POINTS`, `failsafe.md`, `README`) | PASS | Updated v6 golden/failsafe metadata and restore procedures.
- `2026-02-13` | `git commit` + `git tag` + `git push origin main --tags` | PASS | Published v6 golden release and lock tag.
- `2026-02-13` | docs tag-pin update + `git push origin main` | PASS | Replaced v6 `<commit>` placeholders with exact lock/commit IDs in baseline docs.
- `2026-02-13` | `./tools/build_frdmmcxn947.sh debug` | PASS | Rebuilt v7 candidate after popup layering/fixed-core 8-step render and brand watermark updates.
- `2026-02-13` | `cp mcuxsdk_ws/build/tof_demo_cm33_core0.elf failsafe/FAILSAFE_2026-02-13_v7_popup_front_fixedcore_8step.elf` | PASS | Published v7 failsafe artifact.
- `2026-02-13` | docs baseline sync (`README`, `STATUS`, `START_HERE`, `PROJECT_STATE`, `RESTORE_POINTS`, `TOF_DEBUG_STATUS`, `failsafe.md`) | PASS | Synced v7 golden/failsafe metadata and behavior notes.
- `2026-02-13` | `git commit` | PASS | Published v7 release payload commit `97aac50` (UI/render update + failsafe + docs sync).
- `2026-02-13` | `git tag` | PASS | Created `GOLDEN_2026-02-13_v7_popup_front_fixedcore_8step` and `GOLDEN_LOCK_2026-02-13_v7_97aac50`.
- `2026-02-13` | docs tag-pin update (`README`, `STATUS`, `START_HERE`, `PROJECT_STATE`, `RESTORE_POINTS`, `TOF_DEBUG_STATUS`) | PASS | Replaced v7 `<commit>` placeholders with exact lock/commit IDs.
- `2026-02-13` | `git push origin main` + `git push origin GOLDEN_2026-02-13_v7_popup_front_fixedcore_8step GOLDEN_LOCK_2026-02-13_v7_97aac50` | PASS | Published v7 release commits and restore tags to GitHub.
- `2026-02-13` | iterative `build` + `flash` for brand readability tuning | PASS | Refined top-right brand rendering to final one-line readable style.
- `2026-02-13` | `cp mcuxsdk_ws/build/tof_demo_cm33_core0.elf failsafe/FAILSAFE_2026-02-13_v8_brand_font_readable.elf` | PASS | Published v8 failsafe artifact.
- `2026-02-13` | docs baseline sync (`README`, `STATUS`, `START_HERE`, `PROJECT_STATE`, `RESTORE_POINTS`, `TOF_DEBUG_STATUS`, `BUILD_FLASH`, `OPS_RUNBOOK`, `failsafe.md`) | PASS | Prepared v8 golden/failsafe metadata and restore instructions.
- `2026-02-13` | `git commit` | PASS | Published v8 release payload commit `1dccefd`.
- `2026-02-13` | `git tag` | PASS | Created `GOLDEN_2026-02-13_v8_brand_font_readable` and `GOLDEN_LOCK_2026-02-13_v8_1dccefd`.
- `2026-02-13` | docs tag-pin update (`README`, `STATUS`, `START_HERE`, `PROJECT_STATE`, `RESTORE_POINTS`, `TOF_DEBUG_STATUS`) | PASS | Replaced v8 `<commit>` placeholders with exact lock/commit IDs.
- `2026-02-13` | `git push origin main` + `git push origin GOLDEN_2026-02-13_v8_brand_font_readable GOLDEN_LOCK_2026-02-13_v8_1dccefd` | PASS | Published v8 release commits and restore tags to GitHub.
- `2026-02-13` | code update (`src/tof_demo.c`) + `build` + `flash` | PASS | Added AI ON confidence-weighted estimator fusion for TP distance/fullness to improve runtime behavior while preserving AI OFF baseline path.
- `2026-02-13` | code/docs update (`src/tof_demo.c`, `README.md`) + `build` + `flash` | PASS | Added AI confidence percent to terminal last line and expanded README AI/training-data workflow documentation.
- `2026-02-13` | debug-panel line layout update (`src/tof_demo.c`) + `build` + `flash` | PASS | Moved confidence to final dedicated terminal line (`CONF:%`) and restored AI line to `AI:x A:mm`.
- `2026-02-13` | `cp mcuxsdk_ws/build/tof_demo_cm33_core0.elf failsafe/FAILSAFE_2026-02-13_v9_ai_confline_runtime_assist.elf` | PASS | Published v9 failsafe artifact.
- `2026-02-13` | docs baseline sync (`README`, `STATUS`, `START_HERE`, `PROJECT_STATE`, `RESTORE_POINTS`, `TOF_DEBUG_STATUS`, `BUILD_FLASH`, `OPS_RUNBOOK`, `failsafe.md`) | PASS | Prepared v9 golden/failsafe metadata and restore instructions.
- `2026-02-13` | `git commit` | PASS | Published v9 release payload commit `e7e2432`.
- `2026-02-13` | `git tag` | PASS | Created `GOLDEN_2026-02-13_v9_ai_confline_runtime_assist` and `GOLDEN_LOCK_2026-02-13_v9_e7e2432`.
- `2026-02-13` | docs tag-pin update (`README`, `STATUS`, `START_HERE`, `PROJECT_STATE`, `RESTORE_POINTS`, `TOF_DEBUG_STATUS`) | PASS | Replaced v9 `<commit>` placeholders with exact lock/commit IDs.
- `2026-02-13` | `git push origin main` + `git push origin GOLDEN_2026-02-13_v9_ai_confline_runtime_assist GOLDEN_LOCK_2026-02-13_v9_e7e2432` | PASS | Published v9 release commits and restore tags to GitHub.
