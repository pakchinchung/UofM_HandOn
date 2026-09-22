# Peripheral Initialization Skill — PIC32CM PL10

This skill defines the standard process for initializing any peripheral at register level on the PIC32CM6408PL10048 (Cortex-M0+, PIC32CM PL10 Curiosity Nano board).

## Project Layout

```
My_MCC_Config/src/
├── packs/PIC32CM6408PL10048_DFP/
│   ├── pic32cm6408pl10048.h          # Main device header (IRQn, base addresses)
│   ├── component/                     # Register struct + bitfield macros per peripheral
│   │   ├── sercom.h, tc.h, tcc0.h, adc.h, port.h, gclk.h, mclk.h, ...
│   ├── instance/                      # Instance-specific base address defines
│   │   ├── sercom0.h, sercom1.h, tc0.h, ...
│   └── pio/pic32cm6408pl10048.h       # Pin mux definitions (PIN_, MUX_, PINMUX_ macros)
├── packs/CMSIS/CMSIS/Core/Include/    # ARM core headers (core_cm0plus.h, etc.)
├── config/default/
│   ├── definitions.h                  # System-wide includes, CPU_CLOCK_FREQUENCY
│   ├── device.h                       # Device header wrapper
│   ├── initialization.c              # SYS_Initialize() — calls CLOCK, EVSYS, NVIC init
│   └── peripheral/
│       ├── clock/plib_clock.c         # OSCCTRL, OSC32KCTRL, GCLK setup
│       ├── port/plib_port.c/.h        # GPIO and pin mux PLIB
│       ├── nvic/plib_nvic.c/.h        # NVIC setup
│       └── evsys/plib_evsys.c/.h      # Event system
└── main.c                             # Entry point
```

## Step-by-Step Peripheral Initialization Process

### Step 1: Determine Pin Assignments

1. **Check `docs/` folder first** for any `.md` files that already have the pin details for the peripheral/board feature you need.
2. **If not found in docs/**, use the `mplab-docs` MCP server:
   - `search_evk_user_guide` with `evk: "PIC32CM PL10"` to get board-level pin connections.
   - `search_datasheet` with `device: "PIC32CM6408PL10048"` for peripheral-to-pin mapping.
3. **If MCP fails**, look up the PIO header directly:
   - File: `My_MCC_Config/src/packs/PIC32CM6408PL10048_DFP/pio/pic32cm6408pl10048.h`
   - Search for the peripheral name (e.g., `SERCOM1`, `TC0`, `ADC0`) to find all valid pin/mux combinations.
   - Pin mux naming convention: `PIN_<port><pin><mux-letter>_<PERIPHERAL>_<FUNCTION>`
   - Mux letters map to `PERIPHERAL_FUNCTION_A` through `PERIPHERAL_FUNCTION_J` (A=0, B=1, C=2, D=3, ...).
4. **Save the result** to `docs/` as a `.md` file for future reference.

### Step 2: Enable the Bus Clock (MCLK)

Every peripheral sits on an APB bus. Enable its bus clock via the MCLK APBxMASK register before accessing any peripheral registers.

```c
// Peripheral bus mapping (from component/mclk.h):
//   APBA (offset 0x14): PAC, PM, MCLK, RSTC, OSCCTRL, OSC32KCTRL, SUPC, GCLK, WDT, RTC, EIC
//   APBB (offset 0x18): PORT, DSU, NVMCTRL, DMAC, MTB, HMATRIXHS
//   APBC (offset 0x1C): EVSYS, SERCOM0, SERCOM1, TC0, TC1, TC2, TCC0, ADC0, AC, CCL, PTC, SYSCTRL

// Example: Enable SERCOM1 bus clock
MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM1_Msk;
```

### Step 3: Route a Generic Clock (GCLK) to the Peripheral

Most peripherals need a generic clock source routed via GCLK PCHCTRL. The peripheral channel index comes from the datasheet or the instance header.

```c
// GCLK Peripheral Channel indices (from datasheet / instance headers):
//   PCHCTRL[3]  = SERCOM0_CORE
//   PCHCTRL[4]  = SERCOM1_CORE
//   PCHCTRL[5]  = TC0, TC1
//   PCHCTRL[6]  = TC2
//   PCHCTRL[7]  = TCC0
//   PCHCTRL[8]  = ADC0
//   PCHCTRL[9]  = AC
//   PCHCTRL[10] = CCL
//   PCHCTRL[11] = EIC
//   PCHCTRL[12] = EVSYS[0]

// Example: Route GCLK0 (24 MHz OSCHF) to SERCOM1
GCLK_REGS->GCLK_PCHCTRL[4] = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
while ((GCLK_REGS->GCLK_PCHCTRL[4] & GCLK_PCHCTRL_CHEN_Msk) == 0U) {
    /* Wait for channel enable */
}
```

**Clock sources available on GCLK generators:**
- GCLK0: OSCHF at 24 MHz (default system clock, already configured by `CLOCK_Initialize()`)
- GCLK1-3: Available for custom configurations (e.g., OSC32K at 32.768 kHz)

### Step 4: Configure Pin Mux

Use the PORT PLIB or direct register access to assign the peripheral function to the correct pins.

```c
// Using PLIB (recommended for clarity):
PORT_PinPeripheralFunctionConfig(PORT_PIN_PB00, PERIPHERAL_FUNCTION_D);  // PB00 -> SERCOM1 PAD0
PORT_PinPeripheralFunctionConfig(PORT_PIN_PB01, PERIPHERAL_FUNCTION_D);  // PB01 -> SERCOM1 PAD1

// Or direct register access:
// For pin PB00 (pin_num=0 within group B, even pin -> lower nibble of PMUX[0])
PORT_REGS->GROUP[1].PORT_PMUX[0] = (PORT_REGS->GROUP[1].PORT_PMUX[0] & ~0x0FU) | 0x03U; // MUX D = 3
PORT_REGS->GROUP[1].PORT_PINCFG[0] |= PORT_PINCFG_PMUXEN_Msk;
```

**Pin mux lookup:**
- Find the correct PERIPHERAL_FUNCTION letter from the PIO header: `MUX_PB00D_SERCOM1_PAD0 = 3` means function D.
- Functions: A=0, B=1, C=2, D=3, E=4, F=5, G=6, H=7

### Step 5: Configure Peripheral Registers

With clocks enabled and pins muxed, configure the peripheral's own registers. General pattern:

1. **Disable** the peripheral (if it has an ENABLE bit) before configuring.
2. **Wait for sync** if the peripheral has a SYNCBUSY register.
3. **Write configuration registers** (mode, baud, control bits, etc.).
4. **Enable** the peripheral.
5. **Wait for sync** again if applicable.

```c
// Example: SERCOM1 in USART mode
SERCOM1_REGS->USART_INT.SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_SWRST_Msk;
while (SERCOM1_REGS->USART_INT.SERCOM_SYNCBUSY) { }

SERCOM1_REGS->USART_INT.SERCOM_CTRLA =
    SERCOM_USART_INT_CTRLA_MODE(0x1) |    // USART with internal clock
    SERCOM_USART_INT_CTRLA_RXPO(1) |      // RX on PAD1
    SERCOM_USART_INT_CTRLA_TXPO(0) |      // TX on PAD0
    SERCOM_USART_INT_CTRLA_DORD_Msk;      // LSB first

SERCOM1_REGS->USART_INT.SERCOM_CTRLB =
    SERCOM_USART_INT_CTRLB_TXEN_Msk |
    SERCOM_USART_INT_CTRLB_RXEN_Msk |
    SERCOM_USART_INT_CTRLB_CHSIZE(0);     // 8-bit
while (SERCOM1_REGS->USART_INT.SERCOM_SYNCBUSY) { }

// Baud = 65536 * (1 - 16 * (f_baud / f_ref))
// For 9600 baud @ 24 MHz: BAUD ≈ 63019
SERCOM1_REGS->USART_INT.SERCOM_BAUD = 63019U;

SERCOM1_REGS->USART_INT.SERCOM_CTRLA |= SERCOM_USART_INT_CTRLA_ENABLE_Msk;
while (SERCOM1_REGS->USART_INT.SERCOM_SYNCBUSY) { }
```

### Step 6: Enable Interrupts (if needed)

```c
// Enable the peripheral's interrupt in its own INTENSET register
SERCOM1_REGS->USART_INT.SERCOM_INTENSET = SERCOM_USART_INT_INTENSET_RXC_Msk;

// Enable in NVIC (IRQ number from pic32cm6408pl10048.h)
NVIC_EnableIRQ(SERCOM1_IRQn);
```

### Step 7: Add Initialization Call to SYS_Initialize

Add the new peripheral init function call in `My_MCC_Config/src/config/default/initialization.c` inside `SYS_Initialize()`, after `CLOCK_Initialize()`.

## Register Access Conventions

- All peripheral registers use the `<PERIPHERAL>_REGS->` pointer pattern (e.g., `SERCOM1_REGS`, `PORT_REGS`, `GCLK_REGS`, `MCLK_REGS`).
- Bitfield macros follow: `<PERIPHERAL>_<REGISTER>_<FIELD>_Msk` for masks, `<PERIPHERAL>_<REGISTER>_<FIELD>_Pos` for bit position, `<PERIPHERAL>_<REGISTER>_<FIELD>(value)` for value assignment.
- Always check SYNCBUSY after writing to sync-protected registers.
- Use `_Msk` suffixed macros for single-bit fields and `(value)` macros for multi-bit fields.

## Key Reference Files

| What you need | Where to find it |
|---|---|
| Register structures and bitfields | `component/<peripheral>.h` |
| Instance base addresses and IRQ numbers | `instance/<peripheral>.h` and `pic32cm6408pl10048.h` |
| Pin mux options for a peripheral | `pio/pic32cm6408pl10048.h` |
| Bus clock mask bits | `component/mclk.h` (APBA/APBB/APBCMASK) |
| GCLK peripheral channel indices | `component/gclk.h` + datasheet |
| PORT PLIB API | `peripheral/port/plib_port.h` |
| Existing clock setup | `peripheral/clock/plib_clock.c` |
| Board pin connections (LED, VCOM, etc.) | `docs/*.md` files |

## Checklist (use for every new peripheral)

- [ ] Pin assignments identified (docs/ -> MCP -> PIO header)
- [ ] Bus clock enabled (MCLK APBxMASK)
- [ ] Generic clock routed (GCLK PCHCTRL)
- [ ] Pins muxed to peripheral function (PORT PMUX + PINCFG)
- [ ] Peripheral configured (CTRLA, CTRLB, BAUD, etc.)
- [ ] Peripheral enabled
- [ ] Interrupts configured (if needed)
- [ ] Init call added to SYS_Initialize() or called from main()
- [ ] Pin details saved to docs/ for future reference
