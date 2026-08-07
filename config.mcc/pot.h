/*
 * @file pot.h
 *
 * @brief Potentiometer on PD7 read through ADC0.
 *
 * PD7 is ADC0 analog input AIN7. MCC does not configure the ADC in this
 * project, so this module sets it up directly: VDD reference, CLK_PER/4 for a
 * 1 MHz ADC clock at F_CPU = 4 MHz, 12-bit single-ended conversions, and an
 * extended sample time because a potentiometer wiper is a high impedance
 * source. The digital input buffer on PD7 is disabled to stop it loading the
 * analog input and to save the switching current.
 *
 * Results are passed through an exponential moving average so the last couple
 * of ADC bits do not flicker on screen or jitter the game speed.
 */

#ifndef POT_H
#define POT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Interval between conversions, in milliseconds. */
#define POT_SAMPLE_MS (10U)

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
