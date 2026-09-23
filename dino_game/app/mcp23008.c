/*
 * @file mcp23008.c
 *
 * @brief MCP23008 I2C GPIO expander, used here as a debounced button input.
 */

#include "mcp23008.h"
#include "millis.h"

/* Register indices, valid while IOCON.BANK is 0 (the reset default). */
#define MCP23008_REG_IODIR (0x00U)
#define MCP23008_REG_IPOL  (0x01U)
#define MCP23008_REG_IOCON (0x05U)
#define MCP23008_REG_GPPU  (0x06U)
#define MCP23008_REG_GPIO  (0x09U)

static uint8_t mcpRawGpio = 0xFFU;
static uint8_t mcpDebounced = 0;
static uint8_t mcpHistory[3] = { 0, 0, 0 };
static uint8_t mcpHistoryIndex = 0;
static uint8_t mcpPressedLatch = 0;
static uint8_t mcpReleasedLatch = 0;
static uint32_t mcpLastPoll = 0;
static bool mcpOnline = false;

/* Runtime settable so this device keeps its own bus speed independently of the
 * display, which is the whole point of the per-transfer speed in i2c_bus. */
static uint32_t mcpSpeed = MCP23008_I2C_SPEED_DEFAULT;

static bool MCP23008_RegWrite(uint8_t reg, uint8_t value)
{
    uint8_t packet[2] = { reg, value };

    return I2C_Write(mcpSpeed, MCP23008_I2C_ADDR, packet, sizeof(packet));
}

static bool MCP23008_RegRead(uint8_t reg, uint8_t *value)
{
    uint8_t index = reg;

    return I2C_WriteRead(mcpSpeed, MCP23008_I2C_ADDR,
                         &index, 1U, value, 1U);
}

bool MCP23008_Initialize(void)
{
    bool ok;

    /* Sequential addressing on, INT open-drain off, INT active low. The
     * interrupt outputs are unused here; the buttons are polled. */
    ok = MCP23008_RegWrite(MCP23008_REG_IOCON, 0x00U);
    /* Every pin an input. Nothing on this part drives anything. */
    ok = ok && MCP23008_RegWrite(MCP23008_REG_IODIR, 0xFFU);
    /* No input inversion in hardware; the driver inverts in software instead. */
    ok = ok && MCP23008_RegWrite(MCP23008_REG_IPOL, 0x00U);
    /* Internal pull-ups off, the board already has external pull-ups. */
    ok = ok && MCP23008_RegWrite(MCP23008_REG_GPPU, 0x00U);

    mcpOnline = ok;
    mcpDebounced = 0;
    mcpPressedLatch = 0;
    mcpReleasedLatch = 0;
    mcpHistoryIndex = 0;
    mcpLastPoll = millis();

    if (ok)
    {
        uint8_t raw = 0xFFU;

        if (MCP23008_RegRead(MCP23008_REG_GPIO, &raw))
        {
            uint8_t i;

            mcpRawGpio = raw;

            /* Prime the filter with the current state so a button already held
             * at power-up does not register as a fresh press. */
            for (i = 0; i < (uint8_t)(sizeof(mcpHistory)); i++)
            {
                mcpHistory[i] = (uint8_t)(~raw) & BTN_ALL;
            }

            mcpDebounced = (uint8_t)(~raw) & BTN_ALL;
        }
        else
        {
            mcpOnline = false;
            ok = false;
        }
    }

    return ok;
}

void MCP23008_Tasks(void)
{
    uint32_t now = millis();

    if ((now - mcpLastPoll) >= MCP23008_POLL_MS)
    {
        uint8_t raw = 0xFFU;

        mcpLastPoll = now;

        if (MCP23008_RegRead(MCP23008_REG_GPIO, &raw))
        {
            uint8_t sample;
            uint8_t allHigh;
            uint8_t anyHigh;
            uint8_t previous = mcpDebounced;
            uint8_t changed;

            mcpOnline = true;
            mcpRawGpio = raw;

            /* Buttons pull to ground, so invert to get 1 = pressed. */
            sample = (uint8_t)(~raw) & BTN_ALL;

            mcpHistory[mcpHistoryIndex] = sample;
            mcpHistoryIndex++;
            if (mcpHistoryIndex >= (uint8_t)(sizeof(mcpHistory)))
            {
                mcpHistoryIndex = 0;
            }

            /* A bit only turns on after three consecutive pressed samples and
             * only turns off after three consecutive released samples, which
             * gives roughly 15 ms of hysteresis at a 5 ms poll. */
            allHigh = mcpHistory[0] & mcpHistory[1] & mcpHistory[2];
            anyHigh = mcpHistory[0] | mcpHistory[1] | mcpHistory[2];
            mcpDebounced = (uint8_t)((mcpDebounced & anyHigh) | allHigh);

            changed = (uint8_t)(mcpDebounced ^ previous);
            mcpPressedLatch |= (uint8_t)(changed & mcpDebounced);
            mcpReleasedLatch |= (uint8_t)(changed & previous);
        }
        else
        {
            mcpOnline = false;
        }
    }
}

void MCP23008_SpeedSet(uint32_t fScl)
{
    mcpSpeed = fScl;
}

uint8_t MCP23008_Held(void)
{
    return mcpDebounced;
}

uint8_t MCP23008_Pressed(void)
{
    uint8_t edges = mcpPressedLatch;

    mcpPressedLatch = 0;

    return edges;
}

uint8_t MCP23008_Released(void)
{
    uint8_t edges = mcpReleasedLatch;

    mcpReleasedLatch = 0;

    return edges;
}

uint8_t MCP23008_RawGet(void)
{
    return mcpRawGpio;
}

bool MCP23008_IsOnline(void)
{
    return mcpOnline;
}
