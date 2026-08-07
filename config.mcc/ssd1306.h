/*
 * @file ssd1306.h
 *
 * @brief SSD1306 128x64 monochrome OLED driver over TWI0.
 *
 * Rendering goes into a RAM frame buffer and SSD1306_Update() pushes it out.
 * The buffer tracks which of the eight 8-pixel-tall pages changed since the
 * last update and only transmits those, as a small number of contiguous runs.
 * For the game that means the static score row is not resent every frame.
 */

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "gfx_assets.h"
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Panel width in pixels. */
#define SSD1306_WIDTH  (128U)
/** @brief Panel height in pixels. */
#define SSD1306_HEIGHT (64U)
/** @brief Number of 8-pixel-tall pages. */
#define SSD1306_PAGES  (SSD1306_HEIGHT / 8U)

/** @brief 7-bit I2C address. 0x3D is the SA0-high variant, 0x3C is SA0-low. */
#define SSD1306_I2C_ADDR  (0x3DU)
/** @brief SCL frequency used for display traffic. */
#define SSD1306_I2C_SPEED (I2C_SPEED_STANDARD)

/** @brief Pixel operations accepted by the drawing primitives. */
typedef enum
{
    SSD1306_PIXEL_CLEAR = 0, /**< Force the pixel off */
    SSD1306_PIXEL_SET   = 1, /**< Force the pixel on */
    SSD1306_PIXEL_XOR   = 2, /**< Invert the pixel */
} ssd1306_ink_t;

/**
 * @brief Runs the panel power-up and configuration sequence and clears it.
 * @param None.
 * @retval true if the panel acknowledged the whole init sequence
 * @retval false if the panel did not respond
 */
bool SSD1306_Initialize(void);

/**
 * @brief Returns which step of the init sequence failed, for diagnostics.
 *
 * Step 0 is the very first command, so a failure there means the address itself
 * was not acknowledged: wrong address, or SDA/SCL not reaching the panel. A
 * failure at a later step means the panel answered once and then stopped, which
 * points at marginal timing, weak pull-ups or a brown-out instead.
 *
 * @param None.
 * @return Zero-based index of the failing step, or 0xFF if init succeeded.
 */
uint8_t SSD1306_InitFailStepGet(void);

/**
 * @brief Returns the I2C error recorded when the init sequence failed.
 * @param None.
 * @return The i2c_host_error_t value seen at the failing step.
 */
uint8_t SSD1306_InitFailErrorGet(void);

/**
 * @brief Turns the panel output on or off. The frame buffer is untouched.
 * @param on - true to enable the display, false to blank it.
 * @return None.
 */
void SSD1306_DisplayEnable(bool on);

/**
 * @brief Sets the contrast / segment current.
 * @param level - 0x00 (dimmest) to 0xFF (brightest).
 * @return None.
 */
void SSD1306_ContrastSet(uint8_t level);

/**
 * @brief Clears the whole frame buffer and marks every page dirty.
 * @param None.
 * @return None.
 */
void SSD1306_Clear(void);

/**
 * @brief Clears an inclusive range of pages and marks them dirty.
 *        Used by the game to wipe only the play area each frame.
 * @param firstPage - First page to clear, 0..SSD1306_PAGES-1.
 * @param lastPage - Last page to clear, inclusive.
 * @return None.
 */
void SSD1306_PagesClear(uint8_t firstPage, uint8_t lastPage);

/**
 * @brief Transmits every dirty page to the panel and clears the dirty flags.
 * @param None.
 * @retval true if all runs were transmitted successfully
 * @retval false if any transfer failed
 */
bool SSD1306_Update(void);

/**
 * @brief Writes one pixel. Coordinates outside the panel are discarded.
 * @param x - Column, 0 is the left edge.
 * @param y - Row, 0 is the top edge.
 * @param ink - Operation to apply.
 * @return None.
 */
void SSD1306_PixelDraw(int16_t x, int16_t y, ssd1306_ink_t ink);

/**
 * @brief Draws a horizontal line, clipped to the panel.
 * @param x - Leftmost column.
 * @param y - Row.
 * @param width - Length in pixels.
 * @param ink - Operation to apply.
 * @return None.
 */
void SSD1306_HLineDraw(int16_t x, int16_t y, uint8_t width, ssd1306_ink_t ink);

/**
 * @brief Draws a filled rectangle, clipped to the panel.
 * @param x - Left edge.
 * @param y - Top edge.
 * @param width - Width in pixels.
 * @param height - Height in pixels.
 * @param ink - Operation to apply.
 * @return None.
 */
void SSD1306_RectFill(int16_t x, int16_t y, uint8_t width, uint8_t height, ssd1306_ink_t ink);

/**
 * @brief Blits a sprite, clipped to the panel. Clear bits are transparent.
 * @param x - Left edge of the sprite.
 * @param y - Top edge of the sprite.
 * @param sprite - Sprite to draw.
 * @return None.
 */
void SSD1306_SpriteDraw(int16_t x, int16_t y, const sprite_t *sprite);

/**
 * @brief Draws a NUL terminated string in the 5x7 font. Lower case is folded to
 *        upper case; characters outside the font render as blanks. y need not be
 *        page aligned.
 * @param x - Left edge of the first glyph.
 * @param y - Top edge of the text.
 * @param text - String to draw.
 * @return X coordinate just past the last glyph drawn.
 */
int16_t SSD1306_TextDraw(int16_t x, int16_t y, const char *text);

/**
 * @brief Draws an unsigned decimal number, zero padded to minDigits.
 * @param x - Left edge of the first glyph.
 * @param y - Top edge of the text.
 * @param value - Value to render.
 * @param minDigits - Minimum digits to emit, 1..5. Shorter values are zero
 *                    padded so a changing number does not shift on screen.
 * @return X coordinate just past the last glyph drawn.
 */
int16_t SSD1306_TextDrawUint(int16_t x, int16_t y, uint16_t value, uint8_t minDigits);

/**
 * @brief Draws a byte as two upper case hex digits, with no 0x prefix.
 * @param x - Left edge of the first glyph.
 * @param y - Top edge of the text.
 * @param value - Byte to render.
 * @return X coordinate just past the last glyph drawn.
 */
int16_t SSD1306_TextDrawHex8(int16_t x, int16_t y, uint8_t value);

/**
 * @brief Draws a rectangle outline, clipped to the panel.
 * @param x - Left edge.
 * @param y - Top edge.
 * @param width - Width in pixels.
 * @param height - Height in pixels.
 * @param ink - Operation to apply.
 * @return None.
 */
void SSD1306_RectDraw(int16_t x, int16_t y, uint8_t width, uint8_t height, ssd1306_ink_t ink);

/**
 * @brief Returns the pixel width a string would occupy.
 * @param text - String to measure.
 * @return Width in pixels, excluding the trailing inter-glyph gap.
 */
uint8_t SSD1306_TextWidth(const char *text);

/**
 * @brief Draws a string horizontally centred on the panel.
 * @param y - Top edge of the text.
 * @param text - String to draw.
 * @return None.
 */
void SSD1306_TextDrawCentred(int16_t y, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */
