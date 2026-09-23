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

/* Held rather than hard coded so the boot sweep can find the fastest rung this
 * board's bus capacitance and pull-ups actually tolerate. */
static uint32_t ssd1306Speed = SSD1306_I2C_SPEED_DEFAULT;

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
        success = I2C_Write(ssd1306Speed, SSD1306_I2C_ADDR, packet, (size_t)length + 1U);
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

void SSD1306_SpeedSet(uint32_t fScl)
{
    ssd1306Speed = fScl;
}

uint32_t SSD1306_SpeedGet(void)
{
    return ssd1306Speed;
}

void SSD1306_DirtyAll(void)
{
    ssd1306DirtyPages = 0xFFU;
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
            ok = I2C_Write(ssd1306Speed, SSD1306_I2C_ADDR,
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

/* Applies an 8-pixel column mask to a single page byte. This is the one place
 * that touches the frame buffer for bulk drawing: everything below reduces to
 * whole-byte operations so a filled span costs one operation per 8 pixels
 * instead of one function call per pixel. */
static void SSD1306_MaskApply(uint8_t page, int16_t x, uint8_t mask, ssd1306_ink_t ink)
{
    if ((0U != mask) && (x >= 0) && (x < (int16_t)SSD1306_WIDTH) && (page < SSD1306_PAGES))
    {
        uint8_t *cell = &FB[((uint16_t)page * SSD1306_WIDTH) + (uint16_t)x];

        switch (ink)
        {
            case SSD1306_PIXEL_CLEAR:
                *cell &= (uint8_t)~mask;
                break;

            case SSD1306_PIXEL_XOR:
                *cell ^= mask;
                break;

            case SSD1306_PIXEL_SET:
            default:
                *cell |= mask;
                break;
        }

        ssd1306DirtyPages |= (uint8_t)(1U << page);
    }
}

/* Vertical run of pixels in one column, clipped, split across pages. */
static void SSD1306_VSpanDraw(int16_t x, int16_t y, uint8_t height, ssd1306_ink_t ink)
{
    int16_t top = y;
    int16_t bottom = y + (int16_t)height - 1;

    if ((0U != height) && (bottom >= 0) && (top < (int16_t)SSD1306_HEIGHT))
    {
        uint8_t firstPage;
        uint8_t lastPage;
        uint8_t page;

        if (top < 0)
        {
            top = 0;
        }
        if (bottom >= (int16_t)SSD1306_HEIGHT)
        {
            bottom = (int16_t)SSD1306_HEIGHT - 1;
        }

        firstPage = (uint8_t)((uint16_t)top >> 3);
        lastPage = (uint8_t)((uint16_t)bottom >> 3);

        for (page = firstPage; page <= lastPage; page++)
        {
            /* Partial mask on the first and last page, solid 0xFF between. */
            uint8_t high = (page == firstPage) ? (uint8_t)((uint16_t)top & 7U) : 0U;
            uint8_t low = (page == lastPage) ? (uint8_t)((uint16_t)bottom & 7U) : 7U;
            uint8_t mask = (uint8_t)((uint8_t)(0xFFU << high) & (uint8_t)(0xFFU >> (7U - low)));

            SSD1306_MaskApply(page, x, mask, ink);
        }
    }
}

void SSD1306_HLineDraw(int16_t x, int16_t y, uint8_t width, ssd1306_ink_t ink)
{
    if ((y >= 0) && (y < (int16_t)SSD1306_HEIGHT))
    {
        uint8_t page = (uint8_t)((uint16_t)y >> 3);
        uint8_t mask = (uint8_t)(1U << ((uint16_t)y & 7U));
        uint8_t i;

        for (i = 0; i < width; i++)
        {
            SSD1306_MaskApply(page, x + (int16_t)i, mask, ink);
        }
    }
}

void SSD1306_RectFill(int16_t x, int16_t y, uint8_t width, uint8_t height, ssd1306_ink_t ink)
{
    uint8_t i;

    /* Column at a time: a 5 pixel tall fill is one byte operation per column
     * rather than five pixel calls. */
    for (i = 0; i < width; i++)
    {
        SSD1306_VSpanDraw(x + (int16_t)i, y, height, ink);
    }
}

void SSD1306_SpriteDraw(int16_t x, int16_t y, const sprite_t *sprite)
{
    if ((NULL != sprite) && (NULL != sprite->rows) && (sprite->height <= 32U))
    {
        uint8_t col;

        /* Sprites are authored row-major because that keeps the art readable in
         * gfx_assets.c, but the panel is column-major. Transpose one column at a
         * time into a 32-bit accumulator, then emit it as whole page bytes. The
         * bit tests are a few cycles each, far cheaper than a pixel call per
         * lit pixel. */
        for (col = 0; col < sprite->width; col++)
        {
            uint16_t rowMask = (uint16_t)1U << ((sprite->width - 1U) - col);
            uint32_t column = 0;
            uint8_t row;

            for (row = 0; row < sprite->height; row++)
            {
                if (0U != (sprite->rows[row] & rowMask))
                {
                    column |= ((uint32_t)1UL << row);
                }
            }

            if (0UL != column)
            {
                int16_t cx = x + (int16_t)col;
                int16_t topY = y;

                /* Emit the column in page-sized slices. The first slice may
                 * start part way down a page, so it is shifted into place. */
                while (0UL != column)
                {
                    uint8_t shift = (uint8_t)((uint16_t)topY & 7U);
                    uint8_t page = (uint8_t)((uint16_t)topY >> 3);
                    uint8_t chunk = (uint8_t)(column & 0xFFUL);

                    if (0U != shift)
                    {
                        /* Straddles two pages: low part here, rest next loop. */
                        SSD1306_MaskApply(page, cx, (uint8_t)(chunk << shift),
                                          SSD1306_PIXEL_SET);
                        column >>= (8U - shift);
                        topY += (int16_t)(8U - shift);
                    }
                    else
                    {
                        SSD1306_MaskApply(page, cx, chunk, SSD1306_PIXEL_SET);
                        column >>= 8;
                        topY += 8;
                    }

                    if (topY >= (int16_t)SSD1306_HEIGHT)
                    {
                        break;
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

        /* The font is already stored column-major with bit 0 at the top, which
         * is the panel's own layout. Each glyph column is therefore one shift
         * and one or two byte writes: no per-pixel work at all. */
        uint8_t shift = (uint8_t)((uint16_t)y & 7U);
        uint8_t page = (uint8_t)((uint16_t)y >> 3);

        while ('\0' != *p)
        {
            const uint8_t *glyph = &font5x7[(uint16_t)SSD1306_GlyphIndex(*p) * FONT_WIDTH];
            uint8_t col;

            for (col = 0; col < FONT_WIDTH; col++)
            {
                uint8_t bits = glyph[col];

                if (0U != bits)
                {
                    SSD1306_MaskApply(page, cursor + (int16_t)col,
                                      (uint8_t)(bits << shift), SSD1306_PIXEL_SET);

                    /* A 7 pixel glyph at a non-zero offset spills into the next
                     * page. Guarded so a page-aligned row skips the write. */
                    if (0U != shift)
                    {
                        SSD1306_MaskApply(page + 1U, cursor + (int16_t)col,
                                          (uint8_t)(bits >> (8U - shift)),
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
