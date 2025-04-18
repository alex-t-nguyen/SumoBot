#include "common/assert_handler.h"
#include "drivers/io.h"
#include <msp430.h>

static void init_clocks() {
    // Step 1: Disable the FLL control loop
    __bis_SR_register(SCG0); // Disable FLL

    // Step 2: Configure the FLL
    UCSCTL0 = 0x0000; // Set DCOx = 0 and MODx = 0
    /* When selecting the proper DCO frequency range (DCORSELx), the target DCO
     * frequency, fDCO, should be set to reside within the range of fDCO(n,
     * 0),MAX ≤ fDCO ≤ fDCO(n, 31),MIN, where fDCO(n, 0),MAX represents the
     * maximum frequency specified for the DCO frequency, range n, tap 0 (DCOx =
     * 0) and fDCO(n,31),MIN represents the minimum frequency specified for the
     * DCO frequency, range n, tap 31 (DCOx = 31).
     */
    // Set range of DCO_CLK
    // Desired DCO_CLK = 16MHz
    UCSCTL1 = DCORSEL_5; // Select DCO range up to between 6MHz (fDCO(n,0),MAX
                         // and 23.7MHz (fDCO(n,31),MIN

    // DCOCLK = D × (N + 1) × (fFLLREFCLK ÷ n)
    // D = FLL prescaler divider value (1, 2, 4, 8, 16, or 32). By default, D =
    // 2, N = The divider (N + 1) can be set using the FLLN bits, where N > 0.
    // fFLLREFCLK can be XT1CLK, REFOCLK, XT2CLK. Select XT1CLK by default
    // (default is 32.768kHz). Controlled by SELREF n = The value of n is
    // defined by the FLLREFDIV bits (n = 1, 2, 4, 8, 12, or 16). The default is
    // n = 1.
    UCSCTL3 = SELREF__REFOCLK;    // fFLLREFCLK = REFOCLK -> 32.768kHz (The
                                  // XT1/2CLK requires a crystal)
    UCSCTL6 |= (XT1OFF | XT2OFF); // Turn off XT1 and XT2 because not using any
                                  // external crystal to provide these
    UCSCTL2 = FLLD_0 + 487; // FLLD = 1, N = 487 -> 32.768kHz * 487 = ~16MHz (N
                            // = UCSCTL2[9:0] or 10bits == 1024)

    // Step 3: Enable the FLL control loop
    __bic_SR_register(SCG0); // Enable FLL

    // Step 4: Wait for the DCO to settle
    // (n × 32 × 32) fFLLREFCLK cycles for the DCO to settle worse case (1 * 32
    // * 32 = 1024)
    __delay_cycles(5000); // Delay for DCO stabilization

    // Step 5: Select DCO as clock source for MCLK and SCLK, and REFO as clock
    // source for ACLK MUST select REFOCLK for ACLK (SELA) BEFORE clearing
    // XT1CLK fault flag because the default for it is XT1CLK and it will
    // continuously set XT1 fault flag if XT1 is not enabled and present on
    // device
    UCSCTL4 = SELM__DCOCLK + SELS__DCOCLK +
              SELA__REFOCLK; // MCLK & SMCLK = DCO, ACLK = REFO

    // Step 6: Clear oscillator fault flags
    do {
        UCSCTL7 &= ~(DCOFFG | XT1LFOFFG | XT2OFFG); // Clear fault flags
        SFRIFG1 &= ~OFIFG;                          // Clear oscillator fault
    } while (SFRIFG1 & OFIFG);                      // Loop until stable
}

void mcu_init(void) {
    WDTCTL =
        WDTPW |
        WDTHOLD; // stop watchdog timer, otherwise it prevents program from
                 // running because it keeps restarting from watchdog timeout
    init_clocks();
    io_init();
    // Enable interrupts globally
    __enable_interrupt(); // Call function from TI
}
