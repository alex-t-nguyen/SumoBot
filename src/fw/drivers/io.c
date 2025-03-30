#include "drivers/io.h"
#include "common/defines.h"
#include <msp430.h>
#include <stdint.h>

#define IO_PORT_CNT (8U)          // Number of ports in MSP430F5529
#define IO_PINS_PER_PORT_CNT (8U) // Number of pins per port in MSP430F5529
#define IO_PORT_OFFSET (3U)       // Port number = bits 3-5
#define IO_PORT_MASK (7U)         // 3'b111
#define IO_PIN_MASK (7U)          // 3'b111

// Unused IO pins set to either GPIO output, or pullup/down input
// Decided to use pulldown input
#define IO_UNUSED_CONFIG                                                       \
    { IO_SEL_GPIO, IO_DIR_INPUT, IO_REN_ENABLE, IO_OUT_LOW }

static inline uint8_t calc_io_port(io_signal_enum signal) {
    return (signal >> IO_PORT_OFFSET) & (IO_PORT_MASK);
}

static inline uint8_t calc_io_pin_index(io_signal_enum signal) {
    return signal & IO_PIN_MASK;
}

static inline uint8_t calc_io_pin(io_signal_enum signal) {
    return 1 << calc_io_pin_index(signal);
}

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

static const struct io_config io_initial_configs[IO_PORT_CNT *
                                                 IO_PINS_PER_PORT_CNT] = {
    // Test LED for BlinkLED program
    [IO_TEST_LED] = {IO_SEL_GPIO, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},

    // Motor pins
    [MOTOR_RIGHT_IN1] = {IO_SEL_ALT2, IO_DIR_OUTPUT, IO_REN_DISABLE,
                         IO_OUT_LOW},
    [MOTOR_RIGHT_IN2] = {IO_SEL_ALT2, IO_DIR_OUTPUT, IO_REN_DISABLE,
                         IO_OUT_LOW},
    [MOTOR_LEFT_IN1] = {IO_SEL_ALT2, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [MOTOR_LEFT_IN2] = {IO_SEL_ALT2, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},

    // Laster Range Sensor pins
    [XSHUT_RIGHT] = {IO_SEL_GPIO, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [XSHUT_MIDDLE] = {IO_SEL_GPIO, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [XSHUT_LEFT] = {IO_SEL_GPIO, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_LOW},
    [I2C_SDA] = {IO_SEL_ALT1, IO_DIR_OUTPUT, IO_REN_ENABLE, IO_OUT_HIGH},
    [I2C_SCL] = {IO_SEL_ALT1, IO_DIR_OUTPUT, IO_REN_ENABLE, IO_OUT_HIGH},
    //-- Range sensor is open drain and should be pulled up by external pullup
    //resistor on PCB instead
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
    [UART_RX] = {IO_SEL_ALT1, IO_DIR_OUTPUT, IO_REN_DISABLE, IO_OUT_HIGH},

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
    [IO_UNUSED_37] = IO_UNUSED_CONFIG,
    [IO_UNUSED_40] = IO_UNUSED_CONFIG,
    [IO_UNUSED_43] = IO_UNUSED_CONFIG,
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
    [IO_UNUSED_74] = IO_UNUSED_CONFIG,
    [IO_UNUSED_75] = IO_UNUSED_CONFIG,
    [IO_UNUSED_76] = IO_UNUSED_CONFIG,
    [IO_UNUSED_77] = IO_UNUSED_CONFIG,
    [IO_UNUSED_80] = IO_UNUSED_CONFIG,
    [IO_UNUSED_82] = IO_UNUSED_CONFIG};

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
    io_set_ren(signal, config->io_ren);
    io_set_out(signal, config->io_out);
}

void io_init(void) {
    for (io_signal_enum pin = (io_signal_enum)IO_10;
         pin < ARRAY_SIZE(io_initial_configs); pin++) {
        config_io(pin, &io_initial_configs[pin]);
    }
}
