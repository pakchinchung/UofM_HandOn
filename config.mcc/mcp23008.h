/*
 * @file mcp23008.h
 *
 * @brief MCP23008 I2C GPIO expander, used here as a debounced button input.
 *
 * Wiring assumed: the three buttons sit on GP5, GP6 and GP7 with external
 * pull-up resistors and switch to ground, so a pressed button reads LOW. The
 * internal pull-ups (GPPU) are therefore left disabled and the raw port byte is
 * inverted on the way in, so everything above the driver sees 1 = pressed.
 */

#ifndef MCP23008_H
#define MCP23008_H

#include <stdint.h>
#include <stdbool.h>
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 7-bit I2C address. */
#define MCP23008_I2C_ADDR  (0x24U)
/** @brief SCL frequency used for expander traffic. */
#define MCP23008_I2C_SPEED_DEFAULT (I2C_SPEED_STANDARD)

/** @brief Interval between GPIO polls, in milliseconds. */
#define MCP23008_POLL_MS (5U)

/** @brief Jump button, GP5. */
#define BTN_JUMP  (0x20U)
/** @brief Start button, GP6. */
#define BTN_START (0x40U)
/** @brief Reset button, GP7. */
#define BTN_RESET (0x80U)
/** @brief Mask of all pins treated as buttons. */
#define BTN_ALL   (BTN_JUMP | BTN_START | BTN_RESET)

/**
 * @brief Configures GP5-GP7 as inputs with the internal pull-ups off and takes
 *        a first sample.
 * @param None.
 * @retval true if the expander acknowledged
 * @retval false if it did not respond
 */
bool MCP23008_Initialize(void);

/**
 * @brief Polls the expander when MCP23008_POLL_MS has elapsed, runs the
 *        debounce filter and latches press and release edges. Call this often
 *        from the main loop; it rate limits itself.
 * @param None.
 * @return None.
 */
void MCP23008_Tasks(void);

/**
 * @brief Sets the SCL frequency used for expander traffic.
 * @param fScl - Desired SCL frequency in hertz.
 * @return None.
 */
void MCP23008_SpeedSet(uint32_t fScl);

/**
 * @brief Returns the debounced button state.
 * @param None.
 * @return Mask of BTN_* bits currently held down.
 */
uint8_t MCP23008_Held(void);

/**
 * @brief Returns and clears the press edges latched since the last call.
 * @param None.
 * @return Mask of BTN_* bits that went from released to held.
 */
uint8_t MCP23008_Pressed(void);

/**
 * @brief Returns and clears the release edges latched since the last call.
 * @param None.
 * @return Mask of BTN_* bits that went from held to released.
 */
uint8_t MCP23008_Released(void);

/**
 * @brief Returns the last raw GPIO register byte, before inversion. Diagnostic.
 * @param None.
 * @return Raw value of the MCP23008 GPIO register.
 */
uint8_t MCP23008_RawGet(void);

/**
 * @brief Reports whether the most recent poll succeeded.
 * @param None.
 * @retval true if the expander is responding
 * @retval false otherwise
 */
bool MCP23008_IsOnline(void);

#ifdef __cplusplus
}
#endif

#endif /* MCP23008_H */
