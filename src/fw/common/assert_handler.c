#include "assert_handler.h"
#include "common/defines.h"
#include <msp430.h>

/* The TI compiler provides intrinsic support for calling a specific opcode,
 * which means you can write __op_code(0x4343) to trigger a software breakpoint
 * (When LAUNCHPAD FET debugger is attached). MSP430-GCC does not have this
 * intrinsic function, but 0x4343 corresponds to assumly instruction "CLR.B R3".
 */
#define BREAKPOINT __asm volatile("CLR.B R3");

void assert_handler(void) {
    // TODO: Turn off motors ("safe state")
    // TODO: Trace to console
    BREAKPOINT
    
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
        BUSY_WAIT_ms(1000)
    };
}
