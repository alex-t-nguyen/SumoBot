
/* IO pin handling, including pinmapping, initialization, and configurations
 * This wraps the register defines provided in the headers from Texas
 * Instruments */
#define LAUNCHPAD

typedef enum {
// IO_xx -> (Port, Pin bit position)
#ifdef LAUNCHPAD
    // Port 1, bit positions 0-7
    IO_10, // 0 (counts up from 0...)
    IO_11, // 1
    IO_12, // 2
    IO_13, // 3
    IO_14, // 4
    IO_15, // 5
    IO_16, // 6
    IO_17, // 7

    // Port 2, bit positions 0-7
    IO_20, // 0
    IO_21, // 1
    IO_22, // 2
    IO_23, // 3
    IO_24, // 4
    IO_25, // 5
    IO_26, // 6
    IO_27, // 7

    // Port 3, bit positions 0-7
    IO_30, // 0
    IO_31, // 1
    IO_32, // 2
    IO_33, // 3
    IO_34, // 4
    IO_35, // 5
    IO_36, // 6
    IO_37, // 7

    // Port 4, bit positions 0-7
    IO_40, // 0
    IO_41, // 1
    IO_42, // 2
    IO_43, // 3
    IO_44, // 4
    IO_45, // 5
    IO_46, // 6
    IO_47, // 7

    // Port 5, bit positions 0-7
    IO_50, // 0
    IO_51, // 1
    IO_52, // 2
    IO_53, // 3
    IO_54, // 4
    IO_55, // 5
    IO_56, // 6
    IO_57, // 7

    // Port 6, bit positions 0-7
    IO_60, // 0
    IO_61, // 1
    IO_62, // 2
    IO_63, // 3
    IO_64, // 4
    IO_65, // 5
    IO_66, // 6
    IO_67, // 7

    // Port 7, bit positions 0-7
    IO_70, // 0
    IO_71, // 1
    IO_72, // 2
    IO_73, // 3
    IO_74, // 4
    IO_75, // 5
    IO_76, // 6
    IO_77, // 7

    // Port 8, bit positions 0-1
    IO_80, // 0
    IO_81, // 1
    IO_82  // 2
#elif SUMO_BOT

#endif
} io_generic_enum;

typedef enum {
    IO_SEL_GPIO, // 0: Make pin a GPIO
    IO_SEL_ALT1, // 1: Select function 1 of pin
    IO_SEL_ALT2, // 2: Select function 2 of pin
} io_sel_enum;

typedef enum {
    IO_DIR_INPUT, // 0: input
    IO_DIR_OUTPUT // 1: output
} io_dir_enum;

typedef enum {
    IO_REN_DISABLE, // 0: disable
    IO_REN_ENABLE   // 1: enable
} io_ren_enum;

typedef enum {
    // Controls pin output state and whether pullup/pulldown if input pin
    IO_OUT_LOW, // 0: low/pull-down
    IO_OUT_HIGH // 1: high/pull-up
} io_out_enum;

struct io_config {
    io_sel_enum io_sel;
    io_dir_enum io_dir;
    io_ren_enum io_ren;
    io_out_enum io_out;
};

typedef enum {
#ifdef LAUNCHPAD // Launchpad (MSP430F5529)
    IO_TEST_LED = IO_10,
    // Motor pins (IN1 -> AIN1 and BIN1, IN2 -> AIN2 and BIN2) (Motors on same
    // sides will share same PWM input signal)
    MOTOR_RIGHT_IN1 = IO_25, // P2.5
    MOTOR_RIGHT_IN2 = IO_24, // P2.4
    MOTOR_LEFT_IN1 = IO_15,  // P1.5
    MOTOR_LEFT_IN2 = IO_14,  // P1.4

    // Laser Range Sensor pins
    //-- Reset pins
    XSHUT_RIGHT = IO_23,  // P2.3
    XSHUT_MIDDLE = IO_26, // P2.6
    XSHUT_LEFT = IO_81,   // P8.1
    //-- I2C pins
    I2C_SDA = IO_41, // P4.1
    I2C_SCL = IO_42, // P4.2
    //-- Interrupt pins (GPIO interrupt pins -> P1.x-P2.x)
    RANGE_INTERRUPT_RIGHT = IO_16,  // P1.6 (GPIO interrupt pin)
    RANGE_INTERRUPT_MIDDLE = IO_13, // P1.3 (GPIO interrupt pin)
    RANGE_INTERRUPT_LEFT = IO_12,   // P1.2 (GPIO interrupt pin)

    // Line Detector Sensor pins
    LD_FRONT_LEFT = IO_61,  // P6.1
    LD_FRONT_RIGHT = IO_62, // P6.2
    LD_BACK_LEFT = IO_63,   // P6.3
    LD_BACK_RIGHT = IO_64,  // P6.4

    // UART (Programming MCU)
    UART_TX = IO_33, // P3.3
    UART_RX = IO_34  // P3.4
#elif SUMO_BOT       // Actual sumo bot

#endif
} io_signal_enum;

void io_set_sel(io_signal_enum pin, io_sel_enum select);
void io_set_dir(io_signal_enum pin, io_dir_enum dir);
void io_set_ren(io_signal_enum pin, io_ren_enum enable);
void io_set_out(io_signal_enum pin, io_out_enum out);
void config_io(io_signal_enum pin, const struct io_config *config);
