# PREPAY_V0 RS-485 firmware

This is the Keil C51/uVision project for the schematic's MS51PC0AE controller.
The Nuvoton MS51 BSP/device headers and the Keil startup file are included under
`vendor\MS51_BSP`; the project does not need a separate driver download.

## Hardcoded communication settings

- RS-485 port 1 is the Modbus RTU master connected to the RISH 3428.
- Meter slave ID: `5`.
- Serial format: `9600 baud, 8 data bits, even parity, 1 stop bit (8E1)`.
- Function: holding-register read (`03`).
- Read starts at protocol offset `0` (holding register `40001`) and reads ten
  consecutive registers through `40010`.
- RS-485 port 2 is a Modbus RTU slave with controller ID `1`, also at `9600 8E1`.
- Both RS-485 ports generate and check the even-parity ninth UART bit.

The meter register values are returned as raw 16-bit Modbus values; no scaling
or engineering-unit conversion is applied.

## Port 2 register map

The map uses zero-based Modbus protocol offsets. In PC tools that label holding
registers starting at `40001`, add `40001` to the offset:

| Protocol offset | PC-tool label | Contents | Supported operation |
| --- | --- | --- | --- |
| 0 | 40001 | Relay 1 state (`0` off, `1` on) | Read/write |
| 1 | 40002 | Relay 2 state (`0` off, `1` on) | Read/write |
| 2 | 40003 | Relay 3 state (`0` off, `1` on) | Read/write |
| 3-12 | 40004-40013 | Meter registers 40001-40010, in order | Read only |

Use function `03` to read holding registers and function `06` or `10` to write
relay states. The firmware pulses the matching set/reset coil only when a
relay's requested state changes.

## Wiring

1. Power the controller from its specified supply and program the MS51PC0AE
   using the schematic's programming header and a compatible Nu-Link.
2. Connect the meter's two-wire RS-485 terminals to the controller connector
   whose schematic nets are `RS485_A_1` and `RS485_B_1`. Connect A to A and B
   to B; connect signal grounds if the meter provides one. Do not connect the
   meter's supply voltage to A or B.
3. Set the RISH 3428 to slave ID `5`, `9600` baud, `8E1`. Ensure no other
   device on this bus uses slave ID `5`.
4. Connect a USB-RS-485 adapter or other Modbus RTU master to the connector
   labelled `RS485_A_2` / `RS485_B_2`. Join signal ground when available.
   Configure the test master for slave ID `1`, `9600`, `8E1`; do not connect a
   second master to this port.
5. Keep a two-wire RS-485 bus daisy-chained. Terminate only the two physical
   ends of each bus (nominally 120 ohms); the schematic already shows onboard
   120-ohm resistors, so check for duplicate termination before adding one.
   If A/B naming differs between manufacturers, verify the terminal labels or
   swap the pair with power off if communication does not start.

## Build and test in Keil

1. Open `Prepaid_Interface_Test.uvproj` in Keil uVision with the C51 toolchain
   and Nuvoton 8051 device support installed.
2. Build target `PREPAY_V0_Interface_Test`. The HEX and BIN are written under
   `Output`.
3. Program the generated HEX with Nu-Link. On startup the firmware resets all
   three relays to off and begins polling the meter.
4. With the meter connected, confirm the LCD reports register `40001` as valid
   and the meter data appears in the port 2 register map. The two controller
   status LEDs turn on only while meter data is valid.
5. From the Modbus master on port 2, read offsets `0-12`. Write value `1` and
   then `0` to each of offsets `0`, `1`, and `2`, one at a time, to test each
   relay. Do not write to offsets `3-12`.
6. Disconnect the meter (or turn it off). After the next poll times out, meter
   data is cleared, port 2 returns zero for meter offsets `3-12`, and both
   MCU-controlled status LEDs turn off. The schematic's RS-485 TX/RX activity
   LEDs can still blink because the master keeps polling to detect reconnection.
   Reconnect the meter and confirm polling resumes.

If the meter stays offline, first check A/B polarity, common reference, bus
termination, meter ID `5`, `9600 8E1`, and that the selected connector is the
port 1 (`_1`) bus.
