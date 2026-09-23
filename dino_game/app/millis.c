/*
 * @file millis.c
 *
 * @brief Arduino-style millis()/micros()/delay() timebase built on SysTick.
 */

#include <stddef.h>
#include "millis.h"
#include "device.h"

/* SysTick is clocked from the CPU clock (CLKSOURCE = 1). */
#define MILLIS_CORE_CLOCK_HZ (24000000UL)
#define MILLIS_TICKS_PER_MS  (MILLIS_CORE_CLOCK_HZ / 1000UL)
#define MILLIS_TICKS_PER_US  (MILLIS_CORE_CLOCK_HZ / 1000000UL)

static volatile uint32_t millisCounter = 0;

void SysTick_Handler(void)
{
    /* Reading CTRL clears COUNTFLAG, so micros() only ever sees it set for a
     * wrap this ISR has not yet accounted for. */
    (void)SysTick->CTRL;
    millisCounter++;
}

void MILLIS_Initialize(void)
{
    millisCounter = 0;
    (void)SysTick_Config(MILLIS_TICKS_PER_MS);
}

bool MILLIS_TickIsOneMs(void)
{
    /* One period spans LOAD + 1 CPU clocks. */
    return (((SysTick->LOAD & SysTick_LOAD_RELOAD_Msk) + 1UL) == MILLIS_TICKS_PER_MS);
}

uint32_t millis(void)
{
    /* A 32-bit aligned load is atomic on Cortex-M0+, no masking needed. */
    return millisCounter;
}

uint32_t micros(void)
{
    uint32_t primask;
    uint32_t ms;
    uint32_t val;

    primask = __get_PRIMASK();
    __disable_irq();

    val = SysTick->VAL;
    ms = millisCounter;

    /* With interrupts masked the tick ISR cannot run, so a pending COUNTFLAG
     * means the counter has already wrapped and millisCounter is one tick
     * stale. Re-read VAL so it is the post-wrap value. COUNTFLAG clears on
     * read, but the pending SysTick exception itself is unaffected. */
    if (0U != (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk))
    {
        val = SysTick->VAL;
        ms++;
    }

    __set_PRIMASK(primask);

    /* SysTick counts down from LOAD to 0. */
    return (ms * 1000UL) + ((MILLIS_TICKS_PER_MS - 1UL - val) / MILLIS_TICKS_PER_US);
}

void delay(uint32_t ms)
{
    uint32_t start = millis();

    while ((millis() - start) < ms)
    {
        ;
    }
}

bool MILLIS_IntervalElapsed(uint32_t *lastTime, uint32_t interval)
{
    bool elapsed = false;

    if (NULL != lastTime)
    {
        uint32_t now = millis();

        if ((now - *lastTime) >= interval)
        {
            *lastTime += interval;
            elapsed = true;
        }
    }

    return elapsed;
}
