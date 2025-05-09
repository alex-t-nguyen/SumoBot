#include "common/defines.h"
#include "drivers/io.h"
#include "drivers/led.h"
#include "externals/printf/printf.h"
#include <msp430.h>
#include <stdint.h>
#include "common/assert_handler.h"
#include "drivers/mcu_init.h"

int main(void) {
    //WDTCTL =
    //    WDTPW |
    //    WDTHOLD; // stop watchdog timer, otherwise it prevents program from
                 // running because it keeps restarting from watchdog timeout
    //init_clocks();
    //io_init();
    mcu_init();
    const struct io_config led_config = {.io_sel = IO_SEL_GPIO,
                                         .io_dir = IO_DIR_OUTPUT,
                                         .io_ren = IO_REN_DISABLE,
                                         .io_out = IO_OUT_LOW};
    config_io(IO_TEST_LED, &led_config);
    // io_out_enum io_out = IO_OUT_LOW;
    led_init(); // Check that IO_TEST_LED config is correct
    led_state_enum led_state = LED_STATE_ON;
    ASSERT(0)
    while (1) {
        //__delay_cycles(1000000);
        BUSY_WAIT_ms(1000) led_state =
            (led_state == LED_STATE_OFF) ? LED_STATE_ON : LED_STATE_OFF;
        set_led(TEST_LED, led_state);
        // io_out = (io_out == IO_OUT_LOW) ? IO_OUT_HIGH : IO_OUT_LOW;
        // io_set_out(IO_TEST_LED, io_out);
    }
}
