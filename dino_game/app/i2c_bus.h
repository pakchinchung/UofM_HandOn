/*
 * @file i2c_bus.h
 *
 * @brief Blocking, polled SERCOM0 I2C host driver with a per-transfer bus
 *        speed. SDA = PA00 (PAD0), SCL = PA01 (PAD1), mux C.
 *
 * Each byte is polled on INTFLAG.MB/SB with a millis() based timeout so a
 * stuck bus cannot hang the application.
 *
 * Every device on the bus passes its own maximum SCL frequency, and the baud
 * register is only rewritten when the requested speed differs from the one
 * currently programmed. That keeps a fast device (OLED) from forcing a slow
 * device on the same bus to run out of spec.
 *
 * Ceiling note: SCL is derived from the 24 MHz SERCOM0 core GCLK, so
 * f_SCL = f_GCLK / (10 + 2 * BAUD + f_GCLK * T_rise), same form as the AVR
 * TWI. The ceiling is f_GCLK / 10 = 2.4 MHz, well above Fm+.
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** @brief SERCOM0 core clock (GCLK0, OSCHF 24 MHz). */
#define I2C_GCLK_HZ (24000000UL)

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Standard mode, 100 kHz. */
#define I2C_SPEED_STANDARD (100000UL)
/** @brief Fast mode, 400 kHz.  */
#define I2C_SPEED_FAST     (400000UL)
/** @brief Fast mode plus, 1 MHz. Needs CTRLA.SPEED = Fm+. */
#define I2C_SPEED_FAST_PLUS (1000000UL)

/**
 * @brief Hard SCL ceiling for the SERCOM core clock. The divisor cannot go
 *        below 10, so no BAUD value reaches beyond f_GCLK / 10.
 */
#define I2C_SPEED_CEILING (I2C_GCLK_HZ / 10UL)

/** @brief Requests above this switch CTRLA.SPEED to Fast-mode-plus. */
#define I2C_FMPEN_THRESHOLD (400000UL)

/** @brief Bus rise time assumption used for the baud calculation, in ns. */
#define I2C_TRISE_NS (100UL)

/* Transfer timeouts are derived from the byte count and the programmed SCL
 * frequency rather than being a single constant. A full 128x64 frame is 1026
 * bytes, which at 100 kHz takes about 92 ms, so any fixed value small enough to
 * be useful on a 2 byte register write is far too short for a frame push. */

/** @brief Fixed slack added on top of the calculated transfer time. */
#define I2C_TIMEOUT_MARGIN_MS (10UL)
/** @brief Upper bound, so a genuinely stuck bus cannot spin indefinitely. */
#define I2C_TIMEOUT_MAX_MS    (600UL)

/**
 * @brief Clocks, pins and SERCOM0 I2C host setup. Call once after
 *        MILLIS_Initialize(); the bus starts at I2C_SPEED_STANDARD.
 * @param None.
 * @return None.
 */
void I2C_Initialize(void);

/**
 * @brief Returns the BAUD value that would be programmed for a frequency.
 *        Exposed so callers can report the real bus setup rather than the
 *        request, since BAUD is an integer and clamps at 0.
 * @param fScl - Desired SCL frequency in hertz.
 * @return BAUD.BAUD register value.
 */
uint8_t I2C_BaudFor(uint32_t fScl);

/**
 * @brief Returns the SCL frequency that a request actually resolves to.
 * @param fScl - Desired SCL frequency in hertz.
 * @return Achieved frequency in hertz, always <= the request.
 */
uint32_t I2C_SpeedActualFor(uint32_t fScl);

/**
 * @brief Returns the SCL low time for a request, in nanoseconds.
 * @param fScl - Desired SCL frequency in hertz.
 * @return SCL low time in nanoseconds.
 */
uint16_t I2C_SclLowTimeNsFor(uint32_t fScl);

/**
 * @brief Returns the minimum SCL low time the I2C spec allows at a frequency:
 *        4700 ns up to 100 kHz, 1300 ns up to 400 kHz, 500 ns up to 1 MHz.
 * @param fScl - Desired SCL frequency in hertz.
 * @return Minimum permitted SCL low time in nanoseconds.
 */
uint16_t I2C_SclLowMinNsFor(uint32_t fScl);

/**
 * @brief Reports whether a request produces bus timing inside the I2C spec.
 *        A rung can work on the bench and still fail this; prefer one that
 *        passes so behaviour does not depend on temperature or a swapped part.
 * @param fScl - Desired SCL frequency in hertz.
 * @retval true if both the SCL low time and the frequency band are legal
 * @retval false otherwise
 */
bool I2C_TimingIsInSpec(uint32_t fScl);

/**
 * @brief Programs the SERCOM0 baud register for the requested SCL frequency.
 *        Waits for the bus to go idle first. No-op if already at that speed.
 * @param fScl - Desired SCL frequency in hertz.
 * @return None.
 */
void I2C_SpeedSet(uint32_t fScl);

/**
 * @brief Returns the SCL frequency most recently requested via I2C_SpeedSet().
 * @param None.
 * @return Frequency in hertz, or 0 if never set.
 */
uint32_t I2C_SpeedGet(void);

/**
 * @brief Blocking write. Sets the bus speed, starts the transfer and waits.
 * @param fScl - SCL frequency to use for this transfer, in hertz.
 * @param address - 7-bit client address.
 * @param data - Buffer to transmit.
 * @param length - Number of bytes to transmit.
 * @retval true on success
 * @retval false on NACK, bus error or timeout
 */
bool I2C_Write(uint32_t fScl, uint16_t address, uint8_t *data, size_t length);

/**
 * @brief Blocking write-then-read with a repeated start. Used for register
 *        reads (write the register index, read the value back).
 * @param fScl - SCL frequency to use for this transfer, in hertz.
 * @param address - 7-bit client address.
 * @param writeData - Buffer to transmit.
 * @param writeLength - Number of bytes to transmit.
 * @param readData - Buffer to receive into.
 * @param readLength - Number of bytes to receive.
 * @retval true on success
 * @retval false on NACK, bus error or timeout
 */
bool I2C_WriteRead(uint32_t fScl, uint16_t address,
                   uint8_t *writeData, size_t writeLength,
                   uint8_t *readData, size_t readLength);

/** @brief I2C_LastErrorGet() codes. Numbering kept from the AVR build. */
#define I2C_ERROR_NONE      (0x00U) /**< Transfer completed */
#define I2C_ERROR_ADDR_NACK (0x01U) /**< Client did not ACK its address */
#define I2C_ERROR_DATA_NACK (0x02U) /**< Client NACKed a data byte */
#define I2C_ERROR_BUS_COLLISION (0x03U) /**< Bus error or arbitration lost */
#define I2C_ERROR_TIMEOUT   (0x10U) /**< MB/SB never set in time */
#define I2C_ERROR_BUS_BUSY  (0x11U) /**< Bus never became idle */

/**
 * @brief Returns the outcome of the most recent transfer.
 *
 * @param None.
 * @return Error code for the last transfer, 0 if it succeeded.
 */
uint8_t I2C_LastErrorGet(void);

/**
 * @brief Walks the standard 0x08..0x77 address range and reports what answers.
 * @param fScl - SCL frequency to scan at, in hertz. Use a conservative value.
 * @param found - Buffer that receives the addresses that acknowledged.
 * @param maxFound - Capacity of @p found.
 * @return Number of addresses written to @p found.
 */
uint8_t I2C_BusScan(uint32_t fScl, uint8_t *found, uint8_t maxFound);

/**
 * @brief Probes for a client by attempting a zero-length addressed write.
 * @param fScl - SCL frequency to use, in hertz.
 * @param address - 7-bit client address.
 * @retval true if the client acknowledged its address
 * @retval false otherwise
 */
bool I2C_DeviceIsPresent(uint32_t fScl, uint16_t address);

#ifdef __cplusplus
}
#endif

#endif /* I2C_BUS_H */
