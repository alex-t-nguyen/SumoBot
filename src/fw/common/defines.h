#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define CYCLES_1MHZ (1000000U)
#define ms_TO_CYCLES(ms) (CYCLES_1MHZ / 1000U) * ms
#define BUSY_WAIT_ms(ms) __delay_cycles(ms_TO_CYCLES(ms));
