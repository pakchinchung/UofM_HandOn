/*
 * @file bringup.c
 *
 * @brief Hardware bring-up screen for the OLED, buttons and potentiometer.
 */

#include "uart.h"
#include "bringup.h"
#include "ssd1306.h"
#include "mcp23008.h"
#include "pot.h"
#include "millis.h"

/* Minimum frame interval. Set well below what the hardware can sustain so the
 * reported FPS measures the real ceiling rather than this limiter. */
#define BRINGUP_FRAME_MS (5U)

/* Screen layout. Rows are chosen so each block of text sits inside as few
 * 8-pixel pages as possible, which keeps the dirty page count down. */
#define ROW_TITLE   (0)
#define ROW_RULE    (9)
#define ROW_POT     (12)
#define ROW_BAR     (22)
#define ROW_BTN     (34)
#define ROW_STATUS  (46)
#define ROW_FOOTER  (56)

#define BAR_X       (2)
#define BAR_WIDTH   (124)
#define BAR_HEIGHT  (9)

/* Button indicator boxes. */
#define BTN_BOX_Y      (ROW_BTN - 1)
#define BTN_BOX_WIDTH  (11)
#define BTN_BOX_HEIGHT (9)

static uint32_t bringupLastFrame = 0;
static uint32_t bringupFpsWindow = 0;
static uint16_t bringupFrameCount = 0;
static uint16_t bringupFps = 0;

/* Previous values, so the UART mirror only speaks when something moves. */
static uint8_t bringupLastHeld = 0xFFU;
static uint8_t bringupLastPercent = 0xFFU;

/* Hollow box with the label inside when released, inverted when held. The
 * label is drawn first and the fill is XORed over it, so the glyph stays
 * readable either way. */
static void BRINGUP_ButtonBoxDraw(int16_t x, char label, bool held)
{
    char text[2];

    text[0] = label;
    text[1] = '\0';

    SSD1306_RectDraw(x, BTN_BOX_Y, BTN_BOX_WIDTH, BTN_BOX_HEIGHT, SSD1306_PIXEL_SET);
    (void)SSD1306_TextDraw(x + 3, BTN_BOX_Y + 1, text);

    if (held)
    {
        SSD1306_RectFill(x + 1, BTN_BOX_Y + 1, BTN_BOX_WIDTH - 2U, BTN_BOX_HEIGHT - 2U,
                         SSD1306_PIXEL_XOR);
    }
}

void BRINGUP_Initialize(void)
{
    SSD1306_Clear();

    SSD1306_TextDrawCentred(ROW_TITLE, "DINO BRINGUP");
    SSD1306_HLineDraw(0, ROW_RULE, SSD1306_WIDTH, SSD1306_PIXEL_SET);

    (void)SSD1306_TextDraw(0, ROW_POT, "POT");
    (void)SSD1306_TextDraw(0, ROW_BTN, "BTN");

    bringupLastFrame = millis();
    bringupFpsWindow = bringupLastFrame;
    bringupFrameCount = 0;
    bringupFps = 0;
    bringupLastHeld = 0xFFU;
    bringupLastPercent = 0xFFU;

    (void)SSD1306_Update();
}

void BRINGUP_Tasks(void)
{
    uint32_t now = millis();

    if ((now - bringupLastFrame) < BRINGUP_FRAME_MS)
    {
        return;
    }

    bringupLastFrame = now;

    /* Frame rate over a one second window. */
    bringupFrameCount++;
    if ((now - bringupFpsWindow) >= 1000UL)
    {
        bringupFps = bringupFrameCount;
        bringupFrameCount = 0;
        bringupFpsWindow = now;
    }

    {
        uint16_t raw = POT_RawGet();
        uint8_t percent = POT_PercentGet();
        uint8_t held = MCP23008_Held();
        uint8_t rawGpio = MCP23008_RawGet();
        uint8_t fill;
        int16_t cursor;

        /* Page 0 holds the title and never changes, so only wipe 1..7. Several
         * of the rows below straddle a page boundary, so clearing them as one
         * block is both simpler and safer than per-row ranges. */
        SSD1306_PagesClear(1U, 7U);

        SSD1306_HLineDraw(0, ROW_RULE, SSD1306_WIDTH, SSD1306_PIXEL_SET);

        cursor = SSD1306_TextDraw(0, ROW_POT, "POT");
        cursor = SSD1306_TextDrawUint(cursor + 4, ROW_POT, raw, 4U);
        cursor = SSD1306_TextDrawUint(cursor + 8, ROW_POT, percent, 3U);
        (void)SSD1306_TextDraw(cursor, ROW_POT, "%");

        SSD1306_RectDraw(BAR_X, ROW_BAR, BAR_WIDTH, BAR_HEIGHT, SSD1306_PIXEL_SET);
        fill = (uint8_t)(((uint32_t)raw * (BAR_WIDTH - 4U)) / POT_MAX_COUNT);
        if (fill > 0U)
        {
            SSD1306_RectFill(BAR_X + 2, ROW_BAR + 2, fill, BAR_HEIGHT - 4U,
                             SSD1306_PIXEL_SET);
        }

        /* Buttons. */
        (void)SSD1306_TextDraw(0, ROW_BTN, "BTN");
        BRINGUP_ButtonBoxDraw(24, 'J', (0U != (held & BTN_JUMP)));
        BRINGUP_ButtonBoxDraw(40, 'S', (0U != (held & BTN_START)));
        BRINGUP_ButtonBoxDraw(56, 'R', (0U != (held & BTN_RESET)));

        cursor = SSD1306_TextDraw(76, ROW_BTN, "GP:");
        (void)SSD1306_TextDrawHex8(cursor, ROW_BTN, rawGpio);

        /* Device status. */
        cursor = SSD1306_TextDraw(0, ROW_STATUS, "OLED OK  IO ");
        (void)SSD1306_TextDraw(cursor, ROW_STATUS,
                               MCP23008_IsOnline() ? "OK" : "--");

        cursor = SSD1306_TextDraw(0, ROW_FOOTER, "FPS");
        cursor = SSD1306_TextDrawUint(cursor + 4, ROW_FOOTER, bringupFps, 2U);
        cursor = SSD1306_TextDraw(cursor + 8, ROW_FOOTER, "UP");
        cursor = SSD1306_TextDrawUint(cursor + 4, ROW_FOOTER,
                                      (uint16_t)(millis() / 1000UL), 4U);
        (void)SSD1306_TextDraw(cursor, ROW_FOOTER, "S");

        (void)SSD1306_Update();

        /* Mirror to UART only on change, so the TX ring never throttles the
         * frame rate. The pot is quantised to whole percent for the same reason. */
        if ((held != bringupLastHeld) || (percent != bringupLastPercent))
        {
            bringupLastHeld = held;
            bringupLastPercent = percent;

            UART_Printf("POT %4u (%3u%%)  BTN J%c S%c R%c  GPIO 0x%02X  IO %s  FPS %u\r\n",
                   raw, percent,
                   (0U != (held & BTN_JUMP)) ? '*' : '.',
                   (0U != (held & BTN_START)) ? '*' : '.',
                   (0U != (held & BTN_RESET)) ? '*' : '.',
                   rawGpio,
                   MCP23008_IsOnline() ? "OK" : "--",
                   bringupFps);
        }
    }
}
