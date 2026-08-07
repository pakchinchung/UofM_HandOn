/*
 * @file i2c_bus.c
 *
 * @brief Blocking wrappers over the MCC TWI0 host driver, with a per-transfer
 *        bus speed.
 */

#include "i2c_bus.h"
#include "millis.h"
#include "mcc_generated_files/system/system.h"

/* Cycles of F_CPU that fit in the assumed bus rise time, rounded up so the
 * resulting SCL never exceeds what the caller asked for. */
#define I2C_TRISE_CYCLES (((F_CPU / 1000000UL) * I2C_TRISE_NS + 999UL) / 1000UL)

static uint32_t i2cCurrentSpeed = 0;
static uint8_t i2cLastError = 0;

/**
 * @brief Converts an SCL frequency into a TWI0.MBAUD value.
 *
 * From the AVR DA data sheet, f_SCL = F_CPU / (10 + 2 * MBAUD + F_CPU * T_rise),
 * so MBAUD = (F_CPU / f_SCL - 10 - T_rise_cycles) / 2. The division rounds up
 * so the programmed frequency lands at or below the requested one, and the
 * result is clamped at 0 for frequencies F_CPU cannot reach.
 */
uint8_t I2C_MBaudFor(uint32_t fScl)
{
    uint8_t mbaud = 0;

    if (0UL != fScl)
    {
        uint32_t total = F_CPU / fScl;
        uint32_t overhead = 10UL + I2C_TRISE_CYCLES;

        if (total > overhead)
        {
            uint32_t value = ((total - overhead) + 1UL) / 2UL;

            mbaud = (value > 255UL) ? 255U : (uint8_t)value;
        }
    }

    return mbaud;
}

uint32_t I2C_SpeedActualFor(uint32_t fScl)
{
    /* Invert the data sheet expression with the MBAUD that will really be used. */
    uint32_t divisor = 10UL + (2UL * (uint32_t)I2C_MBaudFor(fScl)) + I2C_TRISE_CYCLES;

    return F_CPU / divisor;
}

uint16_t I2C_SclLowTimeNsFor(uint32_t fScl)
{
    /* SCL is held low for 5 + MBAUD peripheral clock cycles.
     *
     * Scaled by 1e6 and divided by kHz rather than by 1e9 and divided by Hz:
     * at MBAUD 114 the latter computes 119 * 1e9 = 1.19e11, which does not fit
     * in 32 bits and silently wraps. */
    uint32_t cycles = 5UL + (uint32_t)I2C_MBaudFor(fScl);
    uint32_t ns = (cycles * 1000000UL) / (F_CPU / 1000UL);

    return (ns > 65535UL) ? 65535U : (uint16_t)ns;
}

uint16_t I2C_SclLowMinNsFor(uint32_t fScl)
{
    uint32_t actual = I2C_SpeedActualFor(fScl);
    uint16_t minNs;

    /* Minimum SCL low time per the I2C specification, by speed band. */
    if (actual <= 100000UL)
    {
        minNs = 4700U; /* Standard-mode */
    }
    else if (actual <= 400000UL)
    {
        minNs = 1300U; /* Fast-mode */
    }
    else
    {
        minNs = 500U;  /* Fast-mode Plus */
    }

    return minNs;
}

bool I2C_TimingIsInSpec(uint32_t fScl)
{
    /* The TWI peripheral itself tops out at Fast-mode Plus, so anything beyond
     * 1 MHz is out of spec no matter what the SCL low time works out to. */
    return ((I2C_SpeedActualFor(fScl) <= I2C_SPEED_FAST_PLUS) &&
            (I2C_SclLowTimeNsFor(fScl) >= I2C_SclLowMinNsFor(fScl)));
}

/* Worst-case wall time for a transfer of the given payload, in milliseconds.
 * Nine bit times per byte covers the ACK, and two extra bytes cover the start,
 * the address, the stop and any clock stretching. Doubled for margin. */
static uint32_t I2C_TimeoutCalc(size_t bytes)
{
    uint32_t timeout = I2C_TIMEOUT_MAX_MS;

    if (0UL != i2cCurrentSpeed)
    {
        uint32_t bits = ((uint32_t)bytes + 2UL) * 9UL;

        timeout = (((bits * 1000UL) / i2cCurrentSpeed) * 2UL) + I2C_TIMEOUT_MARGIN_MS;

        if (timeout > I2C_TIMEOUT_MAX_MS)
        {
            timeout = I2C_TIMEOUT_MAX_MS;
        }
    }

    return timeout;
}

/* Spins until the TWI0 driver reports idle, or the timeout expires.
 *
 * On a timeout the driver is left mid-transfer with its private busy flag still
 * set, and that flag is only ever cleared from the TWI0 interrupt. There is no
 * public call that resets it, so the only honest recovery is to give the
 * in-flight transfer the rest of its time to drain. Getting the timeout right in
 * the first place is what actually keeps the bus healthy. */
static bool I2C_WaitIdle(uint32_t timeoutMs)
{
    uint32_t start = millis();
    bool idle = true;

    while (TWI0_IsBusy())
    {
        if ((millis() - start) >= timeoutMs)
        {
            idle = false;
            break;
        }
    }

    return idle;
}

void I2C_SpeedSet(uint32_t fScl)
{
    if (fScl != i2cCurrentSpeed)
    {
        bool wantFmPlus = (fScl > I2C_FMPEN_THRESHOLD);
        bool haveFmPlus = (0U != (TWI0.CTRLA & TWI_FMPEN_bm));

        /* MBAUD must only be rewritten while the master is idle. Whatever is in
         * flight belongs to the previous device, so allow a full-size transfer
         * at the old speed to finish before switching. */
        (void)I2C_WaitIdle(I2C_TIMEOUT_MAX_MS);

        TWI0.MBAUD = I2C_MBaudFor(fScl);

        /* FMPEN changes the pad slew rate, and CTRLA is only safe to touch with
         * the master disabled. Disabling it drops BUSSTATE back to UNKNOWN, so
         * the bus has to be forced idle again afterwards or no transfer will
         * ever start. Read-modify-write to preserve SDAHOLD and INPUTLVL. */
        if (wantFmPlus != haveFmPlus)
        {
            TWI0.MCTRLA &= (uint8_t)~TWI_ENABLE_bm;

            if (wantFmPlus)
            {
                TWI0.CTRLA |= TWI_FMPEN_bm;
            }
            else
            {
                TWI0.CTRLA &= (uint8_t)~TWI_FMPEN_bm;
            }

            TWI0.MCTRLA |= TWI_ENABLE_bm;
            TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;
        }

        i2cCurrentSpeed = fScl;
    }
}

uint32_t I2C_SpeedGet(void)
{
    return i2cCurrentSpeed;
}

/* Shared tail for the blocking wrappers: wait for the driver, then latch the
 * outcome so it survives TWI0_ErrorGet()'s clear-on-read. */
static bool I2C_TransferFinish(bool started, size_t bytes)
{
    bool success = false;

    if (!started)
    {
        i2cLastError = I2C_ERROR_BUS_BUSY;
    }
    else if (!I2C_WaitIdle(I2C_TimeoutCalc(bytes)))
    {
        i2cLastError = I2C_ERROR_TIMEOUT;
    }
    else
    {
        i2cLastError = (uint8_t)TWI0_ErrorGet();
        success = (I2C_ERROR_NONE == (i2c_host_error_t)i2cLastError);
    }

    return success;
}

uint8_t I2C_LastErrorGet(void)
{
    return i2cLastError;
}

bool I2C_Write(uint32_t fScl, uint16_t address, uint8_t *data, size_t length)
{
    I2C_SpeedSet(fScl);

    return I2C_TransferFinish(TWI0_Write(address, data, length), length);
}

bool I2C_WriteRead(uint32_t fScl, uint16_t address,
                   uint8_t *writeData, size_t writeLength,
                   uint8_t *readData, size_t readLength)
{
    I2C_SpeedSet(fScl);

    return I2C_TransferFinish(
        TWI0_WriteRead(address, writeData, writeLength, readData, readLength),
        writeLength + readLength);
}

bool I2C_DeviceIsPresent(uint32_t fScl, uint16_t address)
{
    uint8_t probe = 0x00U;

    /* Probed with a one byte write rather than a read. Many SSD1306 breakouts
     * strap R/W# low and never acknowledge a read, so a read based scan can
     * miss a panel that writes perfectly well. The MCC driver also needs a data
     * phase to run a transfer to completion, so a zero-length probe is out.
     *
     * The single 0x00 byte is inert for both devices here: for the SSD1306 it is
     * a command control byte with no command following it, and for the MCP23008
     * it only moves the register address pointer to IODIR without writing it.
     * Only the address ACK is inspected, so a data NACK still counts as present. */
    I2C_SpeedSet(fScl);

    (void)I2C_TransferFinish(TWI0_Write(address, &probe, 1U), 1U);

    return ((I2C_ERROR_ADDR_NACK != (i2c_host_error_t)i2cLastError) &&
            (I2C_ERROR_TIMEOUT != i2cLastError) &&
            (I2C_ERROR_BUS_BUSY != i2cLastError));
}

uint8_t I2C_BusScan(uint32_t fScl, uint8_t *found, uint8_t maxFound)
{
    uint8_t count = 0;

    if (NULL != found)
    {
        uint8_t address;

        /* 0x00..0x07 and 0x78..0x7F are reserved by the I2C spec. */
        for (address = 0x08U; address <= 0x77U; address++)
        {
            if (count >= maxFound)
            {
                break;
            }

            if (I2C_DeviceIsPresent(fScl, address))
            {
                found[count] = address;
                count++;
            }
        }
    }

    return count;
}
