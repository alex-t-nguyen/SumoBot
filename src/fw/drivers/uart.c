#include "drivers/uart.h"
#include "common/assert_handler.h"
#include "common/defines.h"
#include "drivers/io.h"
#include "drivers/ring_buffer.h"
#include <assert.h>
#include <msp430.h>
#include <stddef.h>
#include <stdint.h>

// UART ring buffer constants
#define UART_BUFFER_SIZE (16U)
// Initialize UART ring buffer
static struct ring_buffer *uart_ring_buffer = NULL;

// UART configuration constants
#define BRCLK (SMCLK)
#define UART_BAUDRATE (115200U)
static_assert(UART_BAUDRATE <= BRCLK / 16.0f,
              "UART baudrate must be <= 1/16 of UART clock source frequency "
              "BRCLK in oversampling mode.");

#define UART_DIV_FACTOR ((float)BRCLK / UART_BAUDRATE)
static_assert(UART_DIV_FACTOR < 0xFFFFU,
              "UART division factor must fit within 16 bits");
#define UART_PRESCALAR                                                         \
    ((uint16_t)(UART_DIV_FACTOR / 16U)) // UART prescalar is 16 bits and split
                                        // into 2 8-bit registers
#define UART_PRESCALAR_LO_BYTE (UART_PRESCALAR & 0xFFU) // UCAxBR0
#define UART_PRESCALAR_HI_BYTE (UART_PRESCALAR >> 8U)   // UCAxBR1
#define UART_MODULATOR                                                         \
    (uint8_t)((((float)UART_DIV_FACTOR / 16U) - UART_PRESCALAR) * 16U)
static_assert(UART_MODULATOR < 0x16U, "UART modulator must fit within 4 bits");
#define UART_UCBRFx (UART_MODULATOR)
#define UART_UCBRSx                                                            \
    (0U) // Calculated to 0 for lowest error rate based on table in User Guide
static_assert(UART_UCBRSx < 0x8U, "UART modulator must fit within 3 bits");
#define UART_UCOS16 (1U) // Enable oversampling

static void uart_tx_enable_interrupt(void) {
#ifdef LAUNCHPAD
    UCA1IE |= UCTXIE; // Enable UART TX interrupt
#elif SUMOBOT
    UCA0IE |= UCTXIE;  // Enable UART TX interrupt
#endif
}

static void uart_tx_disable_interrupt(void) {
#ifdef LAUNCHPAD
    UCA1IE &= ~UCTXIE; // Disable UART TX interrupt
#elif SUMOBOT
    UCA0IE &= ~UCTXIE; // Disable UART TX interrupt
#endif
}

// static void uart_tx_clear_ifg(void) {
//// Clear UART interrupt flag
//#ifdef LAUNCHPAD
//    UCA1IFG &= ~UCTXIFG;
//#elif SUMOBOT
//    UCA0IFG &= ~UCTXIFG;
//#endif
//}

/**
 * Initializes UART registers
 */
void uart_init(void) {
    uart_ring_buffer = ring_buffer_init(UART_BUFFER_SIZE);
#ifdef LAUNCHPAD // Launchpad uses P4.4/P4.5 for UART with ez-FET to USB module
    // Set to UART mode (default, but just explicitly setting here)
    UCA1CTL0 &= ~(UCPEN | UCPAR | UCMSB | UC7BIT | UCSPB | UCMODE0 | UCSYNC);

    // USCI logic held in reset state
    // To avoid unpredictable behavior, configure or
    // reconfigure the USCI_A module only when UCSWRST is set.
    // Certain UART registers can only be configured when UCSWRST=1
    UCA1CTL1 |= UCSWRST;

    // Select SMCLK as BRCLK clock source
    UCA1CTL1 |= UCSSEL__SMCLK;

    // Baudrate used determines division factor N
    // Baudrate = 115200
    // N = f_BRCLK/Baudrate -> 16MHz/115200 = 138.89
    // UCA0MCTL = [UCBRFx (4 bits) | UCBRSx (3 bits) | UCOS16 (1 bit)]
    UCA1MCTL |= (UART_UCBRFx << 4U) | (UART_UCBRSx << 1U) |
                (UCOS16); // Can use oversampling because N > 16

    UCA1BR0 |= UART_PRESCALAR_LO_BYTE;
    UCA1BR1 |= UART_PRESCALAR_HI_BYTE;

    // Clear reset to release uart module for operation
    UCA1CTL1 &= ~UCSWRST;

    //        UCA1IFG &= ~UCTXIFG; // Reset interrupt flag becuase it is set
    //        when TXBUF is empty, which happens on UART reset with ~UCSWRST

    //        UCA1IE |= UCTXIE; // Enable UART TX interrupt
#elif SUMOBOT // Actual PCB will use P3.3/P3.4 for UART
    // Set to UART mode (default, but just explicitly setting here)
    UCA0CTL0 &= ~(UPEN | UCPAR | UCMSB | UC7BIT | UCSPB | UCMODE0 | UCSYNC);

    // USCI logic held in reset state
    // To avoid unpredictable behavior, configure or
    // reconfigure the USCI_A module only when UCSWRST is set.
    // Certain UART registers can only be configured when UCSWRST=1
    UCA0CTL1 |= UCSWRST;

    // Select SMCLK as BRCLK clock source
    UCA0CTL1 |= UCSSEL__SMCLK;

    // Baudrate used determines division factor N
    // Baudrate = 115200
    // N = f_BRCLK/Baudrate -> 16MHz/115200 = 138.89
    // UCA0MCTL = [UCBRFx (4 bits) | UCBRSx (3 bits) | UCOS16 (1 bit)]
    UCA0MCTL |= (UART_UCBRFx << 4U) | (UART_UCBRSx << 1U) |
                (UCOS16); // Can use oversampling because N > 16

    UCA0BR0 |= UART_PRESCALAR_LO_BYTE;
    UCA0BR1 |= UART_PRESCALAR_HI_BYTE;

    // Clear reset to release uart module for operation
    UCA0CTL1 &= ~UCSWRST;

    //        UCA0IFG &= ~UCTXIFG; // Reset interrupt flag becuase it is set
    //        when TXBUF is empty, which happens on UART reset with ~UCSWRST

    //        UCA0IE |= UCTXIE; // Enable UART TX interrupt
#endif
    // 1. Don't manually clear IFG here because it does not get auto reset by
    // device even though
    //    TXBUF is empty after exiting reset mode. If clearing the IFG here, you
    //    must write something to TXBUF before using the
    //    uart_polling/interrupt/putchar() functions.
    // 2. Don't enable interrupts here, otherwise the ISR will be called
    // immediately after exiting
    //    reset mode and if the IFG is set, it will trigger UART tx_start().
    //    Initially the IFG was cleared here to prevent this, but then it did
    //    not get auto-set again, so the uart_polling() did not work.
    // 3. Tested uart_polling/interrupt/putchar() with these commented out and
    // it works still. The ISR is
    //    enabled when calling the uart_putchar_interrupt() function. The other
    //    2 functions don't need the ISR. uart_tx_clear_ifg();
    //    uart_tx_enable_interrupt();
}

/**
 * Low-level output function needed to use mpaland printf() function
 */
void _putchar(char c) {
#ifdef LAUNCHPAD
    // if(!(UCA1IFG & UCTXIFG)) ASSERT(0)
    while (!(UCA1IFG & UCTXIFG))
        ; // Wait until interrupt flag for transmit buffer ready is set (means
          // can accept new data in buffer)
    UCA1TXBUF = c; // Send character (8 bits) to buffer

    // Some terminals require carriage return ("\r") after line-feed ("\n") for
    // an actual new line
    if (c == '\n') {
        _putchar('\r');
    }
#elif SUMOBOT
    while (!(UCA0IFG & UCTXIFG))
        ; // Wait until interrupt flag for transmit buffer ready is set (means
          // can accept new data in buffer)
    UCA0TXBUF = c; // Send character (8 bits) to buffer

    // Some terminals require carriage return ("\r") after line-feed ("\n") for
    // an actual new line
    if (c == '\n') {
        _putchar('\r');
    }
#endif
}

/**
 * NOT USED ANYMORE
 * Replaced by _putchar() for mpaland printf
 */
void uart_putchar_polling(char c) {
#ifdef LAUNCHPAD
    // if(!(UCA1IFG & UCTXIFG)) ASSERT(0)
    while (!(UCA1IFG & UCTXIFG))
        ; // Wait until interrupt flag for transmit buffer ready is set (means
          // can accept new data in buffer)
    UCA1TXBUF = c; // Send character (8 bits) to buffer

    // Some terminals require carriage return ("\r") after line-feed ("\n") for
    // an actual new line
    if (c == '\n') {
        uart_putchar_polling('\r');
    }
#elif SUMOBOT
    while (!(UCA0IFG & UCTXIFG))
        ; // Wait until interrupt flag for transmit buffer ready is set (means
          // can accept new data in buffer)
    UCA0TXBUF = c; // Send character (8 bits) to buffer

    // Some terminals require carriage return ("\r") after line-feed ("\n") for
    // an actual new line
    if (c == '\n') {
        uart_putchar_polling('\r');
    }
#endif
}

/**
 * NOT USED ANYMORE
 * Replaced by _putchar() for mpaland printf
 */
void uart_putchar_interrupt(char c) {
    while (ring_buffer_isfull(uart_ring_buffer))
        ; // Stay in loop until UART is able to transmit an element

    uart_tx_disable_interrupt(); // Disable interrupts while adding new element
                                 // to ring buffer
    const bool tx_ongoing =
        !ring_buffer_isempty(uart_ring_buffer); // Check if there is an on-going
                                                // UART transmission already
    ring_buffer_push(uart_ring_buffer, c);      // Put element in ring buffer
    if (!tx_ongoing) {   // If no current on-going UART transmission
        uart_tx_start(); // Start a new UART transmission
    }
    uart_tx_enable_interrupt(); // Enable interrupts again
    if (c == '\n') {
        uart_putchar_interrupt('\r');
    }
}

void uart_tx_start(void) {
    if (!ring_buffer_isempty(uart_ring_buffer))
#ifdef LAUNCHPAD
        UCA1TXBUF = ring_buffer_peek(uart_ring_buffer);
#elif SUMOBOT
        UCA0TXBUF = ring_buffer_peek(uart_ring_buffer);
#endif
}

INTERRUPT_FUNCTION(USCI_A1_VECTOR) uart_tx_isr(void) {
#ifdef LAUNCHPAD
    switch (__even_in_range(UCA1IV, 4)) {
    case 0x00: // No interrupt
        break;
    case 0x02: // RX_IFG

        break;
    case 0x04:                             // TX_IFG
        ring_buffer_pop(uart_ring_buffer); // Remove the just transmitted
                                           // element from ring buffer
        //                uart_tx_clear_ifg();
        //              // TX_IFG is auto cleared whenever writing to UCAxTXBUF
        //              and auto set when UCAxTXBUF is ready for more data
        uart_tx_start(); // Send data from ring buffer
        break;
    }
#elif SUMOBOT
    switch (__even_in_range(UCA0IV, 4)) {
    case 0x00: // No interrupt
        break;
    case 0x02: // RX_IFG
        break;
    case 0x04:                             // TX_IFG
        ring_buffer_pop(uart_ring_buffer); // Remove the just transmitted
                                           // element from ring buffer
        //                uart_tx_clear_ifg();
        uart_tx_start(); // Send data from ring buffer
        break;
    }
#endif
}

void uart_init_assert(void) {
    uart_tx_disable_interrupt();
    uart_init();
}

void uart_trace_assert(const char *str) {
    int i = 0;
    while (str[i] != '\0') {
        uart_putchar_polling(str[i]);
        i++;
    }
}
