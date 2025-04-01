#include "printf.h"
#include <drivers/io.h>
#include <msp430.h>
#include <stdint.h>

int main(void) {
    WDTCTL =
        WDTPW |
        WDTHOLD; // stop watchdog timer, otherwise it prevents program from
                 // running because it keeps restarting from watchdog timeout
    io_init();
    const struct io_config led_config = {
        .io_dir = IO_DIR_OUTPUT,
        .io_sel = IO_SEL_GPIO,
        .io_ren = IO_REN_DISABLE,
        .io_out = IO_OUT_HIGH,
    };
    config_io(IO_TEST_LED, &led_config);
    io_out_enum io_out = IO_OUT_LOW;
    while (1) {
        __delay_cycles(1000000);
        io_out = (io_out == IO_OUT_LOW) ? IO_OUT_HIGH : IO_OUT_LOW;
        io_set_out(IO_TEST_LED, io_out);
    }
}
