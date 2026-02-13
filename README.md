# ToF Demo (FRDM-MCXN947) - 3D Printer Spool Monitor

This firmware drives a real-time 3D printer spool state UI from a TMF8828 8x8 ToF sensor on NXP FRDM-MCXN947.

Hardware target:
- FRDM-MCXN947
- LCD-PAR-S035 display (J8)
- TMF8828_EVM_EB_SHIELD (I2C)

Core runtime outputs:
- 8x8 ToF heatmap + debug panel (left column)
- 3D printer spool visualization + 8-step bargraph (right side)
- four-state classification: `FULL`, `MEDIUM`, `LOW`, `EMPTY`
- warning popups (forced top layer)

## Measurement Pipeline

Main path in `src/tof_demo.c` (`tof_update_spool_model`):
- derives `closest_mm`, `curve_mm`, `avg_mm`, and `valid_count` from live 8x8 data;
- applies deterministic sparse/full-empty and hysteresis rules;
- computes fullness (`0..1024`) and maps to an 8-segment bargraph;
- maps to four user-visible states (`FULL`, `MEDIUM`, `LOW`, `EMPTY`).

## AI Behavior

AI in this firmware is a runtime assist and data-analysis layer.

When AI is ON:
- confidence-weighted fusion is applied to model distance/fullness for improved stability;
- denoise/hold logic helps clean the live 8x8 grid;
- debug terminal tail shows:
  - `AI:<0|1> A:<mm>`
  - `CONF:<confidence%>`

When AI is OFF:
- deterministic baseline path runs without AI fusion.

## Roll-Size Data Workflow

Real hardware captures were used across spool conditions (full, medium, low, near-empty/no-spool) to tune thresholds and fusion behavior:
- `AI_CSV` emits compact feature rows.
- `AI_F64` emits full 64-cell frame rows.
- captures were used to tune hysteresis, sparse overrides, and confidence fusion weights.

No on-device ML training is executed in production firmware.

## Build and Flash

```bash
./tools/bootstrap_ubuntu_user.sh
./tools/setup_mcuxsdk_ws.sh
./tools/build_frdmmcxn947.sh debug
./tools/flash_frdmmcxn947.sh
```

## Debug UART

```bash
ls -l /dev/serial/by-id
screen /dev/ttyACM0 115200
```

Useful markers:
- `TOF demo: live 8x8 frames detected`
- `TOF AI: ON/OFF`
- `AI_CSV,...`
- `AI_F64,...`
