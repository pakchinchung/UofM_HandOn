/*
 * @file pot.c
 *
 * @brief Potentiometer on PD7 read through the generated ADC0 driver.
 */

#include "pot.h"
#include "millis.h"
#include "mcc_generated_files/system/system.h"

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

/* CLK_ADC has to land inside 50 kHz..1.5 MHz for a 12-bit conversion, so the
 * prescaler is derived from F_CPU. MCC emits PRESC_DIV2, which was fine at
 * 4 MHz but is 12 MHz at F_CPU 24 MHz: eight times over the limit. */
#define POT_ADC_CLK_MAX (1500000UL)

#if   ((F_CPU / 2UL) <= POT_ADC_CLK_MAX)
  #define POT_ADC_PRESC ADC_PRESC_DIV2_gc
#elif ((F_CPU / 4UL) <= POT_ADC_CLK_MAX)
  #define POT_ADC_PRESC ADC_PRESC_DIV4_gc
#elif ((F_CPU / 8UL) <= POT_ADC_CLK_MAX)
  #define POT_ADC_PRESC ADC_PRESC_DIV8_gc
#elif ((F_CPU / 12UL) <= POT_ADC_CLK_MAX)
  #define POT_ADC_PRESC ADC_PRESC_DIV12_gc
#elif ((F_CPU / 16UL) <= POT_ADC_CLK_MAX)
  #define POT_ADC_PRESC ADC_PRESC_DIV16_gc
#elif ((F_CPU / 20UL) <= POT_ADC_CLK_MAX)
  #define POT_ADC_PRESC ADC_PRESC_DIV20_gc
#elif ((F_CPU / 24UL) <= POT_ADC_CLK_MAX)
  #define POT_ADC_PRESC ADC_PRESC_DIV24_gc
#else
  #define POT_ADC_PRESC ADC_PRESC_DIV32_gc
#endif

/* Extra sample cycles. A potentiometer wiper is a high impedance source and
 * MCC's SAMPLEN of 0 does not give the sample capacitor time to charge, which
 * shows up as a reading that lags or reads low when the knob is mid travel. */
#define POT_ADC_SAMPLEN (14U)

/* Written by the conversion-done callback in interrupt context. */
static volatile uint16_t potAverage = 0;
static volatile bool potBusy = false;

static uint32_t potLastSample = 0;

/* Runs in ADC0_RESRDY interrupt context. The MCC ISR has already cleared the
 * flag, so this only has to take the result and fold it into the average. */
static void POT_ConversionDone(void)
{
    /* adc_result_t is signed; a single-ended 12-bit result is 0..4095. */
    int16_t sample = (int16_t)ADC0_ConversionResultGet();
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

void POT_Initialize(void)
{
    /* ADC0_Initialize() and VREF_Initialize() have already run inside
     * SYSTEM_Initialize(). Three of their settings are wrong for this signal and
     * are corrected here; the rest of the peripheral is left to MCC. Change them
     * in the MCC UI and these writes become no-ops.
     *
     *   1. PRESC   - MCC emits DIV2, which is 12 MHz CLK_ADC at F_CPU 24 MHz
     *                against a 1.5 MHz maximum.
     *   2. SAMPLEN - MCC emits 0, too short for a potentiometer wiper.
     *   3. ADC0REF - MCC emits the internal 1.024 V reference. The pot divides
     *                VDD, so everything above 1.024 V would read as full scale.
     *
     * Configuration is changed with the ADC disabled, then re-enabled. */
    ADC0_Disable();

    ADC0.CTRLC = POT_ADC_PRESC;
    ADC0.SAMPCTRL = POT_ADC_SAMPLEN;
    VREF.ADC0REF = VREF_REFSEL_VDD_gc;

    ADC0_Enable();

    ADC0_ConversionDoneCallbackRegister(POT_ConversionDone);
    ADC0_ChannelSelect(ADC0_CHANNEL_AIN7);

    /* Seed the average with a real reading so the first frame is not a ramp up
     * from zero. The first conversion after enabling is taken while the
     * reference is still settling, so it is discarded. */
    (void)ADC0_ChannelSelectAndConvert(ADC0_CHANNEL_AIN7);
    potAverage = (uint16_t)ADC0_ChannelSelectAndConvert(ADC0_CHANNEL_AIN7);

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
        ADC0_ConversionStart();
    }
}

uint16_t POT_RawGet(void)
{
    uint16_t value;

    /* 16-bit read of a value the ADC interrupt can change mid-access. */
    ENTER_CRITICAL(sreg);
    value = potAverage;
    EXIT_CRITICAL(sreg);

    return value;
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
