/*
 * @file millis.c
 *
 * @brief Arduino-style millis()/micros()/delay() timebase built on TCA0.
 */

#include "millis.h"
#include "mcc_generated_files/timer/tca0.h"
#include "mcc_generated_files/system/utils/atomic.h"

/* TCA0 counts TCA0_CLOCK_FREQ ticks/second, so this many ticks per us. */
#define MILLIS_TICKS_PER_US (TCA0_CLOCK_FREQ / 1000000UL)

static volatile uint32_t millisCounter = 0;

static void MILLIS_Tick(void);

static void MILLIS_Tick(void)
{
    millisCounter++;
}

void MILLIS_Initialize(void)
{
    millisCounter = 0;
    Timer0_OverflowCallbackRegister(MILLIS_Tick);
}

uint32_t millis(void)
{
    uint32_t value;

    /* The counter is 32-bit on an 8-bit core, so the read must be atomic. */
    ENTER_CRITICAL(sreg);
    value = millisCounter;
    EXIT_CRITICAL(sreg);

    return value;
}

uint32_t micros(void)
{
    uint32_t ms;
    uint16_t cnt;

    ENTER_CRITICAL(sreg);

    cnt = (uint16_t)TCA0.SINGLE.CNT;

    /* With interrupts masked the overflow ISR cannot run, so a pending OVF
     * flag means the counter has already wrapped and millisCounter is one
     * tick stale. Re-read CNT so it is the post-wrap value. */
    if (0U != (TCA0.SINGLE.INTFLAGS & TCA_SINGLE_OVF_bm))
    {
        cnt = (uint16_t)TCA0.SINGLE.CNT;
        ms = millisCounter + 1UL;
    }
    else
    {
        ms = millisCounter;
    }

    EXIT_CRITICAL(sreg);

    return (ms * 1000UL) + ((uint32_t)cnt / MILLIS_TICKS_PER_US);
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
