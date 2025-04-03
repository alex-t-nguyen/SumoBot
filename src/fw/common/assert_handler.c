#include "assert_handler.h"

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
    // TODO: Blink LED indefinitely
    while(1) {};
}
