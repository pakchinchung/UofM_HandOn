/*
 * @file menu.c
 *
 * @brief Home menu that selects which game to run.
 */

#include <stdio.h>
#include <stddef.h>
#include "menu.h"
#include "ssd1306.h"
#include "mcp23008.h"
#include "millis.h"

#define MENU_TITLE_Y  (2)
#define MENU_RULE_Y   (12)
#define MENU_FIRST_Y  (17)
#define MENU_ROW_PITCH (11)
#define MENU_TEXT_X   (16)
#define MENU_HINT_Y   (55)

static const char *const menuLabels[MENU_ITEM_COUNT] =
{
    "DINO RUN",
    "BREAKOUT",
    "HARDWARE TEST",
};

static uint8_t menuCursor = 0;
static uint32_t menuLastFrame = 0;
static bool menuDirty = true;

static void MENU_Draw(void)
{
    uint8_t i;

    SSD1306_Clear();

    SSD1306_TextDrawCentred(MENU_TITLE_Y, "SELECT GAME");
    SSD1306_HLineDraw(0, MENU_RULE_Y, SSD1306_WIDTH, SSD1306_PIXEL_SET);

    for (i = 0; i < (uint8_t)MENU_ITEM_COUNT; i++)
    {
        int16_t rowY = (int16_t)(MENU_FIRST_Y + ((int16_t)i * MENU_ROW_PITCH));

        (void)SSD1306_TextDraw(MENU_TEXT_X, rowY, menuLabels[i]);

        if (i == menuCursor)
        {
            /* Highlight the row by inverting a band across it, which reads more
             * clearly on a small mono panel than a cursor glyph alone. */
            SSD1306_RectFill(MENU_TEXT_X - 6, rowY - 2,
                             (uint8_t)(SSD1306_TextWidth(menuLabels[i]) + 12U),
                             (uint8_t)(FONT_HEIGHT + 3U), SSD1306_PIXEL_XOR);
        }
    }

    SSD1306_TextDrawCentred(MENU_HINT_Y, "GP5 PICK  GP6 GO");

    (void)SSD1306_Update();
}

void MENU_Enter(void)
{
    menuLastFrame = millis();
    menuDirty = true;
}

menu_item_t MENU_Tasks(uint8_t *edges)
{
    menu_item_t chosen = MENU_ITEM_NONE;
    uint32_t now = millis();

    if (NULL != edges)
    {
        if (0U != (*edges & BTN_JUMP))
        {
            *edges &= (uint8_t)~BTN_JUMP;

            menuCursor++;
            if (menuCursor >= (uint8_t)MENU_ITEM_COUNT)
            {
                menuCursor = 0;
            }

            menuDirty = true;
        }

        if (0U != (*edges & BTN_START))
        {
            *edges &= (uint8_t)~BTN_START;
            chosen = (menu_item_t)menuCursor;
            printf("Menu: starting %s\r\n", menuLabels[menuCursor]);
        }
    }

    /* The menu is static, so it is only repainted when the cursor moves. */
    if (menuDirty && ((now - menuLastFrame) >= MENU_FRAME_MS))
    {
        menuLastFrame = now;
        menuDirty = false;
        MENU_Draw();
    }

    return chosen;
}
