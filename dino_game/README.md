# DinoGame

Dino runner and Breakout on an SSD1306 OLED, ported from the AVR128DA48
TestAi2 project to the PIC32CM PL10 Curiosity Nano (PIC32CM6408PL10048).

A home menu picks between the Dino game, Breakout and a diagnostic screen.
Buttons come from an MCP23008 I/O expander, a potentiometer sets game speed
or moves the Breakout paddle, and a boot log plus game events go out on the
Virtual COM port.

## Hardware

| Function       | MCU pin | Peripheral         |
|----------------|---------|--------------------|
| I2C SDA        | PA00    | SERCOM0 PAD0       |
| I2C SCL        | PA01    | SERCOM0 PAD1       |
| Potentiometer  | PA29    | ADC0 AIN29         |
| UART TX (VCOM) | PB00    | SERCOM1 PAD0       |
| Heartbeat LED  | PB02    | GPIO, active low   |

I2C devices (external pull-ups on SDA/SCL):

- SSD1306 128x64 OLED
- MCP23008 at 0x24: GP5 = JUMP, GP6 = START, GP7 = back to menu. Buttons
  are active low with external pull-ups.

Full pin and clock details: [docs/dino_game_pins.md](docs/dino_game_pins.md).

## Build and run

1. In VS Code, open this `dino_game` folder (not the repo root).
2. Build and program with the MPLAB extension. The image is written to
   `out/DinoGame/default.hex`.
3. Open a terminal on the Curiosity Nano COM port at 115200 8N1.

At boot the log shows an I2C scan, device status and a bus speed sweep, then
the menu appears on the OLED. The LED blinks every 500 ms while the main loop
is running.

## Code layout

All application code is in `app/`, one `.c`/`.h` pair per module. It uses
registers directly and does not depend on the MCC peripheral libraries.

| Module       | Purpose                                                      |
|--------------|--------------------------------------------------------------|
| `i2c_bus`    | Polled SERCOM0 I2C host with a per-transfer bus speed        |
| `pot`        | ADC0 potentiometer read, interrupt driven, filtered          |
| `millis`     | SysTick 1 ms timebase: `millis()`, `micros()`, `delay()`     |
| `uart`       | SERCOM1 VCOM transmit and `UART_Printf()`                    |
| `led`        | Heartbeat LED                                                |
| `ssd1306`    | OLED driver with dirty-page frame buffer                     |
| `mcp23008`   | Debounced button input over I2C                              |
| `menu`       | Home menu                                                    |
| `dino_game`  | Dino runner                                                  |
| `breakout`   | Breakout                                                     |
| `bringup`    | Diagnostic screen                                            |
| `gfx_assets` | Font and sprites                                             |

`My_MCC_Config/` is MCC generated. Only `My_MCC_Config/src/main.c` is edited
by hand.

## Project files

| Path                         | Purpose                                              |
|------------------------------|------------------------------------------------------|
| `.vscode/DinoGame.mplab.json` | MPLAB project file, do not delete                   |
| `cmake`                      | Generated CMake files                                |
| `_build`                     | CMake build tree, can be deleted                     |
| `out`                        | Final build artifacts                                |
