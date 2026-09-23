# PIC32CM PL10 Curiosity Nano - LED and Virtual COM Port Pin Details

Board: PIC32CM PL10 Curiosity Nano (EV10P22A)  
MCU: PIC32CM6408PL10048

## LED (LED0)

| MCU Pin | Description | Default Connection |
|---------|-------------|--------------------|
| PB02 | User LED (yellow), active low | LED0, Edge connector |

- One yellow user LED is available on the board
- Can be controlled by GPIO or PWM
- Driving the I/O line to GND activates the LED

## Virtual COM Port (USB CDC)

The on-board Nano Debugger provides a Virtual Serial Port (CDC) interface for accessing the target MCU's UART.

| Debugger Pin | MCU Pin | MCU Function | Description |
|--------------|---------|--------------|-------------|
| CDC TX | PB01 | SERCOM1 PAD1 (RX) | USB CDC TX line (debugger transmits, MCU receives) |
| CDC RX | PB00 | SERCOM1 PAD0 (TX) | USB CDC RX line (debugger receives, MCU transmits) |

### SERCOM Configuration

- SERCOM instance: **SERCOM1**
- TX pin (MCU to PC): **PB00** (SERCOM1 PAD0)
- RX pin (PC to MCU): **PB01** (SERCOM1 PAD1)

### Notes

- The Virtual COM Port appears as a CDC device when the board is connected via USB
- Cut straps J105/J106 on the bottom of the board can disconnect the CDC lines from the debugger
- Cutting GPIO straps to the on-board debugger disables the virtual serial port, programming, debugging, and data streaming functions

## Source

PIC32CM PL10 Curiosity Nano User Guide:  
https://onlinedocs.microchip.com/oxy/GUID-DDFE99F7-6849-4A95-9D6E-A6253B0EB088-en-US-1/GUID-23204CC6-89DA-4D44-A6A1-BEEBC6E086C4.html
