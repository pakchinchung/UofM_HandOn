/*
 * @file dino_game.h
 *
 * @brief Chrome-style endless runner on the SSD1306, MCP23008 buttons and the
 *        PD7 potentiometer.
 *
 * Controls: GP5 jumps, GP6 starts or restarts, GP7 returns to the title screen.
 * The potentiometer sets the base scroll speed, so difficulty is adjustable
 * without a rebuild.
 *
 * Frame budget: the measured cost is about 19.5 us per byte pushed, so the
 * layout deliberately keeps every moving object inside pages 1..6. The ground
 * sits at y = 54 so page 7 is never dirtied, and the score row on page 0 is only
 * redrawn when the number actually changes. That is 769 bytes per frame instead
 * of 1025, which is the difference between roughly 50 fps and 38 fps.
 *
 * Game logic runs on a fixed 20 ms timestep decoupled from rendering, so the
 * jump arc does not change if a frame takes longer than usual.
 */

#ifndef DINO_GAME_H
#define DINO_GAME_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Fixed logic timestep in milliseconds. */
#define GAME_TICK_MS (20U)

/** @brief Minimum render interval in milliseconds. */
#define GAME_FRAME_MS (20U)

/**
 * @brief Resets state and draws the title screen.
 * @pre SSD1306_Initialize(), MCP23008_Initialize() and POT_Initialize() must
 *      already have run.
 * @param None.
 * @return None.
 */
void GAME_Initialize(void);

/**
 * @brief Advances the fixed-step logic and renders when due. Call once per main
 *        loop pass; it paces itself.
 * @param None.
 * @return None.
 */
void GAME_Tasks(void);

/**
 * @brief Returns the current score.
 * @param None.
 * @return Score in points.
 */
uint16_t GAME_ScoreGet(void);

/**
 * @brief Returns the best score since power-up.
 * @param None.
 * @return High score in points.
 */
uint16_t GAME_HighScoreGet(void);

#ifdef __cplusplus
}
#endif

#endif /* DINO_GAME_H */
