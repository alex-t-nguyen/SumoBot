#include <msp430.h>
#include <stdint.h>
#include "drivers/mcu_init.h"
#include "drivers/led.h"
#include "drivers/io.h"
#include "drivers/uart.h"
#include "common/defines.h"
#include "common/assert_handler.h"
#include "common/trace.h"
#include "externals/printf/printf.h"

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

SUPPRESS_UNUSED
static void test_putchar(void) {
    test_setup();
    uart_init();
    while(1) {
        _putchar('h');
        _putchar('e');
        _putchar('l');
        _putchar('l');
        _putchar('o');
        _putchar(' '); 
        _putchar('w');
        _putchar('o');
        _putchar('r');
        _putchar('l');
        _putchar('d');
        _putchar('\n');
        BUSY_WAIT_ms(1000);
    }
}

SUPPRESS_UNUSED
static void test_printf(void) {
    test_setup();
    uart_init();
    while(1) {
        printf("Hello World! %d\n", 2025); 
        BUSY_WAIT_ms(1000);
    }
}

SUPPRESS_UNUSED
static void test_trace(void) {
    test_setup();
    trace_init();
    while(1) {
        TRACE("Hello World! %d\n", 2025);
        BUSY_WAIT_ms(1000);
    }
}

SUPPRESS_UNUSED
static void test_uart_polling(void) {
    test_setup();
    uart_init();
    while(1) {
        uart_putchar_polling('h');
        uart_putchar_polling('e');
        uart_putchar_polling('l');
        uart_putchar_polling('l');
        uart_putchar_polling('o');
        uart_putchar_polling(' '); 
        uart_putchar_polling('w');
        uart_putchar_polling('o');
        uart_putchar_polling('r');
        uart_putchar_polling('l');
        uart_putchar_polling('d');
        uart_putchar_polling('\n');
        BUSY_WAIT_ms(1000);
    }
}

SUPPRESS_UNUSED
static void test_uart_interrupt(void) {
    test_setup();
    uart_init();
    while(1) {
        uart_putchar_interrupt('h');
        uart_putchar_interrupt('e');
        uart_putchar_interrupt('l');
        uart_putchar_interrupt('l');
        uart_putchar_interrupt('o');
        uart_putchar_interrupt(' '); 
        uart_putchar_interrupt('w');
        uart_putchar_interrupt('o');
        uart_putchar_interrupt('r');
        uart_putchar_interrupt('l');
        uart_putchar_interrupt('d');
        uart_putchar_interrupt('\n');
        BUSY_WAIT_ms(500);
    }
}

int main() {
    TEST(); // Define passed to Makefile to run a specific test function
    ASSERT(0);
}
