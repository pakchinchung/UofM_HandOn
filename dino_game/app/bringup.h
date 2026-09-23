/*
 * @file bringup.h
 *
 * @brief Hardware bring-up screen: proves the OLED, the MCP23008 buttons and
 *        the PD7 potentiometer are all alive before any game code exists.
 *
 * Shows, live on the OLED:
 *   - the raw and smoothed pot reading, as a number, a percentage and a bar
 *   - the three buttons as filled or hollow boxes, plus the raw GPIO byte
 *   - online / offline status for each I2C device
 *   - the measured frame rate and the millis() uptime, which together confirm
 *     the timer tick and the display refresh path
 *
 * The same information is mirrored to the UART, but only when something
 * changes, so the 32 byte TX ring is never the bottleneck.
 */

#ifndef BRINGUP_H
#define BRINGUP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Draws the static parts of the bring-up screen and resets its counters.
 * @pre SSD1306_Initialize(), MCP23008_Initialize() and POT_Initialize() must
 *      already have run.
 * @param None.
 * @return None.
 */
void BRINGUP_Initialize(void);

/**
 * @brief Redraws the live parts of the screen and pushes the dirty pages out.
 *        Call once per main loop pass; it paces itself to BRINGUP_FRAME_MS.
 * @param None.
 * @return None.
 */
void BRINGUP_Tasks(void);

#ifdef __cplusplus
}
#endif

#endif /* BRINGUP_H */
