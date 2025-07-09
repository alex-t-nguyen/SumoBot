#include "drivers/vl53l0x.h"
#include "common/assert_handler.h"
#include "common/defines.h"
#include "common/trace.h"
#include "drivers/i2c.h"
#include "drivers/io.h"
#include <stdbool.h>
#include <stdint.h>

// Reads/Writes to this can be considered atomic on MSP430
static volatile e__status_multiple status_multiple =
    STATUS_MULTIPLE_NOT_STARTED;
static volatile e__status_multiple status_multiple_front_middle =
    STATUS_MULTIPLE_NOT_STARTED;
static volatile e__status_multiple status_multiple_front_left =
    STATUS_MULTIPLE_NOT_STARTED;
static volatile e__status_multiple status_multiple_front_right =
    STATUS_MULTIPLE_NOT_STARTED;

static bool initialized = false;
static uint8_t stop_variable =
    0; // Used when starting a measurement (copied from API)

// Struct for VL53L0X that contains the address and corresponding xshut io pin
// of the device
struct vl53l0x_cfg {
    uint8_t addr;
    io_signal_enum xshut_io;
};

static t__vl53l0x_ranges latest_ranges = {
    VL53L0X_OUT_OF_RANGE, VL53L0X_OUT_OF_RANGE, VL53L0X_OUT_OF_RANGE,
    VL53L0X_OUT_OF_RANGE, VL53L0X_OUT_OF_RANGE};

// Array of VL53L0X structs that contain the addr and xshut io pin of each
// VL53L0X device
static const struct vl53l0x_cfg vl53l0x_cfgs[VL53L0X_POS_CNT] = {
    [e_VL53L0X_POS_FRONT] = {.addr = 0x30, .xshut_io = XSHUT_MIDDLE},
#ifdef SUMOBOT
    [e_VL53L0X_POS_FRONT_LEFT] = {.addr = 0x31, .xshut_io = XSHUT_LEFT},
    [e_VL53L0X_POS_FRONT_RIGHT] = {.addr = 0x32, .xshut_io = XSHUT_RIGHT}
#endif
};

/**
 * Set the VL53L0X device into HW standby mode
 *
 * @param pos the index of the VL53L0X device
 * @param en_hw_stby true to set device to HW standby mode, false to turn on
 * device (firmware boot and then enter SW standby)
 */
static void vl53l0x_set_hw_standby(e__vl53l0x_pos pos, bool en_hw_stdby) {
    io_set_out(vl53l0x_cfgs[pos].xshut_io,
               en_hw_stdby ? IO_OUT_LOW : IO_OUT_HIGH);
}
/**
 * Check that the VL53L0X device is powered up by reading back
 * the ID of the device with I2C.
 */
static e__vl53l0x_result check_vl53l0x_is_pwrup(void) {
    e__i2c_result result;
    uint8_t vl53l0x_id;
    result =
        i2c_read_addr8_data8(VL53L0X_REG_IDENTIFICATION_MODEL_ID, &vl53l0x_id);
    if (result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    return (vl53l0x_id == VL53L0X_EXPECTED_DEVICE_ID)
               ? e_VL53L0X_RESULT_OK
               : e_VL53L0X_RESULT_ERROR_PWRUP;
}

/**
 * Configure the slave address of the range sensor by writing a
 * specified value to the slave device's address register with I2C.
 *
 * @note Basically overwriting the default slave address with a new value.
 *
 * @param addr is the new slave device's address to set to
 */
e__vl53l0x_result vl53l0x_config_addr(uint8_t addr) {
    e__i2c_result result;
    // 7 bit slave address written to register containing range sensor's slave
    // address
    result = i2c_write_addr8_data8(VL53L0X_REG_I2C_SLAVE_DEVICE_ADDRESS,
                                   (addr & 0x7F));
    if (result != I2C_RESULT_OK)
        return result;
    return e_VL53L0X_RESULT_OK;
}

/**
 * Enables the VL53L0X device by taking it out how HW standby mode
 * and sets the device address register through I2C.
 */
static e__vl53l0x_result vl53l0x_init_addr(e__vl53l0x_pos pos) {
    e__vl53l0x_result result; // Used to check results of commands with vl53l0x
    // Exit HW standby mode and turn on device
    vl53l0x_set_hw_standby(pos, false);

    // Configure device's address with I2C write
    i2c_set_slave_address(
        VL53L0X_DEFAULT_ADDRESS); // First set MSP430's I2C register with the
                                  // slave device's default address (range
                                  // sensor), so that it can be changed with I2C
                                  // next

    // t_BOOT time is 1.2ms maximum according to VL53L0X datasheet
    BUSY_WAIT_ms(3); // Wait 2ms then to ensure VL53L0X device is booted up out
                     // of HW standby mode

    // Check that the range sensor is booted up before trying to change its
    // slave address by reading back the ID of the range sensor
    result = check_vl53l0x_is_pwrup();
    if (result != e_VL53L0X_RESULT_OK)
        return result;

    // Change range sensor's slave address
    result = vl53l0x_config_addr(vl53l0x_cfgs[pos].addr);
    return result;
}

/**
 * Asserts MSP430 pins used for XSHUT pins of VL53L0X are properly configured.
 */
static void vl53l0x_assert_xshut_pins(void) {
    static const struct io_config xshut_config = {.io_sel = IO_SEL_GPIO,
                                                  .io_dir = IO_DIR_OUTPUT,
                                                  .io_ren = IO_REN_DISABLE,
                                                  .io_out = IO_OUT_LOW};

    struct io_config xshut_front_config, xshut_front_left_config,
        xshut_front_right_config;
    // Get current IO configs for XSHUT pins
    io_get_current_config(XSHUT_MIDDLE, &xshut_front_config);
    io_get_current_config(XSHUT_LEFT, &xshut_front_left_config);
    io_get_current_config(XSHUT_RIGHT, &xshut_front_right_config);

    // Assert that the current xshut io configs match the expected config
    ASSERT(io_config_compare(&xshut_config, &xshut_front_config));
    ASSERT(io_config_compare(&xshut_config, &xshut_front_left_config));
    ASSERT(io_config_compare(&xshut_config, &xshut_front_right_config));
}

/**
 * Initializes all of the sensors by putting them in HW standby mode
 * and then waking them up 1-by-1 as described in AN4846.
 * Sets the device addresses for I2C of all the range sensors.
 */
static e__vl53l0x_result vl53l0x_init_all_dev_addr(void) {
    e__vl53l0x_result result;

    // Default IO config should put all the sensors in HW standby mode (XSHUT
    // pin == LOW)
    vl53l0x_assert_xshut_pins();

    // Wake up each sensor by setting XSHUT output HIGH 1-by-1 and
    // assign the sensor a unique address
    // Function to configure VL53L0X device address register with a value (use
    // I2C write) and repeat for each device'
    result = vl53l0x_init_addr(
        e_VL53L0X_POS_FRONT); // Initialize device address of front range sensor
    if (result != e_VL53L0X_RESULT_OK)
        return result;
#ifdef SUMOBOT
    result = vl53l0x_init_addr(
        e_VL53L0X_POS_FRONT_LEFT); // Initialize device address of front left
                                   // range sensor
    if (result != e_VL53L0X_RESULT_OK)
        return result;
    result = vl53l0x_init_addr(
        e_VL53L0X_POS_FRONT_RIGHT); // Initialize device address of front right
                                    // range sensor
    if (result != e_VL53L0X_RESULT_OK)
        return result;
#endif
    return e_VL53L0X_RESULT_OK;
}

static e__vl53l0x_result vl53l0x_data_init(void) {
    uint8_t vhv_config_scl_sda;
    e__i2c_result i2c_result;
    // Set range sensor supply voltage to be 2.8V instead of 1.8V
    i2c_result = i2c_read_addr8_data8(
        VL53L0X_REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, &vhv_config_scl_sda);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    vhv_config_scl_sda |= 0x01; // Set LSB to 1 to set to 2.8V supply
    i2c_result = i2c_write_addr8_data8(
        VL53L0X_REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, vhv_config_scl_sda);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;

    // Set I2C in range sensor to standard mode
    i2c_write_addr8_data8(0x88, 0x00); // Copied from VL53L0X API

    i2c_result = i2c_write_addr8_data8(0x80, 0x01);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_write_addr8_data8(0xFF, 0x01);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_write_addr8_data8(0x00, 0x00);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_read_addr8_data8(0x91, &stop_variable);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_write_addr8_data8(0x00, 0x01);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_write_addr8_data8(0xFF, 0x00);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_write_addr8_data8(0x80, 0x00);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    return e_VL53L0X_RESULT_OK;
}

/* Wait for strobe value to be set. This is used when we read values
 * from NVM (non volatile memory). */
static e__vl53l0x_result vl53l0x_read_strobe(void) {
    uint8_t strobe = 0;
    if (i2c_write_addr8_data8(0x83, 0x00) != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    e__i2c_result i2c_result = I2C_RESULT_OK;
    do {
        i2c_result = i2c_read_addr8_data8(0x83, &strobe);
    } while (i2c_result == I2C_RESULT_OK && (strobe == 0));
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    if (i2c_write_addr8_data8(0x83, 0x01) != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    return e_VL53L0X_RESULT_OK;
}

/**
 * Gets the spad count, spad type och "good" spad map stored by ST in NVM at
 * their production line.
 * .
 * According to the datasheet, ST runs a calibration (without cover glass) and
 * saves a "good" SPAD map to NVM (non volatile memory). The SPAD array has two
 * types of SPADs: aperture and non-aperture. By default, all of the
 * good SPADs are enabled, but we should only enable a subset of them to get
 * an optimized signal rate. We should also only enable either only aperture
 * or only non-aperture SPADs. The number of SPADs to enable and which type
 * are also saved during the calibration step at ST factory and can be retrieved
 * from NVM.
 */
static e__vl53l0x_result
vl53l0x_get_spad_info_from_nvm(uint8_t *spad_count, uint8_t *spad_type,
                               uint8_t good_spad_map[6]) {
    uint8_t tmp_data8 = 0;
    uint32_t tmp_data32 = 0;
    e__i2c_result i2c_result;
    e__vl53l0x_result vl53l0x_result;

    // Setup to read from NVM
    i2c_result = i2c_write_addr8_data8(0x80, 0x01);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_write_addr8_data8(0xFF, 0x01);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_write_addr8_data8(0x00, 0x00);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_write_addr8_data8(0xFF, 0x06);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;

    if (i2c_read_addr8_data8(0x83, &tmp_data8) != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    if (i2c_write_addr8_data8(0x83, tmp_data8 | 0x04) != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    i2c_result = i2c_write_addr8_data8(0xFF, 0x07);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_write_addr8_data8(0x81, 0x01);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    i2c_result = i2c_write_addr8_data8(0x80, 0x01);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;

    // Get the SPAD count and type
    if (i2c_write_addr8_data8(0x94, 0x6b)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    vl53l0x_result = vl53l0x_read_strobe();
    if (vl53l0x_result != e_VL53L0X_RESULT_OK) {
        return vl53l0x_result;
    }
    if (i2c_read_addr8_data32(0x90, &tmp_data32)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    *spad_count = (tmp_data32 >> 8) & 0x7f;
    *spad_type = (tmp_data32 >> 15) & 0x01;

    /* Since the good SPAD map is already stored in
     * REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0 we can simply read that register
     * instead of doing the below */
#if 0
    /* Get the first part of the SPAD map */
    if (!i2c_write_addr8_data8(0x94, 0x24)) {
        return false;
    }
    if (!vl53l0x_read_strobe()) {
        return false;
    }
    if (!i2c_read_addr8_data32(0x90, &tmp_data32)) {
      return false;
    }
    good_spad_map[0] = (uint8_t)((tmp_data32 >> 24) & 0xFF);
    good_spad_map[1] = (uint8_t)((tmp_data32 >> 16) & 0xFF);
    good_spad_map[2] = (uint8_t)((tmp_data32 >> 8) & 0xFF);
    good_spad_map[3] = (uint8_t)(tmp_data32 & 0xFF);

    /* Get the second part of the SPAD map */
    if (!i2c_write_addr8_data8(0x94, 0x25)) {
        return false;
    }
    if (!vl53l0x_read_strobe()) {
        return false;
    }
    if (!i2c_read_addr8_data32(0x90, &tmp_data32)) {
        return false;
    }
    good_spad_map[4] = (uint8_t)((tmp_data32 >> 24) & 0xFF);
    good_spad_map[5] = (uint8_t)((tmp_data32 >> 16) & 0xFF);

#endif

    // Restore after reading from NVM
    i2c_result = i2c_write_addr8_data8(0x81, 0x00);
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    i2c_result = i2c_write_addr8_data8(0xFF, 0x06);
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    if (i2c_read_addr8_data8(0x83, &tmp_data8)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    if (i2c_write_addr8_data8(0x83, tmp_data8 & 0xfb)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    i2c_result = i2c_write_addr8_data8(0xFF, 0x01);
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    i2c_result = i2c_write_addr8_data8(0x00, 0x01);
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    i2c_result =
        i2c_write_addr8_data8(0xFF, 0x00); // <-- go back to default page
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    i2c_result = i2c_write_addr8_data8(0x80, 0x00); // <-- restore default
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    /* When we haven't configured the SPAD map yet, the SPAD map register
     * actually contains the good SPAD map, so we can retrieve it straight from
     * this register instead of reading it from the NVM. */
    const uint8_t reg_global_cfg_addr =
        VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0;
    i2c_result = i2c_read(&reg_global_cfg_addr, 1, good_spad_map, 6);
    if (i2c_result != I2C_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_I2C;
    return e_VL53L0X_RESULT_OK;
}

static e__vl53l0x_result vl53l0x_set_spads_from_nvm(void) {
    uint8_t spad_map[SPAD_MAP_ROW_COUNT] = {0};
    uint8_t good_spad_map[SPAD_MAP_ROW_COUNT] = {0};
    uint8_t spads_enabled_count = 0;
    uint8_t spads_to_enable_count = 0;
    uint8_t spad_type = 0;
    e__i2c_result i2c_result;

    e__vl53l0x_result vl53l0x_result = vl53l0x_get_spad_info_from_nvm(
        &spads_to_enable_count, &spad_type, good_spad_map);
    if (vl53l0x_result != e_VL53L0X_RESULT_OK) {
        return vl53l0x_result;
    }
    i2c_result = i2c_write_addr8_data8(0xFF, 0x01);
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    i2c_result = i2c_write_addr8_data8(
        VL53L0X_REG_DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    i2c_result = i2c_write_addr8_data8(
        VL53L0X_REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    i2c_result = i2c_write_addr8_data8(0xFF, 0x00);
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    i2c_result = i2c_write_addr8_data8(
        VL53L0X_REG_GLOBAL_CONFIG_REF_EN_START_SELECT, SPAD_START_SELECT);
    if (i2c_result != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    uint8_t offset =
        (spad_type == SPAD_TYPE_APERTURE) ? SPAD_APERTURE_START_INDEX : 0;

    /* Create a new SPAD array by selecting a subset of the SPADs suggested by
     * the good SPAD map. The subset should only have the number of type enabled
     * as suggested by the reading from the NVM (spads_to_enable_count and
     * spad_type). */
    for (int row = 0; row < SPAD_MAP_ROW_COUNT; row++) {
        for (int column = 0; column < SPAD_ROW_SIZE; column++) {
            int index = (row * SPAD_ROW_SIZE) + column;
            if (index >= SPAD_MAX_COUNT) {
                return e_VL53L0X_RESULT_ERROR_SPAD;
            }
            if (spads_enabled_count == spads_to_enable_count) {
                // We are done
                break;
            }
            if (index < offset) {
                continue;
            }
            if ((good_spad_map[row] >> column) & 0x1) {
                spad_map[row] |= (1 << column);
                spads_enabled_count++;
            }
        }
        if (spads_enabled_count == spads_to_enable_count) {
            // To avoid looping unnecessarily when we are already done.
            break;
        }
    }

    if (spads_enabled_count != spads_to_enable_count) {
        return e_VL53L0X_RESULT_ERROR_SPAD;
    }

    // Write the new SPAD configuration
    const uint8_t reg_global_cfg_addr =
        VL53L0X_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0;
    if (i2c_write(&reg_global_cfg_addr, 1, spad_map, SPAD_MAP_ROW_COUNT)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    return e_VL53L0X_RESULT_OK;
}

/**
 * Load tuning settings (same as default tuning settings provided by ST api
 * code)
 */
static e__vl53l0x_result vl53l0x_load_default_tuning_settings(void) {
    const uint8_t default_tuning_regs[80][2] = {
        {0xFF, 0x01}, {0x00, 0x00}, {0xFF, 0x00}, {0x09, 0x00}, {0x10, 0x00},
        {0x11, 0x00}, {0x24, 0x01}, {0x25, 0xFF}, {0x75, 0x00}, {0xFF, 0x01},
        {0x4E, 0x2C}, {0x48, 0x00}, {0x30, 0x20}, {0xFF, 0x00}, {0x30, 0x09},
        {0x54, 0x00}, {0x31, 0x04}, {0x32, 0x03}, {0x40, 0x83}, {0x46, 0x25},
        {0x60, 0x00}, {0x27, 0x00}, {0x50, 0x06}, {0x51, 0x00}, {0x52, 0x96},
        {0x56, 0x08}, {0x57, 0x30}, {0x61, 0x00}, {0x62, 0x00}, {0x64, 0x00},
        {0x65, 0x00}, {0x66, 0xA0}, {0xFF, 0x01}, {0x22, 0x32}, {0x47, 0x14},
        {0x49, 0xFF}, {0x4A, 0x00}, {0xFF, 0x00}, {0x7A, 0x0A}, {0x7B, 0x00},
        {0x78, 0x21}, {0xFF, 0x01}, {0x23, 0x34}, {0x42, 0x00}, {0x44, 0xFF},
        {0x45, 0x26}, {0x46, 0x05}, {0x40, 0x40}, {0x0E, 0x06}, {0x20, 0x1A},
        {0x43, 0x40}, {0xFF, 0x00}, {0x34, 0x03}, {0x35, 0x44}, {0xFF, 0x01},
        {0x31, 0x04}, {0x4B, 0x09}, {0x4C, 0x05}, {0x4D, 0x04}, {0xFF, 0x00},
        {0x44, 0x00}, {0x45, 0x20}, {0x47, 0x08}, {0x48, 0x28}, {0x67, 0x00},
        {0x70, 0x04}, {0x71, 0x01}, {0x72, 0xFE}, {0x76, 0x00}, {0x77, 0x00},
        {0xFF, 0x01}, {0x0D, 0x01}, {0xFF, 0x00}, {0x80, 0x01}, {0x01, 0xF8},
        {0xFF, 0x01}, {0x8E, 0x01}, {0x00, 0x01}, {0xFF, 0x00}, {0x80, 0x00}};
    for (uint8_t i = 0; i < ARRAY_SIZE(default_tuning_regs); i++) {
        e__i2c_result result;
        result = i2c_write_addr8_data8(default_tuning_regs[i][0],
                                       default_tuning_regs[i][1]);
        if (result != I2C_RESULT_OK)
            return e_VL53L0X_RESULT_ERROR_I2C;
    }
    return e_VL53L0X_RESULT_OK;
}

// INTERRUPT SERVICE ROUTINE FUNCTIONS FOR INDICATING WHEN RANGE SENSOR
// MEASUREMENTS ARE FINISHED
static void right_measurement_done_isr() {
    status_multiple_front_right = STATUS_SINGLE_DONE;
    if (status_multiple_front_middle == STATUS_SINGLE_DONE &&
        status_multiple_front_left == STATUS_SINGLE_DONE)
        status_multiple = STATUS_MULTIPLE_DONE;
}
static void middle_measurement_done_isr() {
    status_multiple_front_middle = STATUS_SINGLE_DONE;
    if (status_multiple_front_right == STATUS_SINGLE_DONE &&
        status_multiple_front_left == STATUS_SINGLE_DONE)
        status_multiple = STATUS_MULTIPLE_DONE;
}
static void left_measurement_done_isr() {
    status_multiple_front_left = STATUS_SINGLE_DONE;
    if (status_multiple_front_middle == STATUS_SINGLE_DONE &&
        status_multiple_front_right == STATUS_SINGLE_DONE)
        status_multiple = STATUS_MULTIPLE_DONE;
}

static e__vl53l0x_result vl53l0x_configure_interrupt(void) {
    // Interrupt on new sample ready
    if (i2c_write_addr8_data8(
            VL53L0X_REG_SYSTEM_INTERRUPT_CONFIG_GPIO,
            VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY) !=
        I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    /* Configure active low since the pin is pulled-up on most breakout boards
     */
    uint8_t gpio_hv_mux_active_high = 0;
    if (i2c_read_addr8_data8(VL53L0X_REG_GPIO_HV_MUX_ACTIVE_HIGH,
                             &gpio_hv_mux_active_high) != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    gpio_hv_mux_active_high &= ~0x10;
    if (i2c_write_addr8_data8(VL53L0X_REG_GPIO_HV_MUX_ACTIVE_HIGH,
                              gpio_hv_mux_active_high) != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    if (i2c_write_addr8_data8(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01) !=
        I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    return e_VL53L0X_RESULT_OK;
}

/**
 * Enable (or disable) specific steps in the sequence
 */
static e__vl53l0x_result
vl53l0x_set_sequence_steps_enabled(uint8_t sequence_step) {
    if (i2c_write_addr8_data8(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG,
                              sequence_step) != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    return e_VL53L0X_RESULT_OK;
}

static e__vl53l0x_result vl53l0x_static_init(void) {
    e__vl53l0x_result result = vl53l0x_set_spads_from_nvm();
    if (result) {
        return result;
    }
    result = vl53l0x_load_default_tuning_settings();
    if (result) {
        return result;
    }
    result = vl53l0x_configure_interrupt();
    if (result) {
        return result;
    }

    result = vl53l0x_set_sequence_steps_enabled(
        RANGE_SEQUENCE_STEP_DSS + RANGE_SEQUENCE_STEP_PRE_RANGE +
        RANGE_SEQUENCE_STEP_FINAL_RANGE);
    return result;
}

static e__vl53l0x_result
vl53l0x_perform_single_ref_calibration(e__vl53l0x_calibration_type calib_type) {
    uint8_t sysrange_start = 0;
    uint8_t sequence_config = 0;
    switch (calib_type) {
    case VL53L0X_CALIBRATION_TYPE_VHV:
        sequence_config = 0x01;
        sysrange_start = 0x01 | 0x40;
        break;
    case VL53L0X_CALIBRATION_TYPE_PHASE:
        sequence_config = 0x02;
        sysrange_start = 0x01;
        break;
    }
    if (i2c_write_addr8_data8(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG,
                              sequence_config)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    if (i2c_write_addr8_data8(VL53L0X_REG_SYSRANGE_START, sysrange_start)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    // Wait for interrupt
    uint8_t interrupt_status = 0;
    e__i2c_result i2c_result = I2C_RESULT_OK;
    do {
        i2c_result = i2c_read_addr8_data8(VL53L0X_REG_RESULT_INTERRUPT_STATUS,
                                          &interrupt_status);
    } while (i2c_result == I2C_RESULT_OK && ((interrupt_status & 0x07) == 0));
    if (i2c_result) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    if (i2c_write_addr8_data8(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    if (i2c_write_addr8_data8(VL53L0X_REG_SYSRANGE_START, 0x00)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    return e_VL53L0X_RESULT_OK;
}

/**
 * Temperature calibration needs to be run again if the temperature changes by
 * more than 8 degrees according to the datasheet.
 */
static e__vl53l0x_result vl53l0x_perform_ref_calibration(void) {
    e__vl53l0x_result result =
        vl53l0x_perform_single_ref_calibration(VL53L0X_CALIBRATION_TYPE_VHV);
    if (result) {
        return result;
    }
    result =
        vl53l0x_perform_single_ref_calibration(VL53L0X_CALIBRATION_TYPE_PHASE);
    if (result) {
        return result;
    }
    // Restore sequence steps enabled
    result = vl53l0x_set_sequence_steps_enabled(
        RANGE_SEQUENCE_STEP_DSS + RANGE_SEQUENCE_STEP_PRE_RANGE +
        RANGE_SEQUENCE_STEP_FINAL_RANGE);
    return result;
}

static void vl53l0x_configure_front_sensors_interrupt(void) {
    static const struct io_config range_interrupt_config = {
        .io_sel = IO_SEL_GPIO,
        .io_ren = IO_REN_DISABLE,
        .io_dir = IO_DIR_INPUT,
        .io_out = IO_OUT_LOW};
    struct io_config current_config_right;
    struct io_config current_config_middle;
    struct io_config current_config_left;
    io_get_current_config(RANGE_INTERRUPT_RIGHT, &current_config_right);
    io_get_current_config(RANGE_INTERRUPT_MIDDLE, &current_config_middle);
    io_get_current_config(RANGE_INTERRUPT_LEFT, &current_config_left);
    ASSERT(io_config_compare(&range_interrupt_config, &current_config_right));
    ASSERT(io_config_compare(&range_interrupt_config, &current_config_middle));
    ASSERT(io_config_compare(&range_interrupt_config, &current_config_left));

    io_configure_interrupt(RANGE_INTERRUPT_RIGHT, IO_TRIGGER_FALLING,
                           right_measurement_done_isr);
    io_configure_interrupt(RANGE_INTERRUPT_MIDDLE, IO_TRIGGER_FALLING,
                           middle_measurement_done_isr);
    io_configure_interrupt(RANGE_INTERRUPT_LEFT, IO_TRIGGER_FALLING,
                           left_measurement_done_isr);
    io_enable_interrupt(RANGE_INTERRUPT_RIGHT);
    io_enable_interrupt(RANGE_INTERRUPT_MIDDLE);
    io_enable_interrupt(RANGE_INTERRUPT_LEFT);
}

static e__vl53l0x_result vl53l0x_init_config(e__vl53l0x_pos pos) {
    e__vl53l0x_result result;
    i2c_set_slave_address(
        vl53l0x_cfgs[pos]
            .addr); // Set the slave address of the range sensor to initialize
                    // in the MSP430 I2C slave address register

    result =
        vl53l0x_data_init(); // Initialize range sensor's supply voltage and I2C
    if (result != e_VL53L0X_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_PWRUP;

    result = vl53l0x_static_init(); // Initialize range sensor's registers and
                                    // interrupt
    if (result != e_VL53L0X_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_PWRUP;

    result = vl53l0x_perform_ref_calibration();
    if (result != e_VL53L0X_RESULT_OK)
        return e_VL53L0X_RESULT_ERROR_PWRUP;

    // I think NSUMO video only has 1 interrupt IO pin available on his MCU, but
    // ours has 3 (1 for each range sensor). So he's trying to get measurements
    // based on 1 interrupt. I think we can just wait for all 3 interrupt IO
    // pins to know that data is ready to be read from the range sensor using
    // his function vl53l0x_read_range_multiple().
    // TODO: Make function vl53l0x_read_range_multiple()
    vl53l0x_configure_front_sensors_interrupt(); // Possibly configure interrupt
                                                 // for all range sensor pins in
                                                 // this function, instead of
                                                 // just 1 sensor because he
                                                 // only had 1 interrupt IO
                                                 // available, but we have 3
                                                 // available
    return e_VL53L0X_RESULT_OK;
}

static e__vl53l0x_result vl53l0x_start_sysrange(e__vl53l0x_pos pos) {
    i2c_set_slave_address(vl53l0x_cfgs[pos].addr);
    const uint8_t sysrange_regs[6][2] = {{0x80, 0x01}, {0xFF, 0x01},
                                         {0x00, 0x00}, {0x00, 0x01},
                                         {0xFF, 0x00}, {0x80, 0x00}};
    // 3 writes
    e__i2c_result result =
        i2c_write_addr8_data8(sysrange_regs[0][0], sysrange_regs[0][1]);
    if (result != I2C_RESULT_OK) {
        return result;
    }
    result = i2c_write_addr8_data8(sysrange_regs[1][0], sysrange_regs[1][1]);
    if (result != I2C_RESULT_OK) {
        return result;
    }
    result = i2c_write_addr8_data8(sysrange_regs[2][0], sysrange_regs[2][1]);
    if (result != I2C_RESULT_OK) {
        return result;
    }
    if (i2c_write_addr8_data8(0x91, stop_variable) != I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    result = i2c_write_addr8_data8(sysrange_regs[3][0], sysrange_regs[3][1]);
    if (result) {
        return result;
    }
    result = i2c_write_addr8_data8(sysrange_regs[4][0], sysrange_regs[4][1]);
    if (result) {
        return result;
    }
    result = i2c_write_addr8_data8(sysrange_regs[5][0], sysrange_regs[5][1]);
    if (result) {
        return result;
    }
    if (i2c_write_addr8_data8(VL53L0X_REG_SYSRANGE_START, 0x01)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    uint8_t sysrange_start = 0;
    e__i2c_result i2c_result = I2C_RESULT_OK;
    do {
        i2c_result =
            i2c_read_addr8_data8(VL53L0X_REG_SYSRANGE_START, &sysrange_start);
    } while (i2c_result == I2C_RESULT_OK && (sysrange_start & 0x01));
    return i2c_result == I2C_RESULT_OK ? e_VL53L0X_RESULT_OK
                                       : e_VL53L0X_RESULT_ERROR_I2C;
}

// TODO: Verify this works after bring up real robot
e__vl53l0x_result vl53l0x_start_measuring_multiple(void) {
    ASSERT(initialized);
    if (status_multiple == STATUS_MULTIPLE_MEASURING) {
        return e_VL53L0X_RESULT_ERROR_MEASURE_ONGOING;
    }
    status_multiple = STATUS_MULTIPLE_MEASURING;
    e__vl53l0x_result result = vl53l0x_start_sysrange(e_VL53L0X_POS_FRONT);
    if (result != e_VL53L0X_RESULT_OK) {
        return result;
    }
    result = vl53l0x_start_sysrange(e_VL53L0X_POS_FRONT_LEFT);
    if (result != e_VL53L0X_RESULT_OK) {
        return result;
    }
    result = vl53l0x_start_sysrange(e_VL53L0X_POS_FRONT_RIGHT);
    if (result != e_VL53L0X_RESULT_OK) {
        return result;
    }
    return e_VL53L0X_RESULT_OK;
}

// Assumes I2C address is set already
static e__vl53l0x_result vl53l0x_pollwait_sysrange(void) {
    e__i2c_result i2c_result = I2C_RESULT_OK;
    uint8_t interrupt_status = 0;
    do {
        i2c_result = i2c_read_addr8_data8(VL53L0X_REG_RESULT_INTERRUPT_STATUS,
                                          &interrupt_status);
    } while (i2c_result == I2C_RESULT_OK && ((interrupt_status & 0x07) == 0));
    return i2c_result == I2C_RESULT_OK ? e_VL53L0X_RESULT_OK
                                       : e_VL53L0X_RESULT_ERROR_I2C;
}

// Assumes I2C address is set already
static e__vl53l0x_result vl53l0x_clear_sysrange_interrupt(void) {
    if (i2c_write_addr8_data8(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01)) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }
    return e_VL53L0X_RESULT_OK;
}

static e__vl53l0x_result vl53l0x_read_range(e__vl53l0x_pos pos,
                                            uint16_t *range) {
    i2c_set_slave_address(vl53l0x_cfgs[pos].addr);

    e__vl53l0x_result result = vl53l0x_pollwait_sysrange();
    if (result != e_VL53L0X_RESULT_OK) {
        return result;
    }

    if (i2c_read_addr8_data16(VL53L0X_REG_RESULT_RANGE_STATUS + 10, range) !=
        I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    if (i2c_write_addr8_data8(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01) !=
        I2C_RESULT_OK) {
        return e_VL53L0X_RESULT_ERROR_I2C;
    }

    // 8190 or 8191 may be returned when obstacle is out of range.
    if (*range == 8190 || *range == 8191) {
        *range = VL53L0X_OUT_OF_RANGE;
    }

    result = vl53l0x_clear_sysrange_interrupt();
    return result;
}

e__vl53l0x_result vl53l0x_read_range_single(e__vl53l0x_pos pos,
                                            uint16_t *range) {
    ASSERT(initialized);
    e__vl53l0x_result result = vl53l0x_start_sysrange(pos);
    if (result != e_VL53L0X_RESULT_OK) {
        return result;
    }
    result = vl53l0x_read_range(pos, range);
    return result;
}

/*
 * The approach is as follow:
 * For multiple sensors and single interrupt line:
 * 1. Start measure on all sensors
 * 2. If measurement ready
 *    - Read measurement of all sensors
 * 3. else
 *    - Return old values
 * 4. Update measurement ready on interrupt
 */
// TODO: Verify this works after bring up real robot
e__vl53l0x_result vl53l0x_read_range_multiple(t__vl53l0x_ranges ranges,
                                              bool *fresh_values) {
    ASSERT(initialized);
    e__vl53l0x_result result = e_VL53L0X_RESULT_OK;
    if (status_multiple == STATUS_MULTIPLE_NOT_STARTED) {
        result = vl53l0x_start_measuring_multiple();
        if (result) {
            return result;
        }
        // Block here the first time
        while (status_multiple != STATUS_MULTIPLE_DONE) {
        }
    }

    // If all range sensor interrupt have triggered and are ready to be read
    if (status_multiple == STATUS_MULTIPLE_DONE) {
        // Read data from front middle range sensor
        result = vl53l0x_read_range(e_VL53L0X_POS_FRONT,
                                    &latest_ranges[e_VL53L0X_POS_FRONT]);
        if (result) {
            return result;
        }

        // Read data from front left range sensor
        result = vl53l0x_read_range(e_VL53L0X_POS_FRONT_LEFT,
                                    &latest_ranges[e_VL53L0X_POS_FRONT_LEFT]);
        if (result) {
            return result;
        }

        // Read data from front rightrange sensor
        result = vl53l0x_read_range(e_VL53L0X_POS_FRONT_RIGHT,
                                    &latest_ranges[e_VL53L0X_POS_FRONT_RIGHT]);
        if (result) {
            return result;
        }

        result = vl53l0x_start_measuring_multiple();
        if (result) {
            return result;
        }
        // Use new values if sensors are done sensing
        *fresh_values = true;
    } else {
        // If unable to read new data because sensors are not done sensing yet,
        // re-use old values
        *fresh_values = false;
    }
    // Store range sensor data
    for (int i = 0; i < VL53L0X_POS_COUNT; i++) {
        ranges[i] = latest_ranges[i];
    }
    return result;
}

e__vl53l0x_result vl53l0x_init(void) {
    ASSERT(!initialized);
    e__vl53l0x_result result;
    i2c_init(); // Initialize I2C of MSP430F5529 to communicate with VL53L0X
                // range sensor

    result = vl53l0x_init_all_dev_addr();
    if (result != e_VL53L0X_RESULT_OK)
        return result;
    vl53l0x_init_config(e_VL53L0X_POS_FRONT);
#ifdef SUMOBOT
    vl53l0x_init_config(e_VL53L0X_POS_FRONT_LEFT);
    vl53l0x_init_config(e_VL53L0X_POS_FRONT_RIGHT);
#endif

    initialized = true;
    return e_VL53L0X_RESULT_OK;
}
