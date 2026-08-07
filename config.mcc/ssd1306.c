/*
 * @file ssd1306.c
 *
 * @brief SSD1306 128x64 monochrome OLED driver over TWI0.
 */

#include <string.h>
#include "ssd1306.h"

/* Control byte prefixes. Co = 0, D/C selects command or data. */
#define SSD1306_CTRL_CMD  (0x00U)
#define SSD1306_CTRL_DATA (0x40U)

/* Command opcodes used by the init sequence. */
#define SSD1306_CMD_CONTRAST         (0x81U)
#define SSD1306_CMD_ENTIRE_ON_RESUME (0xA4U)
#define SSD1306_CMD_NORMAL_DISPLAY   (0xA6U)
#define SSD1306_CMD_DISPLAY_OFF      (0xAEU)
#define SSD1306_CMD_DISPLAY_ON       (0xAFU)
#define SSD1306_CMD_MEM_MODE         (0x20U)
#define SSD1306_CMD_COL_ADDR         (0x21U)
#define SSD1306_CMD_PAGE_ADDR        (0x22U)
#define SSD1306_CMD_START_LINE       (0x40U)
#define SSD1306_CMD_SEG_REMAP        (0xA1U)
#define SSD1306_CMD_MUX_RATIO        (0xA8U)
#define SSD1306_CMD_COM_SCAN_DEC     (0xC8U)
#define SSD1306_CMD_DISPLAY_OFFSET   (0xD3U)
#define SSD1306_CMD_COM_PIN_CFG      (0xDAU)
#define SSD1306_CMD_CLK_DIV          (0xD5U)
#define SSD1306_CMD_PRECHARGE        (0xD9U)
#define SSD1306_CMD_VCOM_DESEL       (0xDBU)
#define SSD1306_CMD_CHARGE_PUMP      (0x8DU)
#define SSD1306_CMD_DEACTIVATE_SCROLL (0x2EU)

/* One spare byte is reserved ahead of the pixel data so that a run of pages
 * starting at page 0 can be transmitted straight out of this array with the
 * data control byte already in place. Runs starting at a later page borrow the
 * byte immediately before them, see SSD1306_Update(). */
#define SSD1306_FB_BYTES (SSD1306_PAGES * SSD1306_WIDTH)

static uint8_t ssd1306Buffer[1U + SSD1306_FB_BYTES];

/* Bit n set means page n differs from what the panel is showing. */
static uint8_t ssd1306DirtyPages = 0;

/* Pixel data starts one byte in. Index as fb[page * WIDTH + column]. */
#define FB (&ssd1306Buffer[1])

/* Bitmask covering pages firstPage..lastPage inclusive. */
static uint8_t SSD1306_PageRunMask(uint8_t firstPage, uint8_t lastPage)
{
    uint8_t mask = 0;
    uint8_t page;

    for (page = firstPage; page <= lastPage; page++)
    {
        mask |= (uint8_t)(1U << page);
    }

    return mask;
}

static bool SSD1306_CommandSend(const uint8_t *cmd, uint8_t length)
{
    uint8_t packet[8];
    bool success = false;

    if (length < sizeof(packet))
    {
        packet[0] = SSD1306_CTRL_CMD;
        (void)memcpy(&packet[1], cmd, length);
        success = I2C_Write(SSD1306_I2C_SPEED, SSD1306_I2C_ADDR, packet, (size_t)length + 1U);
    }

    return success;
}

static bool SSD1306_Command1(uint8_t cmd)
{
    return SSD1306_CommandSend(&cmd, 1U);
}

static bool SSD1306_Command2(uint8_t cmd, uint8_t arg)
{
    uint8_t buf[2] = { cmd, arg };

    return SSD1306_CommandSend(buf, 2U);
}

/* Restricts the panel auto-increment window to whole pages firstPage..lastPage
 * across all 128 columns. Horizontal addressing wraps from the end of one page
 * to the start of the next, so a multi-page run is one linear write. */
static bool SSD1306_WindowSet(uint8_t firstPage, uint8_t lastPage)
{
    uint8_t colCmd[3] = { SSD1306_CMD_COL_ADDR, 0U, SSD1306_WIDTH - 1U };
    uint8_t pageCmd[3] = { SSD1306_CMD_PAGE_ADDR, firstPage, lastPage };
    bool success = SSD1306_CommandSend(colCmd, 3U);

    if (success)
    {
        success = SSD1306_CommandSend(pageCmd, 3U);
    }

    return success;
}

/* Init sequence as length-prefixed groups: {count, byte0..byteN-1}, ...
 * terminated by a zero count. Walking a table rather than a chain of calls
 * means a failure can be reported as a step index. */
static const uint8_t ssd1306InitSequence[] =
{
    1U, SSD1306_CMD_DISPLAY_OFF,
    2U, SSD1306_CMD_CLK_DIV, 0x80U,
    2U, SSD1306_CMD_MUX_RATIO, SSD1306_HEIGHT - 1U,
    2U, SSD1306_CMD_DISPLAY_OFFSET, 0x00U,
    1U, SSD1306_CMD_START_LINE,
    2U, SSD1306_CMD_CHARGE_PUMP, 0x14U,
    /* Horizontal addressing mode, so a page run is one contiguous write. */
    2U, SSD1306_CMD_MEM_MODE, 0x00U,
    1U, SSD1306_CMD_SEG_REMAP,
    1U, SSD1306_CMD_COM_SCAN_DEC,
    /* 0x12 = alternating COM pins, the 128x64 wiring. */
    2U, SSD1306_CMD_COM_PIN_CFG, 0x12U,
    2U, SSD1306_CMD_CONTRAST, 0xCFU,
    2U, SSD1306_CMD_PRECHARGE, 0xF1U,
    2U, SSD1306_CMD_VCOM_DESEL, 0x40U,
    1U, SSD1306_CMD_ENTIRE_ON_RESUME,
    1U, SSD1306_CMD_NORMAL_DISPLAY,
    1U, SSD1306_CMD_DEACTIVATE_SCROLL,
    0U
};

static uint8_t ssd1306InitFailStep = 0xFFU;
static uint8_t ssd1306InitFailError = 0U;

bool SSD1306_Initialize(void)
{
    uint16_t offset = 0;
    uint8_t step = 0;
    bool ok = true;

    /* Reset value of the reserved control byte, used for page-0 runs. */
    ssd1306Buffer[0] = SSD1306_CTRL_DATA;
    ssd1306InitFailStep = 0xFFU;
    ssd1306InitFailError = 0U;

    while (0U != ssd1306InitSequence[offset])
    {
        uint8_t count = ssd1306InitSequence[offset];

        if (!SSD1306_CommandSend(&ssd1306InitSequence[offset + 1U], count))
        {
            ssd1306InitFailStep = step;
            ssd1306InitFailError = (uint8_t)I2C_LastErrorGet();
            ok = false;
            break;
        }

        offset += (uint16_t)count + 1U;
        step++;
    }

    if (ok)
    {
        SSD1306_Clear();

        if (!SSD1306_Update())
        {
            /* Distinct step value so a failure pushing the frame buffer is not
             * confused with a failure in the command sequence above. */
            ssd1306InitFailStep = 0xF0U;
            ssd1306InitFailError = (uint8_t)I2C_LastErrorGet();
            ok = false;
        }
    }

    if (ok && !SSD1306_Command1(SSD1306_CMD_DISPLAY_ON))
    {
        ssd1306InitFailStep = 0xF1U;
        ssd1306InitFailError = (uint8_t)I2C_LastErrorGet();
        ok = false;
    }

    return ok;
}

uint8_t SSD1306_InitFailStepGet(void)
{
    return ssd1306InitFailStep;
}

uint8_t SSD1306_InitFailErrorGet(void)
{
    return ssd1306InitFailError;
}

void SSD1306_DisplayEnable(bool on)
{
    (void)SSD1306_Command1(on ? SSD1306_CMD_DISPLAY_ON : SSD1306_CMD_DISPLAY_OFF);
}

void SSD1306_ContrastSet(uint8_t level)
{
    (void)SSD1306_Command2(SSD1306_CMD_CONTRAST, level);
}

void SSD1306_Clear(void)
{
    (void)memset(FB, 0, SSD1306_FB_BYTES);
    ssd1306DirtyPages = 0xFFU;
}

void SSD1306_PagesClear(uint8_t firstPage, uint8_t lastPage)
{
    if ((firstPage < SSD1306_PAGES) && (lastPage < SSD1306_PAGES) && (firstPage <= lastPage))
    {
        uint8_t count = (lastPage - firstPage) + 1U;
        uint8_t page;

        (void)memset(&FB[(uint16_t)firstPage * SSD1306_WIDTH], 0,
                     (size_t)count * SSD1306_WIDTH);

        for (page = firstPage; page <= lastPage; page++)
        {
            ssd1306DirtyPages |= (uint8_t)(1U << page);
        }
    }
}

bool SSD1306_Update(void)
{
    uint8_t page = 0;
    bool ok = true;

    while (page < SSD1306_PAGES)
    {
        if (0U == (ssd1306DirtyPages & (uint8_t)(1U << page)))
        {
            page++;
            continue;
        }

        /* Extend the run over every following dirty page so consecutive dirty
         * pages cost one window setup and one transfer instead of N. */
        uint8_t last = page;

        while (((last + 1U) < SSD1306_PAGES) &&
               (0U != (ssd1306DirtyPages & (uint8_t)(1U << (last + 1U)))))
        {
            last++;
        }

        if (SSD1306_WindowSet(page, last))
        {
            uint16_t runBytes = (uint16_t)((last - page) + 1U) * SSD1306_WIDTH;
            /* The byte immediately before this run in the array: for page 0
             * that is the reserved slot, otherwise it is the last column of the
             * previous page. Borrow it for the data control byte and put it
             * back once the transfer has completed. */
            uint8_t *packet = &ssd1306Buffer[(uint16_t)page * SSD1306_WIDTH];
            uint8_t saved = *packet;

            *packet = SSD1306_CTRL_DATA;
            ok = I2C_Write(SSD1306_I2C_SPEED, SSD1306_I2C_ADDR,
                           packet, (size_t)runBytes + 1U) && ok;
            *packet = saved;
        }
        else
        {
            ok = false;
        }

        ssd1306DirtyPages &= (uint8_t)~SSD1306_PageRunMask(page, last);
        page = last + 1U;
    }

    return ok;
}

void SSD1306_PixelDraw(int16_t x, int16_t y, ssd1306_ink_t ink)
{
    if ((x >= 0) && (x < (int16_t)SSD1306_WIDTH) &&
        (y >= 0) && (y < (int16_t)SSD1306_HEIGHT))
    {
        uint8_t page = (uint8_t)((uint16_t)y >> 3);
        uint8_t mask = (uint8_t)(1U << ((uint16_t)y & 7U));
        uint16_t index = ((uint16_t)page * SSD1306_WIDTH) + (uint16_t)x;

        switch (ink)
        {
            case SSD1306_PIXEL_CLEAR:
                FB[index] &= (uint8_t)~mask;
                break;

            case SSD1306_PIXEL_XOR:
                FB[index] ^= mask;
                break;

            case SSD1306_PIXEL_SET:
            default:
                FB[index] |= mask;
                break;
        }

        ssd1306DirtyPages |= (uint8_t)(1U << page);
    }
}

void SSD1306_HLineDraw(int16_t x, int16_t y, uint8_t width, ssd1306_ink_t ink)
{
    uint8_t i;

    for (i = 0; i < width; i++)
    {
        SSD1306_PixelDraw(x + (int16_t)i, y, ink);
    }
}

void SSD1306_RectFill(int16_t x, int16_t y, uint8_t width, uint8_t height, ssd1306_ink_t ink)
{
    uint8_t row;

    for (row = 0; row < height; row++)
    {
        SSD1306_HLineDraw(x, y + (int16_t)row, width, ink);
    }
}

void SSD1306_SpriteDraw(int16_t x, int16_t y, const sprite_t *sprite)
{
    if ((NULL != sprite) && (NULL != sprite->rows))
    {
        uint8_t row;

        for (row = 0; row < sprite->height; row++)
        {
            uint16_t bits = sprite->rows[row];
            uint8_t col;

            /* Nothing to plot on a blank row, and most sprite rows are sparse. */
            if (0U != bits)
            {
                for (col = 0; col < sprite->width; col++)
                {
                    uint16_t mask = (uint16_t)1U << ((sprite->width - 1U) - col);

                    if (0U != (bits & mask))
                    {
                        SSD1306_PixelDraw(x + (int16_t)col, y + (int16_t)row,
                                          SSD1306_PIXEL_SET);
                    }
                }
            }
        }
    }
}

/* Folds a character into the font's covered range, returning a space for
 * anything it cannot represent. */
static uint8_t SSD1306_GlyphIndex(char c)
{
    uint8_t ch = (uint8_t)c;

    if ((ch >= (uint8_t)'a') && (ch <= (uint8_t)'z'))
    {
        ch = (uint8_t)(ch - 32U);
    }

    if ((ch < FONT_FIRST_CHAR) || (ch > FONT_LAST_CHAR))
    {
        ch = (uint8_t)' ';
    }

    return (uint8_t)(ch - FONT_FIRST_CHAR);
}

int16_t SSD1306_TextDraw(int16_t x, int16_t y, const char *text)
{
    int16_t cursor = x;

    if (NULL != text)
    {
        const char *p = text;

        while ('\0' != *p)
        {
            const uint8_t *glyph = &font5x7[(uint16_t)SSD1306_GlyphIndex(*p) * FONT_WIDTH];
            uint8_t col;

            for (col = 0; col < FONT_WIDTH; col++)
            {
                uint8_t bits = glyph[col];
                uint8_t bit;

                for (bit = 0; bit < FONT_HEIGHT; bit++)
                {
                    if (0U != (bits & (uint8_t)(1U << bit)))
                    {
                        SSD1306_PixelDraw(cursor + (int16_t)col, y + (int16_t)bit,
                                          SSD1306_PIXEL_SET);
                    }
                }
            }

            cursor += (int16_t)FONT_ADVANCE;
            p++;
        }
    }

    return cursor;
}

int16_t SSD1306_TextDrawUint(int16_t x, int16_t y, uint16_t value, uint8_t minDigits)
{
    char text[6];
    uint8_t length = 0;
    uint8_t i;

    /* Build the digits least significant first, then reverse in place. */
    do
    {
        text[length] = (char)('0' + (char)(value % 10U));
        value /= 10U;
        length++;
    } while ((0U != value) && (length < 5U));

    while ((length < minDigits) && (length < 5U))
    {
        text[length] = '0';
        length++;
    }

    for (i = 0; i < (length / 2U); i++)
    {
        char swap = text[i];

        text[i] = text[(length - 1U) - i];
        text[(length - 1U) - i] = swap;
    }

    text[length] = '\0';

    return SSD1306_TextDraw(x, y, text);
}

int16_t SSD1306_TextDrawHex8(int16_t x, int16_t y, uint8_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[3];

    text[0] = digits[(value >> 4) & 0x0FU];
    text[1] = digits[value & 0x0FU];
    text[2] = '\0';

    return SSD1306_TextDraw(x, y, text);
}

void SSD1306_RectDraw(int16_t x, int16_t y, uint8_t width, uint8_t height, ssd1306_ink_t ink)
{
    if ((width > 0U) && (height > 0U))
    {
        uint8_t i;

        SSD1306_HLineDraw(x, y, width, ink);
        SSD1306_HLineDraw(x, y + (int16_t)height - 1, width, ink);

        for (i = 1U; i < (height - 1U); i++)
        {
            SSD1306_PixelDraw(x, y + (int16_t)i, ink);
            SSD1306_PixelDraw(x + (int16_t)width - 1, y + (int16_t)i, ink);
        }
    }
}

uint8_t SSD1306_TextWidth(const char *text)
{
    uint8_t width = 0;

    if ((NULL != text) && ('\0' != *text))
    {
        const char *p = text;
        uint8_t count = 0;

        while ('\0' != *p)
        {
            count++;
            p++;
        }

        width = (uint8_t)((count * FONT_ADVANCE) - 1U);
    }

    return width;
}

void SSD1306_TextDrawCentred(int16_t y, const char *text)
{
    uint8_t width = SSD1306_TextWidth(text);
    int16_t x = 0;

    if (width < SSD1306_WIDTH)
    {
        x = (int16_t)((SSD1306_WIDTH - width) / 2U);
    }

    (void)SSD1306_TextDraw(x, y, text);
}
