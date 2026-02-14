# ToF Debug Status (TMF8828 + FRDM-MCXN947 + LCD-PAR-S035)

Date: 2026-02-13
Project: EdgeAI_3D_Printer_Spool_Size_ToF_demo_NXP_FRDM-MCXN947

## Current Runtime Status

- Live TMF8828 8x8 streaming is stable on FRDM-MCXN947.
- 3D printer spool state output remains four-state (`FULL`, `MEDIUM`, `LOW`, `EMPTY`) with 8-segment bargraph rendering.
- AI ON now applies confidence-weighted runtime assist fusion to improve stability and transition quality.
- Debug terminal tail is:
  - `AI:x A:mm`
  - `CONF:%`

## Notes

- This repository variant does not maintain restore-point tags or pinned failsafe artifacts.
