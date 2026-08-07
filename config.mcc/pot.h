/*
 * @file pot.h
 *
 * @brief Potentiometer on PD7 read through ADC0.
 *
 * PD7 is ADC0 analog input AIN7, configured by the generated ADC0 and VREF
 * drivers. This module drives conversions through that driver rather than
 * touching registers, with three exceptions documented in POT_Initialize()
 * where the generated settings are wrong for a potentiometer.
 *
 * Conversions are interrupt driven: POT_Tasks() only starts one, and the
 * ADC0_RESRDY callback folds the result into an exponential moving average. The
 * main loop therefore never blocks on the ADC, and the last couple of noisy ADC
 * bits do not flicker on screen or jitter the game speed.
 */

#ifndef POT_H
#define POT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Interval between conversions, in milliseconds. Halved from 10 ms so a
 *         paddle tracks the knob within one 20 ms game tick. */
#define POT_SAMPLE_MS (5U)

/** @brief Full scale reading of a 12-bit conversion. */
#define POT_MAX_COUNT (4095U)

/**
 * @brief Configures ADC0 for AIN7 and takes a first reading.
 * @param None.
 * @return None.
 */
void POT_Initialize(void);

/**
 * @brief Starts a conversion when POT_SAMPLE_MS has elapsed and folds the
 *        result into the average. Call often; it rate limits itself.
 * @param None.
 * @return None.
 */
void POT_Tasks(void);

/**
 * @brief Returns the smoothed reading.
 * @param None.
 * @return 0 to POT_MAX_COUNT.
 */
uint16_t POT_RawGet(void);

/**
 * @brief Returns the smoothed reading scaled to a percentage.
 * @param None.
 * @return 0 to 100.
 */
uint8_t POT_PercentGet(void);

/**
 * @brief Scales the smoothed reading into an arbitrary inclusive range.
 * @param min - Value returned at the counter-clockwise end.
 * @param max - Value returned at the clockwise end. Must be >= min.
 * @return Scaled value between min and max inclusive.
 */
uint16_t POT_ScaledGet(uint16_t min, uint16_t max);

#ifdef __cplusplus
}
#endif

#endif /* POT_H */
