#include <msp430.h>

void mcu_init(void) {
    WDTCTL =
        WDTPW |
        WDTHOLD; // stop watchdog timer, otherwise it prevents program from
                 // running because it keeps restarting from watchdog timeout
}
