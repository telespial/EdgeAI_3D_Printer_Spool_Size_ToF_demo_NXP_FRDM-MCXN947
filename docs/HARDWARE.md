# Hardware

## Target Platform
- Board: FRDM-MCXN947
- Display: LCD-PAR-S035 on J8 (FlexIO 8080)
- ToF module: TMF8828_EVM_EB_SHIELD on host I2C path

## Built-In Debug Port
- Connector: FRDM-MCXN947 debug USB (MCU-Link, J17)
- Features: SWD flash/debug + virtual COM port (UART logs)
- Default console rate: `115200`

Use this port to confirm whether live ToF data is actually present before
debugging display-side behavior.

## Key Board Wiring
- Debug UART: FLEXCOMM4
- Project I2C path: FLEXCOMM3 (`P1_0` SDA, `P1_1` SCL)
- LCD control: `P0_7` (RS/DC), `P0_12` (CS), `P4_7` (RST)
- LCD FlexIO control: `P0_8` (RD), `P0_9` (WR)
- LCD data bus: FlexIO D16..D31 (`P2_8..P2_11`, `P4_12..P4_23`)

## MRD Sources
- FRDM-MCXN947 platform MRD pack
- LCD-PAR-S035 module MRD pack
- TMF8828_EVM_EB_SHIELD platform MRD pack
