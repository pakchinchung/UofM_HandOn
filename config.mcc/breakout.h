/*
 * @file breakout.h
 *
 * @brief Breakout on the SSD1306, with the PD7 potentiometer as the paddle.
 *
 * Controls: the pot slides the paddle, GP5 launches the ball, GP6 starts or
 * restarts, GP7 returns to the title screen.
 *
 * Rendering differs from the dino game on purpose. There the whole play area was
 * cleared and redrawn every frame, which is right when everything on screen is
 * moving. Here most of the screen is static bricks, so the scene is drawn once
 * and then only the ball and the paddle are erased and redrawn in place. Because
 * SSD1306_MaskApply() marks pages dirty as it touches them, a typical frame ends
 * up pushing two or three pages, around 385 bytes, rather than the full 1025.
 *
 * The one hazard with erase-in-place is that clearing the ball's old rectangle
 * can bite into a brick it was resting against, so BRK_BricksRedrawIn() repaints
 * any surviving brick that overlaps the erased area.
 */

#ifndef BREAKOUT_H
#define BREAKOUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Fixed logic timestep in milliseconds. */
#define BRK_TICK_MS (20U)

/** @brief Minimum render interval in milliseconds. */
#define BRK_FRAME_MS (20U)

/**
 * @brief Resets state and draws the title screen.
 * @pre SSD1306_Initialize(), MCP23008_Initialize() and POT_Initialize() must
 *      already have run.
 * @param None.
 * @return None.
 */
void BRK_Initialize(void);

/**
 * @brief Advances the fixed-step logic and renders when due. Call once per main
 *        loop pass; it paces itself.
 * @param edges - Pointer to the shell's latched button press mask. Cleared once
 *                a logic tick has consumed it, so a press made between ticks is
 *                not lost.
 * @return None.
 */
void BRK_Tasks(uint8_t *edges);

/**
 * @brief Returns the current score.
 * @param None.
 * @return Score in points.
 */
uint16_t BRK_ScoreGet(void);

/**
 * @brief Returns the best score since power-up.
 * @param None.
 * @return High score in points.
 */
uint16_t BRK_HighScoreGet(void);

#ifdef __cplusplus
}
#endif

#endif /* BREAKOUT_H */
