#include "assert_handler.h"
#include "drivers/uart.h"
#include "common/defines.h"
#include "externals/printf/printf.h"
#include <msp430.h>

/* The TI compiler provides intrinsic support for calling a specific opcode,
 * which means you can write __op_code(0x4343) to trigger a software breakpoint
 * (When LAUNCHPAD FET debugger is attached). MSP430-GCC does not have this
 * intrinsic function, but 0x4343 corresponds to assumly instruction "CLR.B R3".
 */
#define BREAKPOINT __asm volatile("CLR.B R3");

// Text + Program Counter + Null termination
#define ASSERT_STRING_MAX_SIZE (15u + 6u + 1u)

/** 
 * Print trace of where ASSERT was triggered
 */
void assert_trace(uint16_t program_counter) {
//    #ifdef LAUNCHPAD
//        P4SEL |= (BIT4 | BIT5);
//        P4DIR |= BIT4;
//        P4DIR &= ~BIT5;
//    #elif SUMOBOT
//        P3SEL |= (BIT3 | BIT4);
//        P3DIR |= BIT3;
//        P3DIR &= ~BIT4;
//    #endif
    uart_init_assert(); // Selecting UART function on pins already handled in mcu_init() via io_init()
    char assert_str[ASSERT_STRING_MAX_SIZE];
    snprintf(assert_str, sizeof(assert_str), "ASSERT 0x%x\n", program_counter);
    uart_trace_assert(assert_str);
}

void assert_blink_led(void) {
    /* Manually blink LED here to avoid recursive assert possibly happening
     * because led_init() and set_led() use asserts
     */
    // Configure IO_TEST_LED pin
    P1SEL &= ~(BIT0);
    P1DIR |= (BIT0);
    P1REN &= ~(BIT0);
    P1OUT &= ~(BIT0);

    while (1) {
        P1OUT ^= BIT0;
        //__delay_cycles(250000);
        BUSY_WAIT_ms(250) // Blink 2x in 1 second (flash ON 2x in 1 second)
    };
}

void assert_handler(uint16_t program_counter) {
    // TODO: Turn off motors ("safe state")
    
    BREAKPOINT
    assert_trace(program_counter);
    assert_blink_led();
}
