#include <stdint.h>

// Polling-based I2C driver because not going to be do anything while sending
// I2C commands Driver for I2C master

typedef enum {
    I2C_RESULT_OK,
    I2C_RESULT_ERROR_START,
    I2C_RESULT_ERROR_TX,
    I2C_RESULT_ERROR_RX,
    I2C_RESULT_ERROR_STOP,
    I2C_RESULT_ERROR_TIMEOUT
} e__i2c_result;

void i2c_init(void);
void i2c_set_slave_address(uint8_t addr);
e__i2c_result i2c_write(const uint8_t *addr, uint8_t addr_size,
                        const uint8_t *data, uint8_t data_size);
e__i2c_result i2c_read(const uint8_t *addr, uint8_t addr_size, uint8_t *data,
                       uint8_t data_size);

// Convenient wrapper functions
e__i2c_result i2c_read_addr8_data8(uint8_t addr, uint8_t *data);
e__i2c_result i2c_read_addr8_data16(uint8_t addr, uint16_t *data);
e__i2c_result i2c_read_addr8_data32(uint8_t addr, uint32_t *data);
e__i2c_result i2c_write_addr8_data8(uint8_t addr, uint8_t data);
