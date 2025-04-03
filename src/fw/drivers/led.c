#include "led.h"
#include "common/assert_handler.h"
#include "drivers/io.h"
#include "common/defines.h"
#include <stdbool.h>

static bool initialized = false;

static const struct io_config led_config = {
    .io_sel = IO_SEL_GPIO,
    .io_dir = IO_DIR_OUTPUT,
    .io_ren = IO_REN_DISABLE,
    .io_out = IO_OUT_LOW 
};

void led_init(void) {
    ASSERT(!initialized);
    struct io_config current_config;
    io_get_current_config(IO_TEST_LED, &current_config);
    ASSERT(io_config_compare(&current_config, &led_config));
    initialized = true;
}

void set_led(led_enum led, led_state_enum state) {
    ASSERT(initialized);
    const io_out_enum out = (state == LED_STATE_OFF) ? IO_OUT_LOW : IO_OUT_HIGH;
    switch(led) {
        case TEST_LED:
           io_set_out(IO_TEST_LED, out);
           break;
    }
}
