/*
 * @file menu.h
 *
 * @brief Home menu that selects which game to run.
 *
 * Controls: GP5 moves the cursor, GP6 starts the highlighted entry. GP7 is not
 * handled here; the shell in main.c owns it globally so it returns to this menu
 * from anywhere.
 */

#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Minimum render interval in milliseconds. */
#define MENU_FRAME_MS (40U)

/** @brief Entries on the home menu, in display order. */
typedef enum
{
    MENU_ITEM_DINO = 0,
    MENU_ITEM_BREAKOUT,
    MENU_ITEM_DIAGNOSTIC,
    MENU_ITEM_COUNT,
    MENU_ITEM_NONE, /**< Returned while no choice has been confirmed */
} menu_item_t;

/**
 * @brief Resets the cursor and draws the menu.
 * @param None.
 * @return None.
 */
void MENU_Enter(void);

/**
 * @brief Handles input and redraws when due.
 * @param edges - Pointer to the shell's latched button press mask. Bits this
 *                function acts on are cleared.
 * @return The chosen entry once GP6 is pressed, otherwise MENU_ITEM_NONE.
 */
menu_item_t MENU_Tasks(uint8_t *edges);

#ifdef __cplusplus
}
#endif

#endif /* MENU_H */
