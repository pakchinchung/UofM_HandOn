# Morse

Morse code transmitter and decoder for the PIC32CM PL10 Curiosity Nano
(PIC32CM6408PL10048).

- **Transmit:** text typed on the Virtual COM port blinks out as Morse on the
  user LED.
- **Receive:** after `/stop`, dots and dashes tapped on a key wired to PA26 are
  decoded back to text on the terminal.

## Hardware

| Function       | MCU pin | Peripheral / notes                         |
|----------------|---------|--------------------------------------------|
| LED0           | PB02    | GPIO, active low, Morse output             |
| SW0            | PB03    | GPIO, active low, toggles replay loop      |
| Morse key      | PA26    | GPIO, active low, internal pull-up         |
| UART TX (VCOM) | PB00    | SERCOM1 PAD0                               |
| UART RX (VCOM) | PB01    | SERCOM1 PAD1                               |

Wire the Morse key between PA26 and GND. No external pull-up is needed.

Board pin details: [docs/pic32cm_pl10_curiosity_nano_pins.md](docs/pic32cm_pl10_curiosity_nano_pins.md).

## Build and run

1. In VS Code, open this `morse` folder (not the repo root).
2. Build and program with the MPLAB extension. The image is written to
   `out/TestPl10Again/default.hex`.
3. Open a terminal on the Curiosity Nano COM port at 115200 8N1.

## Usage

Type plain text and it is queued and sent on the LED. Lines starting with `/`
are commands:

| Command           | Action                                          |
|-------------------|-------------------------------------------------|
| `/help`           | List commands                                   |
| `/speed <ms>`     | Set LED dot unit, 10-1000 ms (default 100)      |
| `/speed`          | Show LED dot unit                               |
| `/loop`           | Toggle replay loop of the last message          |
| `/replay`         | Replay the last message once                    |
| `/stop`           | Stop playback and enable PA26 key decoding      |
| `/decode <morse>` | Decode a pattern, e.g. `/decode ... --- ...`    |
| `/key <ms>`       | Set key dot unit, 20-2000 ms (default 200)      |
| `/key`            | Show key unit and state                         |
| `/keyreset`       | Clear pending key input                         |
| `/keydebug`       | Toggle raw tap timing output                    |
| `/keypin`         | Show live PA26 level                            |

In `/decode`, use `.` and `-` for elements, a space between letters and `/`
between words.

Keying on PA26, relative to the key unit:

- Hold at least 2 units for a dash, shorter for a dot.
- Release for 5 units to end a letter, 10 units for a word space.

These gaps are wider than textbook Morse timing so hand keying on a push
button still decodes reliably.

Recognised prosigns: SOS, AR, SK, KA, VE, HH.

## Code layout

All application code is in `app/`, one `.c`/`.h` pair per module. It uses
registers directly and does not depend on the MCC peripheral libraries.

| Module   | Purpose                                                  |
|----------|----------------------------------------------------------|
| `morse`  | Text to Morse encoder, LED playback, pattern decoder     |
| `key`    | PA26 key sampling, tap timing and live decoding          |
| `cmd`    | UART command parser                                      |
| `uart`   | SERCOM1 VCOM driver, RX interrupt feeds `cmd`            |
| `timer`  | TC0 1 ms tick                                            |
| `button` | SW0 debounce                                             |
| `led`    | LED0 control                                             |

`My_MCC_Config/` is MCC generated. Only `My_MCC_Config/src/main.c` is edited
by hand.

## Project files

| Path                              | Purpose                                   |
|-----------------------------------|-------------------------------------------|
| `.vscode/TestPl10Again.mplab.json` | MPLAB project file, do not delete        |
| `cmake`                           | Generated CMake files                     |
| `_build`                          | CMake build tree, can be deleted          |
| `out`                             | Final build artifacts                     |
