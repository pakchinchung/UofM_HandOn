/*
 * @file i2c_bus.c
 *
 * @brief Blocking, polled SERCOM0 I2C host driver with a per-transfer
 *        bus speed. SDA = PA00 (PAD0), SCL = PA01 (PAD1), mux C.
 */

#include "i2c_bus.h"
#include "millis.h"
#include "device.h"

/* Cycles of I2C_GCLK_HZ that fit in the assumed bus rise time, rounded up so the
 * resulting SCL never exceeds what the caller asked for. */
#define I2C_TRISE_CYCLES (((I2C_GCLK_HZ / 1000000UL) * I2C_TRISE_NS + 999UL) / 1000UL)

static uint32_t i2cCurrentSpeed = 0;
static uint8_t i2cLastError = 0;

/**
 * @brief Converts an SCL frequency into a SERCOM0 BAUD.BAUD value.
 *
 * Same form as the SERCOM I2CM data sheet expression with BAUDLOW = 0: f_SCL = I2C_GCLK_HZ / (10 + 2 * BAUD + I2C_GCLK_HZ * T_rise),
 * so BAUD = (I2C_GCLK_HZ / f_SCL - 10 - T_rise_cycles) / 2. The division rounds up
 * so the programmed frequency lands at or below the requested one, and the
 * result is clamped at 0 for frequencies I2C_GCLK_HZ cannot reach.
 */
uint8_t I2C_BaudFor(uint32_t fScl)
{
    uint8_t mbaud = 0;

    if (0UL != fScl)
    {
        uint32_t total = I2C_GCLK_HZ / fScl;
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
    /* Invert the data sheet expression with the BAUD that will really be used. */
    uint32_t divisor = 10UL + (2UL * (uint32_t)I2C_BaudFor(fScl)) + I2C_TRISE_CYCLES;

    return I2C_GCLK_HZ / divisor;
}

uint16_t I2C_SclLowTimeNsFor(uint32_t fScl)
{
    /* SCL is held low for 5 + BAUD peripheral clock cycles.
     *
     * Scaled by 1e6 and divided by kHz rather than by 1e9 and divided by Hz:
     * at BAUD 114 the latter computes 119 * 1e9 = 1.19e11, which does not fit
     * in 32 bits and silently wraps. */
    uint32_t cycles = 5UL + (uint32_t)I2C_BaudFor(fScl);
    uint32_t ns = (cycles * 1000000UL) / (I2C_GCLK_HZ / 1000UL);

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
    /* The SERCOM is only specified up to at Fast-mode Plus, so anything beyond
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

#define I2C_REGS (&SERCOM0_REGS->I2CM)

/* Peripheral function C for SERCOM0 on PA00/PA01 (MUX_PA00C_SERCOM0_PAD0). */
#define I2C_PMUX_FUNC_C (0x02U)

/* CTRLB.CMD values. */
#define I2C_CMD_REPEATED_START (1U)
#define I2C_CMD_READ_BYTE      (2U)
#define I2C_CMD_STOP           (3U)

static void I2C_SyncWait(void)
{
    while (0U != (I2C_REGS->SERCOM_SYNCBUSY & SERCOM_I2CM_SYNCBUSY_SYSOP_Msk))
    {
        ;
    }
}

static void I2C_Command(uint32_t cmd)
{
    I2C_REGS->SERCOM_CTRLB = (I2C_REGS->SERCOM_CTRLB & ~SERCOM_I2CM_CTRLB_CMD_Msk) |
                             SERCOM_I2CM_CTRLB_CMD(cmd);
    I2C_SyncWait();
}

/* SERCOM powers up with BUSSTATE UNKNOWN and will not start a transfer until
 * it is forced to IDLE. Needed after every enable. */
static void I2C_ForceIdle(void)
{
    I2C_REGS->SERCOM_STATUS = SERCOM_I2CM_STATUS_BUSSTATE(SERCOM_I2CM_STATUS_BUSSTATE_IDLE_Val);
    I2C_SyncWait();
}

static void I2C_Enable(bool enable)
{
    if (enable)
    {
        I2C_REGS->SERCOM_CTRLA |= SERCOM_I2CM_CTRLA_ENABLE_Msk;
    }
    else
    {
        I2C_REGS->SERCOM_CTRLA &= ~SERCOM_I2CM_CTRLA_ENABLE_Msk;
    }

    while (0U != (I2C_REGS->SERCOM_SYNCBUSY & SERCOM_I2CM_SYNCBUSY_ENABLE_Msk))
    {
        ;
    }
}

/* Waits for MB (host on bus, write side) or SB (byte received) with a
 * deadline. Returns false on timeout. */
static bool I2C_WaitFlag(uint32_t start, uint32_t timeoutMs)
{
    while (0U == (I2C_REGS->SERCOM_INTFLAG &
                  (SERCOM_I2CM_INTFLAG_MB_Msk | SERCOM_I2CM_INTFLAG_SB_Msk)))
    {
        if ((millis() - start) >= timeoutMs)
        {
            return false;
        }
    }

    return true;
}

/* Recovers from a timeout or bus error: drop the transfer, reset the host
 * state machine and force the bus idle so the next call can start cleanly. */
static void I2C_Recover(void)
{
    I2C_Enable(false);
    I2C_Enable(true);
    I2C_ForceIdle();
}

void I2C_Initialize(void)
{
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_SERCOM0_Msk;

    GCLK_REGS->GCLK_PCHCTRL[SERCOM0_GCLK_ID_CORE] = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[SERCOM0_GCLK_ID_CORE] & GCLK_PCHCTRL_CHEN_Msk) == 0U)
    {
        ;
    }

    /* PA00 even -> low nibble, PA01 odd -> high nibble of PMUX[0]. The board
     * has external pull-ups, so no internal pull is enabled. */
    PORT_REGS->GROUP[0].PORT_PMUX[0] = (uint8_t)((I2C_PMUX_FUNC_C << 4) | I2C_PMUX_FUNC_C);
    PORT_REGS->GROUP[0].PORT_PINCFG[0] = PORT_PINCFG_PMUXEN_Msk;
    PORT_REGS->GROUP[0].PORT_PINCFG[1] = PORT_PINCFG_PMUXEN_Msk;

    I2C_REGS->SERCOM_CTRLA = SERCOM_I2CM_CTRLA_SWRST_Msk;
    while (0U != (I2C_REGS->SERCOM_SYNCBUSY & SERCOM_I2CM_SYNCBUSY_SWRST_Msk))
    {
        ;
    }

    I2C_REGS->SERCOM_CTRLA = SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
                             SERCOM_I2CM_CTRLA_SDAHOLD_75NS;
    /* Smart mode: reading DATA sends the ACK/NACK in ACKACT automatically. */
    I2C_REGS->SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk;
    I2C_SyncWait();

    I2C_REGS->SERCOM_BAUD = SERCOM_I2CM_BAUD_BAUD(I2C_BaudFor(I2C_SPEED_STANDARD));
    i2cCurrentSpeed = I2C_SPEED_STANDARD;

    I2C_Enable(true);
    I2C_ForceIdle();
}

/* Waits for the previous STOP to finish so the host can be reconfigured. */
static bool I2C_WaitIdle(uint32_t timeoutMs)
{
    uint32_t start = millis();

    while ((I2C_REGS->SERCOM_STATUS & SERCOM_I2CM_STATUS_BUSSTATE_Msk) !=
           SERCOM_I2CM_STATUS_BUSSTATE_IDLE)
    {
        if ((millis() - start) >= timeoutMs)
        {
            return false;
        }
    }

    return true;
}

void I2C_SpeedSet(uint32_t fScl)
{
    if (fScl != i2cCurrentSpeed)
    {
        bool wantFmPlus = (fScl > I2C_FMPEN_THRESHOLD);

        if (!I2C_WaitIdle(I2C_TIMEOUT_MAX_MS))
        {
            I2C_Recover();
        }

        /* BAUD and CTRLA.SPEED are enable-protected, so the host is switched
         * off around the change. Disabling drops BUSSTATE to UNKNOWN, hence
         * the forced idle afterwards. */
        I2C_Enable(false);

        I2C_REGS->SERCOM_BAUD = SERCOM_I2CM_BAUD_BAUD(I2C_BaudFor(fScl));

        if (wantFmPlus)
        {
            I2C_REGS->SERCOM_CTRLA |= SERCOM_I2CM_CTRLA_SPEED_FASTPLUS_MODE;
        }
        else
        {
            I2C_REGS->SERCOM_CTRLA &= ~SERCOM_I2CM_CTRLA_SPEED_Msk;
        }

        I2C_Enable(true);
        I2C_ForceIdle();

        i2cCurrentSpeed = fScl;
    }
}

uint32_t I2C_SpeedGet(void)
{
    return i2cCurrentSpeed;
}

uint8_t I2C_LastErrorGet(void)
{
    return i2cLastError;
}

/* Checks the outcome of an address or data phase once MB/SB is set. */
static uint8_t I2C_PhaseError(uint8_t nackCode)
{
    uint16_t status = I2C_REGS->SERCOM_STATUS;
    uint8_t error = I2C_ERROR_NONE;

    if (0U != (status & (SERCOM_I2CM_STATUS_BUSERR_Msk | SERCOM_I2CM_STATUS_ARBLOST_Msk)))
    {
        /* Write-one-to-clear. */
        I2C_REGS->SERCOM_STATUS = SERCOM_I2CM_STATUS_BUSERR_Msk | SERCOM_I2CM_STATUS_ARBLOST_Msk;
        error = I2C_ERROR_BUS_COLLISION;
    }
    else if (0U != (status & SERCOM_I2CM_STATUS_RXNACK_Msk))
    {
        error = nackCode;
    }
    else
    {
        /* Byte acknowledged. */
    }

    return error;
}

/* Sends the address byte and waits for the phase to finish. */
static uint8_t I2C_AddressPhase(uint16_t address, bool read, uint32_t start, uint32_t timeoutMs)
{
    I2C_REGS->SERCOM_ADDR = ((uint32_t)address << 1) | (read ? 1UL : 0UL);
    I2C_SyncWait();

    if (!I2C_WaitFlag(start, timeoutMs))
    {
        return I2C_ERROR_TIMEOUT;
    }

    return I2C_PhaseError(I2C_ERROR_ADDR_NACK);
}

/* One complete transaction: optional write phase, optional repeated start
 * read phase, then STOP. Any failure issues a STOP (or a full recover on a
 * timeout / bus error) so the bus is always left idle. */
static bool I2C_Transfer(uint16_t address,
                         const uint8_t *writeData, size_t writeLength,
                         uint8_t *readData, size_t readLength)
{
    uint32_t timeoutMs = I2C_TimeoutCalc(writeLength + readLength);
    uint32_t start = millis();
    uint8_t error = I2C_ERROR_NONE;
    size_t i;

    if (!I2C_WaitIdle(timeoutMs))
    {
        I2C_Recover();
        i2cLastError = I2C_ERROR_BUS_BUSY;
        return false;
    }

    if ((writeLength > 0U) || (0U == readLength))
    {
        error = I2C_AddressPhase(address, false, start, timeoutMs);

        for (i = 0U; (I2C_ERROR_NONE == error) && (i < writeLength); i++)
        {
            I2C_REGS->SERCOM_DATA = writeData[i];
            I2C_SyncWait();

            error = I2C_WaitFlag(start, timeoutMs) ? I2C_PhaseError(I2C_ERROR_DATA_NACK)
                                                   : I2C_ERROR_TIMEOUT;
        }
    }

    if ((I2C_ERROR_NONE == error) && (readLength > 0U))
    {
        /* ACK every byte but the last. */
        I2C_REGS->SERCOM_CTRLB &= ~SERCOM_I2CM_CTRLB_ACKACT_Msk;
        I2C_SyncWait();

        /* Writing ADDR while owning the bus produces the repeated start. */
        error = I2C_AddressPhase(address, true, start, timeoutMs);

        for (i = 0U; (I2C_ERROR_NONE == error) && (i < readLength); i++)
        {
            if ((i + 1U) == readLength)
            {
                /* NACK the final byte and queue the STOP before reading DATA,
                 * or smart mode would clock in another byte. */
                I2C_REGS->SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_ACKACT_Msk;
                I2C_SyncWait();
                I2C_Command(I2C_CMD_STOP);
                readData[i] = I2C_REGS->SERCOM_DATA;
                I2C_SyncWait();
            }
            else
            {
                /* Smart mode: this read ACKs and starts the next byte. */
                readData[i] = I2C_REGS->SERCOM_DATA;
                I2C_SyncWait();

                if (!I2C_WaitFlag(start, timeoutMs))
                {
                    error = I2C_ERROR_TIMEOUT;
                }
                else if (0U != (I2C_REGS->SERCOM_INTFLAG & SERCOM_I2CM_INTFLAG_ERROR_Msk))
                {
                    error = I2C_PhaseError(I2C_ERROR_DATA_NACK);
                }
                else
                {
                    /* Next byte ready. */
                }
            }
        }
    }

    if ((I2C_ERROR_TIMEOUT == error) || (I2C_ERROR_BUS_COLLISION == error))
    {
        /* State machine is not trustworthy. */
        I2C_Recover();
    }
    else if ((I2C_ERROR_NONE != error) || (0U == readLength))
    {
        /* End of a write, or a NACK: the host still owns the bus. A completed
         * read has already queued its STOP. */
        I2C_Command(I2C_CMD_STOP);
    }
    else
    {
        /* Read finished, STOP already sent. */
    }

    i2cLastError = error;

    return (I2C_ERROR_NONE == error);
}

bool I2C_Write(uint32_t fScl, uint16_t address, uint8_t *data, size_t length)
{
    I2C_SpeedSet(fScl);

    return I2C_Transfer(address, data, length, NULL, 0U);
}

bool I2C_WriteRead(uint32_t fScl, uint16_t address,
                   uint8_t *writeData, size_t writeLength,
                   uint8_t *readData, size_t readLength)
{
    I2C_SpeedSet(fScl);

    return I2C_Transfer(address, writeData, writeLength, readData, readLength);
}

bool I2C_DeviceIsPresent(uint32_t fScl, uint16_t address)
{
    /* Address-only write: SERCOM can issue a STOP straight after the address
     * phase, so unlike the AVR build no dummy data byte is needed. */
    I2C_SpeedSet(fScl);

    (void)I2C_Transfer(address, NULL, 0U, NULL, 0U);

    return (I2C_ERROR_NONE == i2cLastError);
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
