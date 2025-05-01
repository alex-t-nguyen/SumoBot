#include "drivers/pwm.h"
#include "common/assert_handler.h"
#include "common/defines.h"
#include "common/trace.h"
#include "drivers/io.h"
#include <assert.h>
#include <msp430.h>
#include <stdbool.h>
#include <stdint.h>

#define PWM_TIMER_FREQ_HZ (SMCLK / TIMER_INPUT_DIVIER_3)
#define PWM_PERIOD_FREQ_HZ (20000U)
#define PWM_PERIOD_TICKS (PWM_TIMER_FREQ_HZ / PWM_PERIOD_FREQ_HZ)
static_assert(PWM_PERIOD_TICKS == 100, "Expect 100 ticks per period");

#define PWM_TAxCCR0                                                            \
    (PWM_PERIOD_TICKS - 1) // Subtract 1 because timer counts from 0
static bool initialized = false;
static const struct io_config pwm_io_config = {.io_sel = IO_SEL_ALT2,
                                               .io_dir = IO_DIR_OUTPUT,
                                               .io_ren = IO_REN_DISABLE,
                                               .io_out = IO_OUT_LOW};

struct pwm_channel_cfg {
    bool enable;
    volatile unsigned int *const cctl;
    volatile unsigned int *const ccr;
};

static struct pwm_channel_cfg pwm_cfgs[] = {
    [MOTOR_RIGHT_IN1] = {.enable = false, .cctl = &TA2CCTL2, .ccr = &TA2CCR2},
    [MOTOR_RIGHT_IN2] = {.enable = false, .cctl = &TA2CCTL1, .ccr = &TA2CCR1},
    [MOTOR_LEFT_IN1] = {.enable = false, .cctl = &TA0CCTL4, .ccr = &TA0CCR4},
    [MOTOR_LEFT_IN2] = {.enable = false, .cctl = &TA0CCTL3, .ccr = &TA0CCR3}};

/**
 * Initialize PWM timer registers
 */
void pwm_init(void) {
    ASSERT(!initialized);

    // Check that motor driver input pins are configured correctly
    struct io_config mdrv_r1_io_config, mdrv_r2_io_config, mdrv_l1_io_config,
        mdrv_l2_io_config;
    io_get_current_config(MOTOR_RIGHT_IN1, &mdrv_r1_io_config);
    io_get_current_config(MOTOR_RIGHT_IN2, &mdrv_r2_io_config);
    io_get_current_config(MOTOR_LEFT_IN1, &mdrv_l1_io_config);
    io_get_current_config(MOTOR_LEFT_IN2, &mdrv_l2_io_config);
    // TRACE("PWM io config io_sel = %d", pwm_io_config.io_sel);
    // TRACE("MDRV_L1 io_sel = %d", mdrv_l1_io_config.io_sel);
    // TRACE("PWM io config io_dir = %d", pwm_io_config.io_dir);
    // TRACE("MDRV_L1 io_dir = %d", mdrv_l1_io_config.io_dir);
    // TRACE("PWM io config io_ren = %d", pwm_io_config.io_ren);
    // TRACE("MDRV_L1 io_ren = %d", mdrv_l1_io_config.io_ren);
    // TRACE("PWM io config io_out = %d", pwm_io_config.io_out);
    // TRACE("MDRV_L1 io_out = %d", mdrv_l1_io_config.io_out);
    ASSERT(io_config_compare(&pwm_io_config, &mdrv_r1_io_config));
    ASSERT(io_config_compare(&pwm_io_config, &mdrv_r2_io_config));
    ASSERT(io_config_compare(&pwm_io_config, &mdrv_l1_io_config));
    ASSERT(io_config_compare(&pwm_io_config, &mdrv_l2_io_config));
    /**
     * TAxCTL0: [Reserved (15-10), TASSEL (9-8), ID (7-6), MC (5-4), Reserved
     * (3), TACLR (2), TAIE (1), TAIFG (0)] TASSEL = 2 (Clock source SMCLK) ID =
     * 3 (Divide clock source by 8) MC = 0 (Timer Mode: Halted)
     */
    TA0CTL |= (TASSEL_2 | ID_3 | MC_0);
    TA2CTL |= (TASSEL_2 | ID_3 | MC_0);
    TA0CCR0 = PWM_TAxCCR0;
    TA2CCR0 = PWM_TAxCCR0;
    initialized = true;
}

/**
 * Checks if all pwm channels (motor drive inputs) are disabled
 */
static bool pwm_ch_all_disabled() {
    for (uint8_t ch = 0; ch < ARRAY_SIZE(pwm_cfgs); ch++) {
        if (pwm_cfgs[ch].enable)
            return false;
    }
    return true;
}

/**
 * Resets timer and sets timer to count up mode
 */
static void pwm_enable(bool enable) {
    /**
     * MC_0: Timer is halted
     * MC_1: Up mode (Count up to TAxCCR0)
     */
    TA0CTL = (enable ? MC_1 : MC_0) + TACLR;
    TA2CTL = (enable ? MC_1 : MC_0) + TACLR;
}

/**
 * Enables PWM channel that connects to motor driver
 * or disables all PWM channels if they are all off
 */
static void pwm_channel_enable(mdrv_enum mdrv, bool enable) {
    if (pwm_cfgs[mdrv].enable != enable) {
        *pwm_cfgs[mdrv].cctl = enable ? OUTMOD_7 : OUTMOD_0;
        pwm_cfgs[mdrv].enable = enable;
        if (enable) {
            pwm_enable(true);
        } else if (pwm_ch_all_disabled()) {
            pwm_enable(false);
        }
    }
}

/**
 *  Scale input PWM duty cycle down by 25%
 *  This will lower the average voltage from VM on the motor
 *  NOTE: Might not use this function if able to get 12V rated motors.
 */
static inline uint8_t pwm_scale_duty_cycle(uint8_t duty_cycle_percent) {
    /**
     * Battery is at ~8V and motor supplies are rated for 6V, so scale down the
     * applied input PWM duty cycle by 25% so that average voltage will be ~6V
     * max. This function should never return 0 because that is the same as if
     * PWM is disabled.
     */
    return duty_cycle_percent == 1 ? duty_cycle_percent
                                   : (duty_cycle_percent * 3 / 4);
}

/**
 * Set duty cycle of PWM for specified motor driver
 */
void pwm_set_duty_cycle(mdrv_enum mdrv, uint8_t duty_cycle_percent) {
    ASSERT(initialized);
    const bool enable =
        duty_cycle_percent > 0; // Check if given duty cycle is > 0%
    if (enable) {               // If duty cycle is greater than 0
        *pwm_cfgs[mdrv].ccr = pwm_scale_duty_cycle(
            duty_cycle_percent); // Set PWM to scaled target duty cycle
    }
    pwm_channel_enable(mdrv,
                       enable); // Enable PWM output of MCU to motor driver
}
