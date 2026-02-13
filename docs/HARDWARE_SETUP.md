# Hardware Setup

## Required Hardware
- FRDM-MCXN947 board
- LCD-PAR-S035 display module (J8)
- TMF8828_EVM_EB_SHIELD (I2C)
- Debug USB cable (MCU-Link / J17)

## Bring-Up Checklist
1. Attach LCD-PAR-S035 to J8.
2. Connect TMF8828 shield on the configured I2C host path.
3. Connect debug USB to host PC.
4. Confirm probe visibility:
```bash
LinkServer probe '#1' dapinfo
```
5. Confirm serial port visibility:
```bash
ls -l /dev/serial/by-id
```

## Console
- Open UART log stream:
```bash
screen /dev/ttyACM0 115200
```

## Live-Data Markers
- `TOF demo: live 8x8 frames detected`
- Terminal `LIVE:1` in left-column debug panel

## Wiring Summary
- Debug UART: FLEXCOMM4
- ToF I2C path: FLEXCOMM3 (`P1_0` SDA, `P1_1` SCL)
- LCD control pins: `P0_7` (RS/DC), `P0_12` (CS), `P4_7` (RST)
