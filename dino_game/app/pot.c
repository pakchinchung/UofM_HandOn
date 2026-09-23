/*
 * @file pot.c
 *
 * @brief Potentiometer on PA29 (ADC0 AIN29), register-level ADC0.
 */

#include "pot.h"
#include "millis.h"
#include "device.h"

/* Shift used by the exponential moving average: avg += (sample - avg) >> N.
 * Only applied while the knob is nearly still. */
#define POT_FILTER_SHIFT (2U)

/* Movement larger than this many counts is treated as the user deliberately
 * turning the knob, and is tracked with no filtering at all.
 *
 * A plain moving average is wrong for a paddle: at a 5 ms sample interval a
 * shift of 2 gives roughly a 20 ms time constant, which reads as lag when you
 * are trying to intercept a ball. Smoothing only exists to stop the last couple
 * of ADC bits flickering, and that noise is small. 24 counts out of 4095 is
 * under 0.6% of travel, well under one pixel of paddle movement, so real input
 * always crosses the threshold and arrives immediately. */
#define POT_MOVE_THRESHOLD (24)

/* ADC0 is clocked from the 24 MHz bus clock through CTRLB.PRESCALER, with no
 * GCLK channel of its own. DIV16 gives a 1.5 MHz CLK_ADC, the same rate the
 * AVR build settled on. */
#define POT_ADC_PRESC ADC_CTRLB_PRESCALER_DIV16

/* TIMEBASE is the number of bus clock cycles in 1 us, rounded up. The ADC uses
 * it to time its internal settling delays. */
#define POT_ADC_TIMEBASE (24U)

/* Extra sample cycles. A potentiometer wiper is a high impedance source and a
 * SAMPLEN of 0 does not give the sample capacitor time to charge, which shows
 * up as a reading that lags or reads low when the knob is mid travel. */
#define POT_ADC_SAMPLEN (14U)

/* PA29 = ADC0 AIN29, peripheral function B (MUX_PA29B_ADC0_AIN29). */
#define POT_PIN         (29U)
#define POT_PMUX_FUNC_B (0x01U)

/* Written by the conversion-done callback in interrupt context. */
static volatile uint16_t potAverage = 0;
static volatile bool potBusy = false;

static uint32_t potLastSample = 0;

/* ADC0 result ready. Reading RESULT clears RESRDY. */
void ADC0_Handler(void)
{
    /* A single-ended 12-bit result is 0..4095. */
    int16_t sample = (int16_t)(ADC0_REGS->ADC_RESULT & 0x0FFFU);
    int16_t delta;

    if (sample < 0)
    {
        sample = 0;
    }

    delta = sample - (int16_t)potAverage;

    {
        int16_t magnitude = (delta < 0) ? (int16_t)-delta : delta;

        if (magnitude > POT_MOVE_THRESHOLD)
        {
            /* Deliberate movement: no filtering, so the paddle has no lag. */
            potAverage = (uint16_t)sample;
        }
        else if (magnitude <= (int16_t)(1U << POT_FILTER_SHIFT))
        {
            /* The shift discards the final increments, so snap once inside one
             * step or the average would never quite reach the endpoints. */
            potAverage = (uint16_t)sample;
        }
        else
        {
            /* Nearly still: smooth away the last noisy ADC bits. */
            potAverage = (uint16_t)((int16_t)potAverage + (delta >> POT_FILTER_SHIFT));
        }
    }

    potBusy = false;
}

static void POT_Start(void)
{
    ADC0_REGS->ADC_COMMAND = ADC_COMMAND_MODE_SINGLE | ADC_COMMAND_START_IMMEDIATE;
}

/* Blocking conversion, only used before the interrupt is enabled. */
static uint16_t POT_ConvertBlocking(void)
{
    POT_Start();

    while (0U == (ADC0_REGS->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk))
    {
        ;
    }

    return (uint16_t)(ADC0_REGS->ADC_RESULT & 0x0FFFU);
}

void POT_Initialize(void)
{
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_ADC0_Msk;

    /* Analog pin: digital input buffer off, mux to function B. PA29 is odd,
     * so it is the high nibble of PMUX[14]. */
    PORT_REGS->GROUP[0].PORT_DIRCLR = (1UL << POT_PIN);
    PORT_REGS->GROUP[0].PORT_PMUX[POT_PIN / 2U] =
        (uint8_t)((PORT_REGS->GROUP[0].PORT_PMUX[POT_PIN / 2U] & 0x0FU) | (POT_PMUX_FUNC_B << 4));
    PORT_REGS->GROUP[0].PORT_PINCFG[POT_PIN] = PORT_PINCFG_PMUXEN_Msk;

    ADC0_REGS->ADC_CTRLA = ADC_CTRLA_SWRST_Msk;
    while (0U != (ADC0_REGS->ADC_CTRLA & ADC_CTRLA_SWRST_Msk))
    {
        ;
    }

    /* Configuration registers are enable-protected, so write them first. */
    ADC0_REGS->ADC_CTRLB = POT_ADC_PRESC | ADC_CTRLB_TIMEBASE(POT_ADC_TIMEBASE);
    /* The pot divides VDD, so VDD is the only reference that reads full
     * travel. */
    ADC0_REGS->ADC_CTRLC = ADC_CTRLC_REFSEL(ADC_CTRLC_REFSEL_VDD_Val);
    ADC0_REGS->ADC_CTRLD = ADC_CTRLD_RESOLUTION(ADC_CTRLD_RESOLUTION_12BIT_Val);
    ADC0_REGS->ADC_CTRLE = ADC_CTRLE_SAMPLEN(POT_ADC_SAMPLEN);
    ADC0_REGS->ADC_INPUTCTRL = ADC_INPUTCTRL_MUXPOS(ADC_INPUTCTRL_MUXPOS_AIN29_Val) |
                               ADC_INPUTCTRL_MUXNEG(ADC_INPUTCTRL_MUXNEG_GND_Val);

    ADC0_REGS->ADC_CTRLA = ADC_CTRLA_ENABLE_Msk;

    /* Seed the average with a real reading so the first frame is not a ramp up
     * from zero. The first conversion after enabling is taken while the
     * reference is still settling, so it is discarded. */
    (void)POT_ConvertBlocking();
    potAverage = POT_ConvertBlocking();

    ADC0_REGS->ADC_INTENSET = ADC_INTENSET_RESRDY_Msk;
    NVIC_EnableIRQ(ADC0_IRQn);

    potBusy = false;
    potLastSample = millis();
}

void POT_Tasks(void)
{
    uint32_t now = millis();

    /* Fire and forget: the conversion completes in the interrupt, so the main
     * loop never blocks waiting for the ADC. */
    if (!potBusy && ((now - potLastSample) >= POT_SAMPLE_MS))
    {
        potLastSample = now;
        potBusy = true;
        POT_Start();
    }
}

uint16_t POT_RawGet(void)
{
    /* An aligned 16-bit load is atomic on Cortex-M0+. */
    return potAverage;
}

uint8_t POT_PercentGet(void)
{
    return (uint8_t)(((uint32_t)POT_RawGet() * 100UL) / POT_MAX_COUNT);
}

uint16_t POT_ScaledGet(uint16_t min, uint16_t max)
{
    uint16_t scaled = min;

    if (max > min)
    {
        uint32_t span = (uint32_t)(max - min);

        scaled = (uint16_t)(min + (uint16_t)(((uint32_t)POT_RawGet() * span) / POT_MAX_COUNT));
    }

    return scaled;
}
