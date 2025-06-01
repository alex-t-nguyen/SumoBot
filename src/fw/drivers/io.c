#include "drivers/io.h"
#include "common/assert_handler.h"
#include "common/defines.h"
#include "common/trace.h"
#include <assert.h>
#include <msp430.h>
#include <stddef.h>
#include <stdint.h>

#define IO_PORT_CNT (8U)           // Number of ports in MSP430F5529
#define IO_PINS_PER_PORT_CNT (8U)  // Number of pins per port in MSP430F5529
#define IO_PORT_OFFSET (3U)        // Port number = bits 3-5
#define IO_PORT_MASK (7U)          // 3'b111
#define IO_PIN_MASK (7U)           // 3'b111
#define IO_INTERRUPT_PORT_CNT (2U) // Number of interrupt ports
// Unused IO pins set to either GPIO output, or pullup/down input
// Decided to use pulldown input
#define IO_UNUSED_CONFIG                                                       \
    { IO_SEL_GPIO, IO_DIR_INPUT, IO_REN_ENABLE, IO_OUT_LOW }

// With -fshort-enums compiler flag, enums are set to 1 byte instead of 2 bytes
// io_generic_enum should not > 1 byte (8 bits) since all the values are between
// 0-255 (8 bits)
static_assert(sizeof(io_generic_enum) == 1,
              "Unexpected size, -fshort-enums missing?");

static inline uint8_t calc_io_port(io_signal_enum signal) {
    return (signal >> IO_PORT_OFFSET) & (IO_PORT_MASK);
}

static inline uint8_t calc_io_pin_index(io_signal_enum signal) {
    return signal & IO_PIN_MASK;
}

static inline uint8_t calc_io_pin(io_signal_enum signal) {
    return 1 << calc_io_pin_index(signal);
}

typedef enum {
    IO_PORT1,
    IO_PORT2,
    IO_PORT3,
    IO_PORT4,
    IO_PORT5,
    IO_PORT6,
    IO_PORT7,
    IO_PORT8
} io_port_enum;

// Store address of memory-mapped registers to be accessed via array
// If you were to store the value P1DIR, it would no longer be memory mapped
// when accessing through the array because the array just copies the value of
// P1DIR (a hex address) and places it somewhere else in memory. But accessing
// P1DIR outside of the array will still be memory mapped
static volatile uint8_t *const port_sel_regs[IO_PORT_CNT] = {
    &P1SEL, &P2SEL, &P3SEL, &P4SEL, &P5SEL, &P6SEL, &P7SEL, &P8SEL};
static volatile uint8_t *const port_dir_regs[IO_PORT_CNT] = {
    &P1DIR, &P2DIR, &P3DIR, &P4DIR, &P5DIR, &P6DIR, &P7DIR, &P8DIR};
static volatile uint8_t *const port_ren_regs[IO_PORT_CNT] = {
    &P1REN, &P2REN, &P3REN, &P4REN, &P5REN, &P6REN, &P7REN, &P8REN};
static volatile uint8_t *const port_out_regs[IO_PORT_CNT] = {
    &P1OUT, &P2OUT, &P3OUT, &P4OUT, &P5OUT, &P6OUT, &P7OUT, &P8OUT};
static volatile uint8_t *const port_in_regs[IO_PORT_CNT] = {
    &P1IN, &P2IN, &P3IN, &P4IN, &P5IN, &P6IN, &P7IN, &P8IN};

static volatile uint8_t *const port_interrupt_flag_regs[IO_INTERRUPT_PORT_CNT] =
    {&P1IFG, &P2IFG};
static volatile uint8_t *const
    port_interrupt_edge_select_regs[IO_INTERRUPT_PORT_CNT] = {&P1IES, &P2IES};
static volatile uint8_t
    *const port_interrupt_enable_regs[IO_INTERRUPT_PORT_CNT] = {&P1IE, &P2IE};

static isr_function isr_functions[IO_INTERRUPT_PORT_CNT][IO_PINS_PER_PORT_CNT] =
    {[IO_PORT1] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
     [IO_PORT2] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}};

static const io_signal_enum io_adc_pins_arr[] = {LD_FRONT_LEFT, LD_FRONT_RIGHT,
                                                 LD_BACK_LEFT, LD_BACK_RIGHT};

typedef enum { HW_TYPE_LAUNCHPAD, HW_TYPE_SUMOBOT } hw_type_enum;

static hw_type_enum io_detect_hw_type(void) {
    const struct io_config hw_type_config = {.io_sel = IO_SEL_GPIO,
                                             .io_dir = IO_DIR_INPUT,
                                             .io_ren = IO_REN_ENABLE,
                                             .io_out = IO_OUT_LOW};
    config_io(DETECT_HW_TYPE_PIN, &hw_type_config);

    // Read pin value
    // 1. Get port and pin number
    // 2. Get input value at pin and shift down to 1 bit to check 1 or 0
    uint8_t port = calc_io_port(DETECT_HW_TYPE_PIN);
    uint8_t pin = calc_io_pin(DETECT_HW_TYPE_PIN);
    uint8_t pin_index = calc_io_pin_index(DETECT_HW_TYPE_PIN);
    return ((*port_in_regs[port] &= pin) >> pin_index) ? HW_TYPE_SUMOBOT
                                                       : HW_TYPE_LAUNCHPAD;
}

static const struct io_config io_initial_configs[IO_PORT_CNT *
                                                 IO_PINS_PER_PORT_CNT] = {
    // Detect HW Type pin
    [DETECT_HW_TYPE_PIN] = {IO_SEL_GPIO, IO_DIR_INPUT, IO_REN_DISABLE,
                            IO_OUT_LOW},

    // Test LED for BlinkLED program
    [IO_TEST_LED] = {IO_SEL_GPIO, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},

    // Motor pins
    [MOTOR_RIGHT_IN1] = {IO_SEL_ALT2, IO_DIR_OUTPUT, IO_REN_DISABLE,
                         IO_OUT_LOW},
    [MOTOR_RIGHT_IN2] = {IO_SEL_ALT2, IO_DIR_OUTPUT, IO_REN_DISABLE,
                         IO_OUT_LOW},
    [MOTOR_LEFT_IN1] = {IO_SEL_ALT2, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [MOTOR_LEFT_IN2] = {IO_SEL_ALT2, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [MOTOR_ENABLE] = {IO_SEL_GPIO, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [MOTOR_RIGHT_NFAULT] = {IO_SEL_GPIO, IO_DIR_INPUT, IO_REN_ENABLE,
                            IO_OUT_HIGH},
    [MOTOR_LEFT_NFAULT] = {IO_SEL_GPIO, IO_DIR_INPUT, IO_REN_ENABLE,
                           IO_OUT_HIGH},

    // Laster Range Sensor pins
    [XSHUT_RIGHT] = {IO_SEL_GPIO, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [XSHUT_MIDDLE] = {IO_SEL_GPIO, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [XSHUT_LEFT] = {IO_SEL_GPIO, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [I2C_SDA] = {IO_SEL_ALT1, IO_DIR_OUTPUT, IO_REN_ENABLE, IO_OUT_HIGH},
    [I2C_SCL] = {IO_SEL_ALT1, IO_DIR_OUTPUT, IO_REN_ENABLE, IO_OUT_HIGH},
    //-- Range sensor is open drain and should be pulled up by external pullup
    // resistor on PCB instead
    [RANGE_INTERRUPT_RIGHT] = {IO_SEL_GPIO, IO_DIR_INPUT, IO_REN_ENABLE,
                               IO_OUT_LOW},
    [RANGE_INTERRUPT_MIDDLE] = {IO_SEL_GPIO, IO_DIR_INPUT, IO_REN_ENABLE,
                                IO_OUT_LOW},
    [RANGE_INTERRUPT_LEFT] = {IO_SEL_GPIO, IO_DIR_INPUT, IO_REN_ENABLE,
                              IO_OUT_LOW},

    // Line Detector Sensor pins
    [LD_FRONT_LEFT] = {IO_SEL_ALT1, IO_DIR_INPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [LD_FRONT_RIGHT] = {IO_SEL_ALT1, IO_DIR_INPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [LD_BACK_LEFT] = {IO_SEL_ALT1, IO_DIR_INPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [LD_BACK_RIGHT] = {IO_SEL_ALT1, IO_DIR_INPUT, IO_REN_DISABLE, IO_OUT_LOW},

    // UART (Programming MCU)
    // Pullup resistor provided by external device instead
    [UART_TX] = {IO_SEL_ALT1, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_HIGH},
    [UART_RX] = {IO_SEL_ALT1, IO_DIR_INPUT, IO_REN_DISABLE, IO_OUT_HIGH},
#ifdef LAUNCHPAD
    // Unused IO Pins
    [IO_UNUSED_11] = IO_UNUSED_CONFIG,
    [IO_UNUSED_17] = IO_UNUSED_CONFIG,
    [IO_UNUSED_20] = IO_UNUSED_CONFIG,
    [IO_UNUSED_21] = IO_UNUSED_CONFIG,
    [IO_UNUSED_22] = IO_UNUSED_CONFIG,
    [IO_UNUSED_27] = IO_UNUSED_CONFIG,
    [IO_UNUSED_30] = IO_UNUSED_CONFIG,
    [IO_UNUSED_31] = IO_UNUSED_CONFIG,
    [IO_UNUSED_32] = IO_UNUSED_CONFIG,
    [IO_UNUSED_33] = IO_UNUSED_CONFIG,
    [IO_UNUSED_34] = IO_UNUSED_CONFIG,
    [IO_UNUSED_35] = IO_UNUSED_CONFIG,
    [IO_UNUSED_36] = IO_UNUSED_CONFIG,
    //[IO_UNUSED_37] = IO_UNUSED_CONFIG,
    [IO_UNUSED_40] = IO_UNUSED_CONFIG,
    //[IO_UNUSED_43] = IO_UNUSED_CONFIG,
    //[IO_UNUSED_44] = IO_UNUSED_CONFIG,
    //[IO_UNUSED_45] = IO_UNUSED_CONFIG,
    [IO_UNUSED_46] = IO_UNUSED_CONFIG,
    [IO_UNUSED_47] = IO_UNUSED_CONFIG,
    [IO_UNUSED_50] = IO_UNUSED_CONFIG,
    [IO_UNUSED_51] = IO_UNUSED_CONFIG,
    [IO_UNUSED_52] = IO_UNUSED_CONFIG,
    [IO_UNUSED_53] = IO_UNUSED_CONFIG,
    [IO_UNUSED_54] = IO_UNUSED_CONFIG,
    [IO_UNUSED_55] = IO_UNUSED_CONFIG,
    [IO_UNUSED_56] = IO_UNUSED_CONFIG,
    [IO_UNUSED_57] = IO_UNUSED_CONFIG,
    [IO_UNUSED_60] = IO_UNUSED_CONFIG,
    [IO_UNUSED_65] = IO_UNUSED_CONFIG,
    [IO_UNUSED_66] = IO_UNUSED_CONFIG,
    [IO_UNUSED_67] = IO_UNUSED_CONFIG,
    [IO_UNUSED_70] = IO_UNUSED_CONFIG,
    [IO_UNUSED_71] = IO_UNUSED_CONFIG,
    [IO_UNUSED_72] = IO_UNUSED_CONFIG,
    [IO_UNUSED_73] = IO_UNUSED_CONFIG,
    //[IO_UNUSED_74] = IO_UNUSED_CONFIG,
    [IO_UNUSED_75] = IO_UNUSED_CONFIG,
    [IO_UNUSED_76] = IO_UNUSED_CONFIG,
    [IO_UNUSED_77] = IO_UNUSED_CONFIG,
    [IO_UNUSED_80] = IO_UNUSED_CONFIG,
//[IO_UNUSED_82] = IO_UNUSED_CONFIG
#elif SUMOBOT
    // Unused IO Pins
    [IO_UNUSED_11] = IO_UNUSED_CONFIG,
    [IO_UNUSED_17] = IO_UNUSED_CONFIG,
    [IO_UNUSED_20] = IO_UNUSED_CONFIG,
    [IO_UNUSED_21] = IO_UNUSED_CONFIG,
    [IO_UNUSED_22] = IO_UNUSED_CONFIG,
    [IO_UNUSED_27] = IO_UNUSED_CONFIG,
    [IO_UNUSED_30] = IO_UNUSED_CONFIG,
    [IO_UNUSED_31] = IO_UNUSED_CONFIG,
    [IO_UNUSED_32] = IO_UNUSED_CONFIG,
    [IO_UNUSED_35] = IO_UNUSED_CONFIG,
    [IO_UNUSED_36] = IO_UNUSED_CONFIG,
    //[IO_UNUSED_37] = IO_UNUSED_CONFIG,
    [IO_UNUSED_40] = IO_UNUSED_CONFIG,
    //[IO_UNUSED_43] = IO_UNUSED_CONFIG,
    [IO_UNUSED_44] = IO_UNUSED_CONFIG,
    [IO_UNUSED_45] = IO_UNUSED_CONFIG,
    [IO_UNUSED_46] = IO_UNUSED_CONFIG,
    [IO_UNUSED_47] = IO_UNUSED_CONFIG,
    [IO_UNUSED_50] = IO_UNUSED_CONFIG,
    [IO_UNUSED_51] = IO_UNUSED_CONFIG,
    [IO_UNUSED_52] = IO_UNUSED_CONFIG,
    [IO_UNUSED_53] = IO_UNUSED_CONFIG,
    [IO_UNUSED_54] = IO_UNUSED_CONFIG,
    [IO_UNUSED_55] = IO_UNUSED_CONFIG,
    [IO_UNUSED_56] = IO_UNUSED_CONFIG,
    [IO_UNUSED_57] = IO_UNUSED_CONFIG,
    [IO_UNUSED_60] = IO_UNUSED_CONFIG,
    [IO_UNUSED_65] = IO_UNUSED_CONFIG,
    [IO_UNUSED_66] = IO_UNUSED_CONFIG,
    [IO_UNUSED_67] = IO_UNUSED_CONFIG,
    [IO_UNUSED_70] = IO_UNUSED_CONFIG,
    [IO_UNUSED_71] = IO_UNUSED_CONFIG,
    [IO_UNUSED_72] = IO_UNUSED_CONFIG,
    [IO_UNUSED_73] = IO_UNUSED_CONFIG,
    //[IO_UNUSED_74] = IO_UNUSED_CONFIG,
    [IO_UNUSED_75] = IO_UNUSED_CONFIG,
    [IO_UNUSED_76] = IO_UNUSED_CONFIG,
    [IO_UNUSED_77] = IO_UNUSED_CONFIG,
    [IO_UNUSED_80] = IO_UNUSED_CONFIG,
//[IO_UNUSED_82] = IO_UNUSED_CONFIG
#endif
};

void io_set_sel(io_signal_enum signal, io_sel_enum select) {
    const uint8_t port = calc_io_port(signal);
    const uint8_t pin = calc_io_pin(signal);

    switch (select) {
    case IO_SEL_GPIO:
        *port_sel_regs[port] &= ~pin;
        break;
    case IO_SEL_ALT1:
    case IO_SEL_ALT2:
        *port_sel_regs[port] |= pin;
        break;
    }
}

void io_set_dir(io_signal_enum signal, io_dir_enum dir) {
    const uint8_t port = calc_io_port(signal);
    const uint8_t pin = calc_io_pin(signal);

    switch (dir) {
    case IO_DIR_INPUT:
        *port_dir_regs[port] &= ~pin;
        break;
    case IO_DIR_OUTPUT:
        *port_dir_regs[port] |= pin;
        break;
    }
}

void io_set_ren(io_signal_enum signal, io_ren_enum enable) {
    const uint8_t port = calc_io_port(signal);
    const uint8_t pin = calc_io_pin(signal);

    switch (enable) {
    case IO_REN_DISABLE:
        *port_ren_regs[port] &= ~pin;
        break;
    case IO_REN_ENABLE:
        *port_ren_regs[port] |= pin;
        break;
    }
}

void io_set_out(io_signal_enum signal, io_out_enum out) {
    const uint8_t port = calc_io_port(signal);
    const uint8_t pin = calc_io_pin(signal);

    switch (out) {
    case IO_OUT_LOW:
        *port_out_regs[port] &= ~pin;
        break;
    case IO_OUT_HIGH:
        *port_out_regs[port] |= pin;
        break;
    }
}

void config_io(io_signal_enum signal, const struct io_config *config) {
    io_set_sel(signal, config->io_sel);
    io_set_dir(signal, config->io_dir);
    io_set_out(signal, config->io_out);
    io_set_ren(signal, config->io_ren);
}

void io_init(void) {
#if defined(SUMOBOT)
    ASSERT(io_detect_hw_type() == HW_TYPE_SUMOBOT)
#elif defined(LAUNCHPAD)
    ASSERT(io_detect_hw_type() == HW_TYPE_LAUNCHPAD)
#else
    ASSERT(0)
#endif
    for (io_signal_enum pin = (io_signal_enum)IO_10;
         pin < ARRAY_SIZE(io_initial_configs); pin++) {
        config_io(pin, &io_initial_configs[pin]);
    }
}

/**
 * Get the current IO configuration of a given signal/pin
 * NOTE: IO_SEL can be 0,1, or 2 if the port is Port1 or Port2
 *       This is accounted for in the function.
 */
void io_get_current_config(io_signal_enum signal, struct io_config *config) {
    const uint8_t port = calc_io_port(signal);
    const uint8_t pin = calc_io_pin(signal);
    const uint8_t pin_index = calc_io_pin_index(signal);
    config->io_dir = (io_dir_enum)((*port_dir_regs[port] & pin) >> pin_index);
    config->io_ren = (io_ren_enum)((*port_ren_regs[port] & pin) >> pin_index);
    config->io_out = (io_out_enum)((*port_out_regs[port] & pin) >> pin_index);
    if (port == 0 || port == 1) { // If port == 0 or 1 (corresponds to P1 and P2
                                  // on MCU), then IO_SEL can be 0/1/2
        io_sel_enum io_sel_temp;
        io_sel_temp = (io_sel_enum)((*port_sel_regs[port] & pin) >> pin_index);
        if (io_sel_temp == 1) {                  // If IO_SEL bit is set to 1
            if (config->io_dir == 0) {           // If direction is 0
                config->io_sel = (io_sel_enum)1; // IO_SEL is actually = 1
            } else if (config->io_dir == 1) {
                config->io_sel = (io_sel_enum)2; // IO_SEL is actually = 2
            }
        } else { // Set IO_SEL = 0
            config->io_sel = (io_sel_enum)0;
        }
    } else { // IO_SEL will only be 0/1
        config->io_sel =
            (io_sel_enum)((*port_sel_regs[port] & pin) >> pin_index);
    }
}

bool io_config_compare(const struct io_config *cfg1,
                       const struct io_config *cfg2) {
    return cfg1->io_sel == cfg2->io_sel && cfg1->io_dir == cfg2->io_dir &&
           cfg1->io_ren == cfg2->io_ren && cfg1->io_out == cfg2->io_out;
}

static void io_clear_interrupt(io_signal_enum signal) {
    /* Clear interrupt flag */
    *port_interrupt_flag_regs[calc_io_port(signal)] &= ~calc_io_pin(signal);
}

/* This function also disables the interrupt because selecting the interrupt
 * edge can also trigger an interrupt according to the datasheet.
 */
static void io_set_interrupt_trigger(io_signal_enum signal,
                                     io_trigger_enum trigger) {
    const uint8_t port = calc_io_port(signal);
    const uint8_t pin = calc_io_pin(signal);
    io_disable_interrupt(signal);
    switch (trigger) {
    case IO_TRIGGER_RISING:
        *port_interrupt_edge_select_regs[port] &= ~pin;
        break;
    case IO_TRIGGER_FALLING:
        *port_interrupt_edge_select_regs[port] |= pin;
        break;
    }
    io_clear_interrupt(signal); // Disabling interrupt doesn't prevent interrupt
                                // flag from getting set
}

static void io_register_isr(io_signal_enum signal, isr_function isr) {
    const uint8_t port = calc_io_port(signal);
    const uint8_t pin_index = calc_io_pin_index(signal);
    ASSERT(!isr_functions[port][pin_index]);
    isr_functions[port][pin_index] = isr;
}

void io_configure_interrupt(io_signal_enum signal, io_trigger_enum trigger,
                            isr_function isr) {
    io_set_interrupt_trigger(signal, trigger);
    io_register_isr(signal, isr);
}

static inline void io_unregister_isr(io_signal_enum signal) {
    const uint8_t port = calc_io_port(signal);
    const uint8_t pin_index = calc_io_pin_index(signal);
    isr_functions[port][pin_index] = NULL;
}

void io_deconfigure_interrupt(io_signal_enum signal) {
    io_unregister_isr(signal);
    io_disable_interrupt(signal);
}

void io_enable_interrupt(io_signal_enum signal) {
    *port_interrupt_enable_regs[calc_io_port(signal)] |= calc_io_pin(signal);
}

void io_disable_interrupt(io_signal_enum signal) {
    *port_interrupt_enable_regs[calc_io_port(signal)] &= ~calc_io_pin(signal);
}

static void io_isr(io_signal_enum io) {
    const uint8_t port = calc_io_port(io);
    const uint8_t pin_index = calc_io_pin_index(io);
    const uint8_t pin = calc_io_pin(io);
    if (*port_interrupt_flag_regs[port] & pin) {
        if (isr_functions[port][pin_index]) {
            isr_functions[port][pin_index]();
        } else
            ASSERT(!isr_functions[port][pin_index])
    }
    io_clear_interrupt(io); // Clear interrupt flag performing ISR action
}

/**
 * Reads value of io pin
 * Pin must be an input
 */
bool io_read_input(io_signal_enum io) {
    uint8_t port = calc_io_port(io);
    uint8_t pin = calc_io_pin(io);
    uint8_t pin_idx = calc_io_pin_index(io);

    TRACE("Reading input pin P%d.%d...", port, pin);
    return ((*port_in_regs[port] & pin) >> pin_idx) ? true : false;
}

/**
 * Returns array of adc pins
 * Sets cnt variable to size of array (pass by reference)
 */
const io_signal_enum *get_io_adc_pins(uint8_t *cnt) {
    *cnt = ARRAY_SIZE(io_adc_pins_arr);
    return io_adc_pins_arr;
}

/**
 * Returns the index of the adc IO pin
 */
uint8_t io_to_adc_idx(io_signal_enum io) {
    ASSERT((calc_io_port(io) == IO_PORT6) || (calc_io_port(io) == IO_PORT7));
    return calc_io_pin_index(io);
}

INTERRUPT_FUNCTION(PORT1_VECTOR) isr_port_1(void) {
    for (io_generic_enum io = IO_10; io <= IO_17; io++) {
        io_isr(io);
    }
}

INTERRUPT_FUNCTION(PORT2_VECTOR) isr_port_2(void) {
    for (io_generic_enum io = IO_20; io <= IO_27; io++) {
        io_isr(io);
    }
}
