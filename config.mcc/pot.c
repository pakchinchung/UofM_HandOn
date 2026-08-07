/*
 * @file pot.c
 *
 * @brief Potentiometer on PD7 read through ADC0.
 */

#include "pot.h"
#include "millis.h"
#include "mcc_generated_files/system/system.h"

/* Shift used by the exponential moving average: avg += (sample - avg) >> N. */
#define POT_FILTER_SHIFT (2U)

/* Guard so a conversion that never completes cannot wedge the main loop. */
#define POT_CONVERT_TIMEOUT_MS (5U)

static uint16_t potAverage = 0;
static uint32_t potLastSample = 0;

/* Runs one blocking conversion. At 1 MHz CLK_ADC with the extended sample
 * length this is roughly 30 us, short enough to just wait for. */
static bool POT_ConvertBlocking(uint16_t *result)
{
    uint32_t start = millis();
    bool done = false;

    ADC0.COMMAND = ADC_STCONV_bm;

    while (0U == (ADC0.INTFLAGS & ADC_RESRDY_bm))
    {
        if ((millis() - start) >= POT_CONVERT_TIMEOUT_MS)
        {
            break;
        }
    }

    if (0U != (ADC0.INTFLAGS & ADC_RESRDY_bm))
    {
        *result = ADC0.RES;
        ADC0.INTFLAGS = ADC_RESRDY_bm;  /* Flag is cleared by writing a one. */
        done = true;
    }

    return done;
}

void POT_Initialize(void)
{
    uint16_t sample = 0;

    /* PD7 carries the analog signal, so take the digital input buffer out of
     * the way. MCC already leaves PORTD as inputs with no pull-ups. */
    PORTD.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;

    /* VDD as the ADC reference. Full scale therefore tracks the supply, which
     * is what a ratiometric potentiometer divider wants. */
    VREF.ADC0REF = VREF_REFSEL_VDD_gc;

    /* CLK_ADC = CLK_PER / 4 = 1 MHz at F_CPU 4 MHz, inside the 50 kHz to
     * 1.5 MHz window the data sheet allows for 12-bit conversions. */
    ADC0.CTRLC = ADC_PRESC_DIV4_gc;

    /* No result accumulation; one conversion per result. */
    ADC0.CTRLB = ADC_SAMPNUM_NONE_gc;

    /* Stretch the sample phase. A potentiometer wiper is a high impedance
     * source and the default 2 cycle sample does not charge the S/H cap. */
    ADC0.SAMPCTRL = 14U;

    ADC0.MUXPOS = ADC_MUXPOS_AIN7_gc;

    ADC0.CTRLA = ADC_ENABLE_bm | ADC_RESSEL_12BIT_gc;

    /* Throw the first conversion away; it is taken while the reference and the
     * sample capacitor are still settling. */
    (void)POT_ConvertBlocking(&sample);

    if (POT_ConvertBlocking(&sample))
    {
        potAverage = sample;
    }

    potLastSample = millis();
}

void POT_Tasks(void)
{
    uint32_t now = millis();

    if ((now - potLastSample) >= POT_SAMPLE_MS)
    {
        uint16_t sample = 0;

        potLastSample = now;

        if (POT_ConvertBlocking(&sample))
        {
            /* Signed difference so the average can move in both directions. */
            int16_t delta = (int16_t)sample - (int16_t)potAverage;

            potAverage = (uint16_t)((int16_t)potAverage + (delta >> POT_FILTER_SHIFT));

            /* The shift loses the last increment, so snap the final counts. */
            if ((delta > 0) && (delta < (int16_t)(1U << POT_FILTER_SHIFT)))
            {
                potAverage = sample;
            }
            else if ((delta < 0) && (delta > -(int16_t)(1U << POT_FILTER_SHIFT)))
            {
                potAverage = sample;
            }
            else
            {
                /* Average is still converging. */
            }
        }
    }
}

uint16_t POT_RawGet(void)
{
    return potAverage;
}

uint8_t POT_PercentGet(void)
{
    return (uint8_t)(((uint32_t)potAverage * 100UL) / POT_MAX_COUNT);
}

uint16_t POT_ScaledGet(uint16_t min, uint16_t max)
{
    uint16_t scaled = min;

    if (max > min)
    {
        uint32_t span = (uint32_t)(max - min);

        scaled = (uint16_t)(min + (uint16_t)(((uint32_t)potAverage * span) / POT_MAX_COUNT));
    }

    return scaled;
}
