#include "uart.h"
#include "cmd.h"
#include "device.h"

// SERCOM1 USART on PIC32CM PL10 Curiosity Nano Virtual COM Port
// TX: PB00 (SERCOM1 PAD0, Peripheral Function D)
// RX: PB01 (SERCOM1 PAD1, Peripheral Function D)

#define UART_PERIPH_FUNC_D 0x03U

void UART_Init(uint32_t baud_rate)
{
    // 1. Enable bus clock for SERCOM1 (APBC, bit 2)
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM1_Msk;

    // 2. Route GCLK0 (24 MHz) to SERCOM1_CORE (peripheral channel 8)
    GCLK_REGS->GCLK_PCHCTRL[8] = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[8] & GCLK_PCHCTRL_CHEN_Msk) == 0U) { }

    // 3. Configure pins PB00 (TX) and PB01 (RX) to peripheral function D
    // PB00 = pin 0 in group B (even pin -> lower nibble of PMUX[0])
    // PB01 = pin 1 in group B (odd pin -> upper nibble of PMUX[0])
    PORT_REGS->GROUP[1].PORT_PMUX[0] = (UART_PERIPH_FUNC_D << 4) | UART_PERIPH_FUNC_D;
    PORT_REGS->GROUP[1].PORT_PINCFG[0] |= PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[1].PORT_PINCFG[1] |= PORT_PINCFG_PMUXEN_Msk;

    // 4. Software reset SERCOM1
    SERCOM1_REGS->USART.SERCOM_CTRLA = SERCOM_USART_CTRLA_SWRST_Msk;
    while ((SERCOM1_REGS->USART.SERCOM_SYNCBUSY & SERCOM_USART_SYNCBUSY_SWRST_Msk) != 0U) { }

    // 5. Configure CTRLA: internal clock USART, TX on PAD0, RX on PAD1, LSB first
    SERCOM1_REGS->USART.SERCOM_CTRLA =
        SERCOM_USART_CTRLA_MODE_USART_INT |
        SERCOM_USART_CTRLA_TXPO_MUX0 |
        SERCOM_USART_CTRLA_RXPO_MUX1 |
        SERCOM_USART_CTRLA_DORD_Msk;

    // 6. Configure CTRLB: 8-bit, TX enable, RX enable
    SERCOM1_REGS->USART.SERCOM_CTRLB =
        SERCOM_USART_CTRLB_CHSIZE_8_BIT |
        SERCOM_USART_CTRLB_TXEN_Msk |
        SERCOM_USART_CTRLB_RXEN_Msk;
    while ((SERCOM1_REGS->USART.SERCOM_SYNCBUSY & SERCOM_USART_SYNCBUSY_CTRLB_Msk) != 0U) { }

    // 7. Set baud rate (async arithmetic mode: BAUD = 65536 * (1 - 16 * f_baud / f_ref))
    uint64_t br = (uint64_t)65536U * (24000000U - 16U * baud_rate) / 24000000U;
    SERCOM1_REGS->USART.SERCOM_BAUD = (uint16_t)br;

    // 8. Enable RXC interrupt
    SERCOM1_REGS->USART.SERCOM_INTENSET = SERCOM_USART_INTENSET_RXC_Msk;
    NVIC_EnableIRQ(SERCOM1_IRQn);

    // 9. Enable SERCOM1
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

uint8_t UART_ReadByte(void)
{
    while ((SERCOM1_REGS->USART.SERCOM_INTFLAG & SERCOM_USART_INTFLAG_RXC_Msk) == 0U) { }
    return (uint8_t)(SERCOM1_REGS->USART.SERCOM_DATA);
}

void SERCOM1_Handler(void)
{
    if ((SERCOM1_REGS->USART.SERCOM_INTFLAG & SERCOM_USART_INTFLAG_RXC_Msk) != 0U) {
        uint8_t data = (uint8_t)(SERCOM1_REGS->USART.SERCOM_DATA);
        CMD_ReceiveByte((char)data);
    }
}
