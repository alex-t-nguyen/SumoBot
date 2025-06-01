#include "drivers/drv8848.h"
#include "common/assert_handler.h"
#include "common/trace.h"
#include "drivers/io.h"
#include "drivers/pwm.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static bool initialized = false;

static const drv8848_enum motor_sides[][2] = {
    [MOTORS_RIGHT] = {DRV8848_RIGHT1, DRV8848_RIGHT2},
    [MOTORS_LEFT] = {DRV8848_LEFT1, DRV8848_LEFT2}};

/**
 * Checks io pins (besides the PWM inputs) for motor drivers are configured
 * correctly
 */
static void drv8848_assert_io_config(void) {
    // IO config for ENABLE pin of motor driver
    const struct io_config motor_enable_config = {.io_sel = IO_SEL_GPIO,
                                                  .io_dir = IO_DIR_OUTPUT,
                                                  .io_ren = IO_REN_DISABLE,
                                                  .io_out = IO_OUT_LOW};

    // IO config for nFAULT pin of motor driver
    const struct io_config motor_nfault_config = {.io_sel = IO_SEL_GPIO,
                                                  .io_dir = IO_DIR_INPUT,
                                                  .io_ren = IO_REN_ENABLE,
                                                  .io_out = IO_OUT_HIGH};
    struct io_config curr_motor_enable_config, curr_motor_right_nfault_config,
        curr_motor_left_nfault_config;
    io_get_current_config(MOTOR_ENABLE, &curr_motor_enable_config);
    io_get_current_config(MOTOR_RIGHT_NFAULT, &curr_motor_right_nfault_config);
    io_get_current_config(MOTOR_LEFT_NFAULT, &curr_motor_left_nfault_config);
    ASSERT(io_config_compare(&motor_enable_config, &curr_motor_enable_config));
    ASSERT(io_config_compare(&motor_nfault_config,
                             &curr_motor_right_nfault_config));
    ASSERT(io_config_compare(&motor_nfault_config,
                             &curr_motor_left_nfault_config));
}

/**
 * Initializes motor driver PWM and checks IO configuration
 * for other pins of motor driver (enable and nFAULT)
 */
void drv8848_init(void) {
    ASSERT(!initialized);

    pwm_init();
    drv8848_assert_io_config();
    drv8848_enable(true); // Enable both motor drivers
    initialized = true;
}

/**
 * Set the mode of the motor driver
 * Mode depends on state of PWM inputs
 */
void drv8848_set_mode(motor_side_enum side, drv8848_mode_enum mode,
                      uint8_t duty_cycle) {
    /**
     * xIN1 xIN2 xOUT1 xOUT2 Function (DC Motor)
     * 0    0    Z     Z     Coast (fast decay)
     * 0    1    L     H     Reverse
     * 1    0    H     L     Forward
     * 1    1    L     L     Brake (slow decay)
     */
    switch (mode) {
    case DRV8848_MODE_COAST:
        // Set both inputs to LOW (0% duty cycle)
        drv8848_set_pwm(motor_sides[side][0],
                        0); // Set left/right motor xIN1 to 0% duty cycle
        drv8848_set_pwm(motor_sides[side][1],
                        0); // Set left/right motor xIN2 to 0% duty cycle
        // drv8848_set_pwm(DRV8848_RIGHT1, 0);
        // drv8848_set_pwm(DRV8848_RIGHT2, 0);
        // drv8848_set_pwm(DRV8848_LEFT1, 0);
        // drv8848_set_pwm(DRV8848_LEFT2, 0);
        break;
    case DRV8848_MODE_FORWARD:
        // PWM xIN1, Disable xIN2
        drv8848_set_pwm(
            motor_sides[side][0],
            duty_cycle); // Set left/right motor xIN1 to current duty cycle
        drv8848_set_pwm(motor_sides[side][1],
                        0); // Set left/right motor xIN2 to 0% duty cycle
        // drv8848_set_pwm(DRV8848_RIGHT1, curr_pwm_dutycycle);
        // drv8848_set_pwm(DRV8848_RIGHT2, 0);
        // drv8848_set_pwm(DRV8848_LEFT1, curr_pwm_dutycycle);
        // drv8848_set_pwm(DRV8848_LEFT2, 0);
        break;
    case DRV8848_MODE_REVERSE:
        // Disable xIN1, PWM xIN2
        drv8848_set_pwm(motor_sides[side][0],
                        0); // Set left/right motor xIN1 to 0% duty cycle
        drv8848_set_pwm(
            motor_sides[side][1],
            duty_cycle); // Set left/right motor xIN2 to current duty cycle
        // drv8848_set_pwm(DRV8848_RIGHT1, 0);
        // drv8848_set_pwm(DRV8848_RIGHT2, curr_pwm_dutycycle);
        // drv8848_set_pwm(DRV8848_LEFT1, 0);
        // drv8848_set_pwm(DRV8848_LEFT2, curr_pwm_dutycycle);
        break;
    case DRV8848_MODE_STOP:
        // Enable both inputs
        drv8848_set_pwm(motor_sides[side][0],
                        100); // Set left/right motor xIN1 to 100% duty cycle
        drv8848_set_pwm(motor_sides[side][1],
                        100); // Set left/right motor xIN2 to 100% duty cycle
        // drv8848_set_pwm(DRV8848_RIGHT1, 100);
        // drv8848_set_pwm(DRV8848_RIGHT2, 100);
        // drv8848_set_pwm(DRV8848_LEFT1, 100);
        // drv8848_set_pwm(DRV8848_LEFT2, 100);
        break;
    }
}

/**
 * Set the PWM duty cycle inputs of the motor driver
 */
static_assert(DRV8848_RIGHT1 == (int)PWM_DRV8848_RIGHT1,
              "PWM and DRV RIGHT1 enum mismatch");
static_assert(DRV8848_RIGHT2 == (int)PWM_DRV8848_RIGHT2,
              "PWM and DRV RIGHT2 enum mismatch");
static_assert(DRV8848_LEFT1 == (int)PWM_DRV8848_LEFT1,
              "PWM and DRV LEFT1 enum mismatch");
static_assert(DRV8848_LEFT2 == (int)PWM_DRV8848_LEFT2,
              "PWM and DRV LEFT2 enum mismatch");
void drv8848_set_pwm(drv8848_enum device, uint8_t duty_cycle) {
    pwm_set_duty_cycle((mdrv_enum)device, duty_cycle);
}

/**
 * Enable or disable both motor drivers
 */
void drv8848_enable(bool enable) {
    // Enable both motors
    if (enable)
        io_set_out(MOTOR_ENABLE, IO_OUT_HIGH);
    else // Disable both motors
        io_set_out(MOTOR_ENABLE, IO_OUT_LOW);
}

/**
 * Check nfault pin value of motor driver
 * IO config compare for nFAULT already handled in drv8848_init()
 */
void drv8848_check_nfault(motor_side_enum side) {
    // Right Side motor = 0
    // Left side motor = 1
    io_signal_enum nfault_pin = side ? MOTOR_LEFT_NFAULT : MOTOR_RIGHT_NFAULT;
    bool nfault_stat = io_read_input(nfault_pin); // Read nfault pin
    if (!nfault_stat) {
        TRACE("nFAULT = 0");
        drv8848_enable(false); // Disable both motor drivers
        // Turn off PWM inputs to both motors
        drv8848_set_pwm(DRV8848_RIGHT1, 0);
        drv8848_set_pwm(DRV8848_RIGHT2, 0);
        drv8848_set_pwm(DRV8848_LEFT1, 0);
        drv8848_set_pwm(DRV8848_LEFT2, 0);
    } else {
        TRACE("nFAULT = 1");
    }
}
