/*
 * @file gfx_assets.h
 *
 * @brief 5x7 font and game sprite tables.
 */

#ifndef GFX_ASSETS_H
#define GFX_ASSETS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief First character present in the font table. */
#define FONT_FIRST_CHAR (0x20U)
/** @brief Last character present in the font table. */
#define FONT_LAST_CHAR  (0x5FU)
/** @brief Glyph width in pixels. */
#define FONT_WIDTH      (5U)
/** @brief Glyph height in pixels. */
#define FONT_HEIGHT     (7U)
/** @brief Horizontal advance per character, including the inter-glyph gap. */
#define FONT_ADVANCE    (FONT_WIDTH + 1U)

/**
 * @brief Column-major 5x7 glyphs for characters FONT_FIRST_CHAR..FONT_LAST_CHAR.
 *        Each glyph is FONT_WIDTH bytes, one per pixel column, bit 0 = top row.
 */
extern const uint8_t font5x7[(FONT_LAST_CHAR - FONT_FIRST_CHAR + 1U) * FONT_WIDTH];

/**
 * @brief A 1bpp sprite stored one row per entry, MSB aligned to the left edge.
 *        Bit (width - 1) of a row is the leftmost pixel.
 */
typedef struct
{
    uint8_t width;          /**< Sprite width in pixels, 1..16 */
    uint8_t height;         /**< Sprite height in pixels */
    const uint16_t *rows;   /**< Pointer to @ref height row words */
} sprite_t;

/** @brief Dinosaur, running animation frame A. 12x16. */
extern const sprite_t spriteDinoRunA;
/** @brief Dinosaur, running animation frame B. 12x16. */
extern const sprite_t spriteDinoRunB;
/** @brief Dinosaur, both feet down. Used while airborne and when dead. 12x16. */
extern const sprite_t spriteDinoStand;
/** @brief Small single cactus. 7x14. */
extern const sprite_t spriteCactusSmall;
/** @brief Large cactus cluster. 11x18. */
extern const sprite_t spriteCactusLarge;

#ifdef __cplusplus
}
#endif

#endif /* GFX_ASSETS_H */
