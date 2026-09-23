#include "timer.h"
#include "device.h"

// 24 MHz / 64 = 375000 Hz. CC0 = 374 → period = 1.00 ms.
#define TC0_COMPARE_VALUE 374U

static Timer_Callback tick_callback;

void Timer_Init(void)
{
    tick_callback = (Timer_Callback)0;

    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TC0_Msk;

    GCLK_REGS->GCLK_PCHCTRL[9] = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[9] & GCLK_PCHCTRL_CHEN_Msk) == 0U) { }

    TC0_REGS->COUNT16.TC_CTRLA = TC_CTRLA_SWRST_Msk;
    while ((TC0_REGS->COUNT16.TC_SYNCBUSY & TC_SYNCBUSY_SWRST_Msk) != 0U) { }

    TC0_REGS->COUNT16.TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV64;

    TC0_REGS->COUNT16.TC_WAVE = TC_WAVE_WAVEGEN_MFRQ;

    TC0_REGS->COUNT16.TC_CC[0] = TC0_COMPARE_VALUE;
    while ((TC0_REGS->COUNT16.TC_SYNCBUSY & TC_SYNCBUSY_CC0_Msk) != 0U) { }

    TC0_REGS->COUNT16.TC_INTENSET = TC_INTENSET_MC0_Msk;

    NVIC_EnableIRQ(TC0_IRQn);

    TC0_REGS->COUNT16.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    while ((TC0_REGS->COUNT16.TC_SYNCBUSY & TC_SYNCBUSY_ENABLE_Msk) != 0U) { }
}

void Timer_RegisterCallback(Timer_Callback cb)
{
    tick_callback = cb;
}

void TC0_Handler(void)
{
    TC0_REGS->COUNT16.TC_INTFLAG = TC_INTFLAG_MC0_Msk;
    if (tick_callback != (Timer_Callback)0) {
        tick_callback();
    }
}
