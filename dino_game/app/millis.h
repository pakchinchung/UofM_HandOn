/*
 * @file millis.h
 *
 * @brief Arduino-style millis()/micros()/delay() timebase built on SysTick.
 *
 * SysTick runs from the 24 MHz CPU clock with a reload of 23999, so its
 * interrupt fires every 24000 / 24 MHz = 1.000 ms. Every tick bumps a 32-bit
 * counter, which wraps after ~49.7 days.
 */

#ifndef MILLIS_H
#define MILLIS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts SysTick with a 1 ms period and enables its interrupt.
 *        Call once after SYS_Initialize().
 * @param None.
 * @return None.
 */
void MILLIS_Initialize(void);

/**
 * @brief Checks that SysTick really is set up for a 1 ms period.
 *
 * The whole timebase rests on LOAD + 1 CPU clocks equalling one millisecond.
 * If the CPU clock is ever changed in MCC this silently stops being true, so
 * callers can check once at start-up rather than chase a timebase that is
 * quietly wrong.
 *
 * @param None.
 * @retval true if the SysTick period is exactly 1 ms
 * @retval false otherwise
 */
bool MILLIS_TickIsOneMs(void);

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
