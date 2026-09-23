# UofM Hands-On: PIC32CM PL10 Curiosity Nano

Hands-on projects for the PIC32CM PL10 Curiosity Nano (EV10P22A,
PIC32CM6408PL10048, Arm Cortex-M0+ at 24 MHz).

Each folder is a standalone MPLAB project for VS Code. Open the project folder
itself, not this repo root.

| Project                     | Description                                                            |
|-----------------------------|------------------------------------------------------------------------|
| [morse](morse/)             | Morse code on the LED from UART text, and decoding of a key on PA26    |
| [dino_game](dino_game/)     | Dino runner and Breakout on an SSD1306 OLED, with MCP23008 buttons and a potentiometer. Ported from AVR128DA48 |

## Requirements

- VS Code with the MPLAB extension pack
- XC32 v5.10
- PIC32CM-PL_DFP and CMSIS packs (installed by the MPLAB extension)

## Conventions

- Application code lives in each project's `app/` folder, one `.c`/`.h`
  pair per module.
- Peripherals are set up at register level. The MCC peripheral libraries are
  not used.
- `My_MCC_Config/` is MCC generated. Only `My_MCC_Config/src/main.c` is
  edited by hand.
- Pin assignments for each project are in its `docs/` folder.
