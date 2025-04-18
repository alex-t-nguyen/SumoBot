#include <msp430.h>
#include <stdint.h>
#include "drivers/mcu_init.h"
#include "drivers/led.h"
#include "drivers/io.h"
#include "common/defines.h"
#include "common/assert_handler.h"

SUPPRESS_UNUSED
static void test_setup(void) {
    mcu_init();
}

SUPPRESS_UNUSED
static void test_assert(void) {
    test_setup();
    ASSERT(0)
}

SUPPRESS_UNUSED
static void test_blink_led(void) {
    test_setup();
    const struct io_config led_config = {.io_sel = IO_SEL_GPIO,
                                         .io_dir = IO_DIR_OUTPUT,
                                         .io_ren = IO_REN_DISABLE,
                                         .io_out = IO_OUT_LOW};
    config_io(IO_TEST_LED, &led_config);
    led_init(); // Check that IO_TEST_LED config is correct
    led_state_enum led_state = LED_STATE_ON;
    while (1) {
        BUSY_WAIT_ms(1000)
        led_state = (led_state == LED_STATE_OFF) ? LED_STATE_ON : LED_STATE_OFF;
        set_led(TEST_LED, led_state);
    }
}

SUPPRESS_UNUSED
static void io_range_interrupt_right_isr(void) {
    /* Test interrupt function to turn ON IO_TEST_LED */
    set_led(TEST_LED, LED_STATE_ON);
}

SUPPRESS_UNUSED
static void io_range_interrupt_left_isr(void) {
    /* Test interrupt function to turn OFF IO_TEST_LED */
    set_led(TEST_LED, LED_STATE_OFF);
}

SUPPRESS_UNUSED
static void test_io_interrupt(void) {
    test_setup();
    const struct io_config input_config = {
       .io_sel = IO_SEL_GPIO,
       .io_dir = IO_DIR_INPUT,
       .io_ren = IO_REN_ENABLE,
       .io_out = IO_OUT_HIGH
    };

    static const struct io_config led_config = {.io_sel = IO_SEL_GPIO,
                                            .io_dir = IO_DIR_OUTPUT,
                                            .io_ren = IO_REN_DISABLE,
                                            .io_out = IO_OUT_LOW};
    config_io(IO_TEST_LED, &led_config); // Configure IO_TEST_LED
    config_io(RANGE_INTERRUPT_RIGHT, &input_config);
    config_io(RANGE_INTERRUPT_LEFT, &input_config);
    led_init(); // Check that IO_TEST_LED config is correct
    io_configure_interrupt(RANGE_INTERRUPT_RIGHT, IO_TRIGGER_FALLING, io_range_interrupt_right_isr);
    io_configure_interrupt(RANGE_INTERRUPT_LEFT, IO_TRIGGER_FALLING, io_range_interrupt_left_isr);
    io_enable_interrupt(RANGE_INTERRUPT_RIGHT);
    io_enable_interrupt(RANGE_INTERRUPT_LEFT);
    while(1);
}

int main() {
    TEST(); // Define passed to Makefile to run a specific test function
    ASSERT(0);
}
