/*
 * @file millis.h
 *
 * @brief Arduino-style millis()/micros()/delay() timebase built on TCA0.
 *
 * TCA0 is configured by MCC for a 4 MHz prescaled clock with PER = 0xF9F
 * (3999), so the overflow interrupt fires every 4000 / 4 MHz = 1.000 ms.
 * MILLIS_Initialize() hooks the overflow callback and every tick bumps a
 * 32-bit counter, which wraps after ~49.7 days.
 */

#ifndef MILLIS_H
#define MILLIS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registers the 1 ms tick callback on the TCA0 overflow event.
 *        Call once after SYSTEM_Initialize().
 * @param None.
 * @return None.
 */
void MILLIS_Initialize(void);

/**
 * @brief Returns the number of milliseconds since MILLIS_Initialize().
 *        Safe to call from main context and from an ISR.
 * @param None.
 * @return Milliseconds elapsed, wrapping at 2^32.
 */
uint32_t millis(void);

/**
 * @brief Returns the number of microseconds since MILLIS_Initialize().
 *        Resolution is 0.25 us, rounded down to 1 us.
 * @param None.
 * @return Microseconds elapsed, wrapping at 2^32 (~71.6 minutes).
 */
uint32_t micros(void);

/**
 * @brief Blocks for the given number of milliseconds.
 * @pre Global interrupts must be enabled, otherwise this never returns.
 * @param ms - Number of milliseconds to wait.
 * @return None.
 */
void delay(uint32_t ms);

/**
 * @brief Non-blocking interval helper. Returns true once @p interval ms have
 *        passed since @p *lastTime, and advances @p *lastTime by @p interval.
 *        Unsigned arithmetic makes this wrap-safe.
 * @param lastTime - Pointer to the caller's last-fired timestamp.
 * @param interval - Interval in milliseconds.
 * @retval true if the interval elapsed
 * @retval false otherwise
 */
bool MILLIS_IntervalElapsed(uint32_t *lastTime, uint32_t interval);

#ifdef __cplusplus
}
#endif

#endif /* MILLIS_H */
