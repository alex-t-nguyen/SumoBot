#include "drivers/i2c.h"
#include "common/assert_handler.h"
#include "common/trace.h"
#include "drivers/io.h"
#include <msp430.h>
#include <stdbool.h>
#include <stdint.h>

#define RETRY_COUNT (UINT16_MAX) // Needs to be large enough to give hardware time to actually set registers (uint8_t max == 255, not long enough)
static bool initialized = false;

static inline void i2c_set_tx_byte(uint8_t data) {
    UCB1TXBUF = data; // Set the data to be transmitted
}

static uint8_t i2c_get_rx_byte() { return UCB1RXBUF; }

static e__i2c_result i2c_stop_transfer() {
    uint16_t retries = RETRY_COUNT;
    // Send stop condition
    UCB1CTL1 |= UCTXSTP;
    // UCTXSTP bit in UCB1CTL1 is automatically cleared after stop condition is
    // generated
    while (UCB1CTL1 & UCTXSTP) {
        retries--;
        if (retries == 0) {
            TRACE("I2C stop condition timeout");
            return I2C_RESULT_ERROR_TIMEOUT;
        }
    }
    return I2C_RESULT_OK;
}

static inline void i2c_send_start_condition(void) {
    // This also sends the slave device address after the start condition
    // The UCTXSTT flag is cleared after the complete address is sent
    UCB1CTL1 |= UCTXSTT; // Send start condition if in master mode
}

static e__i2c_result i2c_wait_for_start_condition(void) {
    uint16_t retries = RETRY_COUNT;
    // UCTXSTT is automatically cleared after START condition and address
    // information is transmitted
    while (UCB1CTL1 & UCTXSTT) {
        retries--;
        if (retries == 0) {
            TRACE("I2C start condition timeout");
            return I2C_RESULT_ERROR_TIMEOUT;
        }
    }
    // If receive NACK from slave, return error, otherwise ok
    return (UCB1IFG & UCNACKIFG) ? I2C_RESULT_ERROR_START : I2C_RESULT_OK;
}

/**
 * After transmitting data on I2C SDA line, wait for UCTXIFG flag to be set.
 * This indicates that the TXBUF is empty and that the data has been
 * transferred. If receive NACK from slave device then return error, otherwise
 * return ok.
 */
static e__i2c_result i2c_wait_for_tx_byte(void) {
    uint16_t retries = RETRY_COUNT;
    // USCI transmit interrupt flag. UCTXIFG is set when UCBxTXBUF is empty
    while (!(UCB1IFG & UCTXIFG)) {
        retries--;
        if (retries == 0) {
            TRACE("I2C TX send timeout");
            return I2C_RESULT_ERROR_TIMEOUT;
        }
    }
    // If receive NACK from slave, return error, otherwise ok
    return (UCB1IFG & UCNACKIFG) ? I2C_RESULT_ERROR_TX : I2C_RESULT_OK;
}

/**
 * After transmitting data on I2C SDA line, wait for UCTXIFG flag to be set.
 * This indicates that the TXBUF is empty and that the data has been
 * transferred. If receive NACK from slave device then return error, otherwise
 * return ok.
 */
static e__i2c_result i2c_wait_for_rx_byte(void) {
    uint16_t retries = RETRY_COUNT;
    // USCI transmit interrupt flag. UCRXIFG is set when UCBxRXBUF is empty
    while (!(UCB1IFG & UCRXIFG)) {
        retries--;
        if (retries == 0) {
            TRACE("I2C RX receive timeout");
            return I2C_RESULT_ERROR_TIMEOUT;
        }
    }
    // If receive NACK from slave, return error, otherwise ok
    return (UCB1IFG & UCNACKIFG) ? I2C_RESULT_ERROR_RX : I2C_RESULT_OK;
}

static e__i2c_result i2c_send_addr(const uint8_t *addr, uint8_t addr_size) {
    UCB1CTL1 |= UCTR;           // Set to transmitter mode
    i2c_send_start_condition(); // Send start condition (Start condition
                                // actually generated only when the bus is not
                                // busy, this bit is cleared when start
                                // condition is actually sent)

    // Send I2C frame for 1st address byte
    i2c_set_tx_byte(addr[0]);
    e__i2c_result result = i2c_wait_for_start_condition();
    if (result != I2C_RESULT_OK)
        return result;
    result = i2c_wait_for_tx_byte();
    if (result != I2C_RESULT_OK)
        return result;

    // If address > 1 byte long
    // then send the remaining address bytes here
    for (uint8_t i = 1; i < addr_size; i++) {
        i2c_set_tx_byte(addr[i]);
        result = i2c_wait_for_tx_byte();
        if (result != I2C_RESULT_OK)
            return result;
    }

    return result;
}

static e__i2c_result i2c_start_tx_transfer(const uint8_t *addr,
                                           uint8_t addr_size) {
    return i2c_send_addr(addr, addr_size);
}

static e__i2c_result i2c_start_rx_transfer(const uint8_t *addr,
                                           uint8_t addr_size) {
    // Send device address and register address
    e__i2c_result result = i2c_send_addr(addr, addr_size);
    if (result != I2C_RESULT_OK)
        return result;

    // Configure master as receiver now to read data from slave device on SDA
    UCB1CTL1 &= ~UCTR;          // Set to receiver mode
    i2c_send_start_condition(); // Send start condition and slave device address
    result = i2c_wait_for_start_condition(); // Wait for start condition to
                                             // actually be sent
    return result;
}

/**
 * Initialize I2C pins of MSP430F5529
 */
void i2c_init(void) {
    ASSERT(!initialized);
    static const struct io_config i2c_config = {.io_sel = IO_SEL_ALT1,
                                                .io_dir = IO_DIR_OUTPUT,
                                                .io_ren = IO_REN_ENABLE,
                                                .io_out = IO_OUT_HIGH};
    struct io_config current_i2c_config;
    io_get_current_config(I2C_SDA, &current_i2c_config);
    ASSERT(io_config_compare(&i2c_config, &current_i2c_config));
    io_get_current_config(I2C_SCL, &current_i2c_config);
    ASSERT(io_config_compare(&i2c_config, &current_i2c_config));

    // Must set reset while configuring
    UCB1CTL1 |= UCSWRST;

    // UCB1CTLW0 == UCB1CTL0 [15:8] + UCB1CTL1 [7:0]
    // Single I2C master, synchronous mode, I2C mode
    UCB1CTL0 |= UCMST | UCMODE_3 | UCSYNC;
    // Select SMCLK as I2C clock source, set to transmitter mode
    UCB1CTL1 |= UCSSEL_2 | UCTR;

    // Divide SMCLK down to I2C speed (16MHz/160 = 100kHz)
    UCB1BR0 = (uint8_t)160; // Upper 8 bits of I2C clock source prescalar
    UCB1BR1 = (uint8_t)0;   // Lower 8 bts of I2C clock source prescalar

    UCB1CTL1 &= ~UCSWRST;
    initialized = true;
}

/**
 * Sets the register with the device address of the I2C device
 * to be communicated with. (This is the slave device address not the register
 * address) Register: UCBxI2CSA
 */
void i2c_set_slave_address(uint8_t addr) { UCB1I2CSA = addr; }

/**
 * Writes the I2C data to the specified register address
 */
e__i2c_result i2c_write(const uint8_t *addr, uint8_t addr_size,
                        const uint8_t *data, uint8_t data_size) {
    // Not available in msp430f5529.h file for some reason
    // UCB1TBCNT = data_size; // Set the number of bytes to transmit

    // Send device address and regiser address
    e__i2c_result result = i2c_start_tx_transfer(addr, addr_size);
    if (result != I2C_RESULT_OK)
        return result;

    // Send data bytes, starting from MSB to LSB
    for (uint16_t i = 0; i < data_size; i++) {
        i2c_set_tx_byte(data[i]);
        result = i2c_wait_for_tx_byte();
        if (result != I2C_RESULT_OK)
            return result;
    }
    return result;
}

/**
 * Reads the I2C data from the specified register address
 */
e__i2c_result i2c_read(const uint8_t *addr, uint8_t addr_size, uint8_t *data,
                       uint8_t data_size) {
    // Send device address and set master to receiver mode
    e__i2c_result result = i2c_start_rx_transfer(addr, addr_size);
    if (result != I2C_RESULT_OK)
        return result;

    // Read MSB first to LSB last
    for (uint8_t i = data_size - 1; i >= 1; i--) {
        result = i2c_wait_for_rx_byte(); // Wait until byte is received from
                                         // slave device and put into RXBUF
        if (result != I2C_RESULT_OK)
            return result;
        data[i] = i2c_get_rx_byte();
    }

    // Send stop condition before waiting to read last byte (required by MSP430)
    result = i2c_stop_transfer();
    if (result != I2C_RESULT_OK)
        return result;

    // Wait to receive data byte sent from slave device
    result = i2c_wait_for_rx_byte();
    if (result != I2C_RESULT_OK)
        return result;

    data[0] = i2c_get_rx_byte(); // Store received data byte from slave into
                                 // data buffer
    return result;
}

// Convenient wrapper functions to call the main i2c_write() and i2c_read()
// functions
/**
 * Reads 1 byte of data at register address of size 1 byte
 */
e__i2c_result i2c_read_addr8_data8(uint8_t addr, uint8_t *data) {
    e__i2c_result result = i2c_read(&addr, 1, data, 1);
    return result;
}

/**
 * Reads 2 bytes of data at register address of size 1 byte
 */
e__i2c_result i2c_read_addr8_data16(uint8_t addr, uint16_t *data) {
    e__i2c_result result = i2c_read(&addr, 1, (uint8_t *)data, 2);
    return result;
}

/**
 * Reads 4 byte of data at register address of size 1 byte
 */
e__i2c_result i2c_read_addr8_data32(uint8_t addr, uint32_t *data) {
    e__i2c_result result = i2c_read(&addr, 1, (uint8_t *)data, 4);
    return result;
}

/**
 * Writes 1 byte of data to register address of size 1 byte
 */
e__i2c_result i2c_write_addr8_data8(uint8_t addr, uint8_t data) {
    e__i2c_result result = i2c_write(&addr, 1, &data, 1);
    return result;
}
