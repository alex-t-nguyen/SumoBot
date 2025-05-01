#define SUPPRESS_UNUSED __attribute__((unused))

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define INTERRUPT_FUNCTION(vector) void __attribute__((interrupt(vector)))

#define CYCLES_1MHZ (1000000U)
#define CYCLES_16MHZ (16U * CYCLES_1MHZ)
#define CYCLES_PER_MS (CYCLES_16MHZ / 1000U)
#define ms_TO_CYCLES(ms) (CYCLES_PER_MS * ms)
#define BUSY_WAIT_ms(ms) __delay_cycles(ms_TO_CYCLES(ms));

#define SMCLK (CYCLES_16MHZ)
#define TIMER_INPUT_DIVIER_3 (8U)
