#include <stdarg.h>
#include <stdio.h>
#include "uart.h"
#include "device.h"

// SERCOM1 USART on PIC32CM PL10 Curiosity Nano Virtual COM Port
// TX: PB00 (SERCOM1 PAD0, Peripheral Function D)
// RX: PB01 (SERCOM1 PAD1, Peripheral Function D)
// Transmit only: the game shell takes no console input.

#define UART_PERIPH_FUNC_D 0x03U
#define UART_REF_CLOCK_HZ  24000000U
#define UART_PRINTF_BUF    128U

void UART_Init(uint32_t baud_rate)
{
    // 1. Bus clock
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM1_Msk;

    // 2. GCLK0 (24 MHz) -> SERCOM1_CORE
    GCLK_REGS->GCLK_PCHCTRL[SERCOM1_GCLK_ID_CORE] = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[SERCOM1_GCLK_ID_CORE] & GCLK_PCHCTRL_CHEN_Msk) == 0U) { }

    // 3. Pin mux: PB00 even -> low nibble, PB01 odd -> high nibble of PMUX[0]
    PORT_REGS->GROUP[1].PORT_PMUX[0] = (UART_PERIPH_FUNC_D << 4) | UART_PERIPH_FUNC_D;
    PORT_REGS->GROUP[1].PORT_PINCFG[0] |= PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[1].PORT_PINCFG[1] |= PORT_PINCFG_PMUXEN_Msk;

    // 4. Reset
    SERCOM1_REGS->USART.SERCOM_CTRLA = SERCOM_USART_CTRLA_SWRST_Msk;
    while ((SERCOM1_REGS->USART.SERCOM_SYNCBUSY & SERCOM_USART_SYNCBUSY_SWRST_Msk) != 0U) { }

    // 5. Internal clock USART, TX PAD0, RX PAD1, LSB first
    SERCOM1_REGS->USART.SERCOM_CTRLA =
        SERCOM_USART_CTRLA_MODE_USART_INT |
        SERCOM_USART_CTRLA_TXPO_MUX0 |
        SERCOM_USART_CTRLA_RXPO_MUX1 |
        SERCOM_USART_CTRLA_DORD_Msk;

    // 6. 8-bit, TX only
    SERCOM1_REGS->USART.SERCOM_CTRLB =
        SERCOM_USART_CTRLB_CHSIZE_8_BIT |
        SERCOM_USART_CTRLB_TXEN_Msk;
    while ((SERCOM1_REGS->USART.SERCOM_SYNCBUSY & SERCOM_USART_SYNCBUSY_CTRLB_Msk) != 0U) { }

    // 7. Async arithmetic mode: BAUD = 65536 * (1 - 16 * f_baud / f_ref)
    uint64_t br = (uint64_t)65536U * (UART_REF_CLOCK_HZ - 16U * baud_rate) / UART_REF_CLOCK_HZ;
    SERCOM1_REGS->USART.SERCOM_BAUD = (uint16_t)br;

    // 8. Enable
    SERCOM1_REGS->USART.SERCOM_CTRLA |= SERCOM_USART_CTRLA_ENABLE_Msk;
    while ((SERCOM1_REGS->USART.SERCOM_SYNCBUSY & SERCOM_USART_SYNCBUSY_ENABLE_Msk) != 0U) { }
}

void UART_WriteByte(uint8_t data)
{
    while ((SERCOM1_REGS->USART.SERCOM_INTFLAG & SERCOM_USART_INTFLAG_DRE_Msk) == 0U) { }
    SERCOM1_REGS->USART.SERCOM_DATA = data;
}

void UART_WriteString(const char *str)
{
    while (*str != '\0') {
        UART_WriteByte((uint8_t)*str++);
    }
}

void UART_Printf(const char *fmt, ...)
{
    char buf[UART_PRINTF_BUF];
    va_list args;

    va_start(args, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    UART_WriteString(buf);
}
