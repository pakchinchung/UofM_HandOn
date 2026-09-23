# Dino Game - Pin Assignments (PIC32CM PL10 Curiosity Nano)

Board: PIC32CM PL10 Curiosity Nano (EV10P22A)
MCU: PIC32CM6408PL10048
Ported from: TestAi2 (AVR128DA48)

## Summary

| Function        | MCU Pin | Peripheral       | Mux | AVR original      |
|-----------------|---------|------------------|-----|-------------------|
| I2C SDA         | PA00    | SERCOM0 PAD0     | C   | PC2 (TWI0 SDA)    |
| I2C SCL         | PA01    | SERCOM0 PAD1     | C   | PC3 (TWI0 SCL)    |
| Potentiometer   | PA29    | ADC0 AIN29       | B   | PD7 (ADC0 AIN7)   |
| UART TX (VCOM)  | PB00    | SERCOM1 PAD0     | D   | USART1            |
| UART RX (VCOM)  | PB01    | SERCOM1 PAD1     | D   | USART1            |
| Heartbeat LED   | PB02    | GPIO, active low | -   | PC6               |

## I2C bus (SERCOM0, I2C master)

- External pull-ups on SDA/SCL (internal pull-ups not used).
- Devices:
  - SSD1306 128x64 OLED
  - MCP23008 I/O expander at 0x24
    - GP5 = JUMP, GP6 = START, GP7 = RESET/menu
    - Buttons active low, external pull-ups, GPPU off

## Clocks

| Peripheral | APB mask               | GCLK channel (instance header)   |
|------------|------------------------|----------------------------------|
| SERCOM0    | `MCLK_APBCMASK_SERCOM0` | `SERCOM0_GCLK_ID_CORE` = 6       |
| SERCOM1    | `MCLK_APBCMASK_SERCOM1` | `SERCOM1_GCLK_ID_CORE` = 8       |
| TC0        | `MCLK_APBCMASK_TC0`     | `TC0_GCLK_ID` = 9                |
| ADC0       | `MCLK_APBCMASK_ADC0`    | none - clocked from bus clock, `ADC_CTRLB_PRESCALER` |

Note: the PCHCTRL table in `.claude/skills/peripheral-init/SKILL.md` does
not match the DFP instance headers. The instance header values are correct.

## Source

`My_MCC_Config/src/packs/PIC32CM6408PL10048_DFP/pio/pic32cm6408pl10048.h`:
`MUX_PA00C_SERCOM0_PAD0 = 2`, `MUX_PA01C_SERCOM0_PAD1 = 2`, `MUX_PA29B_ADC0_AIN29 = 1`.
