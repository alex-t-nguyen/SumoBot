#ifndef DISABLE_TRACE
#include "common/trace.h"
#include "common/assert_handler.h"
#include "drivers/uart.h"
#include "externals/printf/printf.h"
#include <stdbool.h>

static bool initialized = false;

/**
 * Initialize UART
 */
void trace_init(void) {
    ASSERT(!initialized);
    uart_init();
    initialized = true;
}

/**
 * Wrapper for printf function
 *
 * @param format: string to determine how to format output string (same as in
 * printf)
 */
void trace(const char *format, ...) {
    ASSERT(initialized);

    // Variable argument functions from stdarg.h
    va_list args_list; // Create va_list object that holds the arguments
                       // (variables in the ...)
    va_start(args_list,
             format); // Initialize the va_list object with the arguments,
                      // starting after the variable "format" as the identifier
    vprintf(format, args_list); // Same as printf, but using variable arguments
                                // (va_list object) (in stdio.h, but also a
                                // macro in externals/printf/printf.h)
    va_end(args_list);          // Clean up va_list object when done using it
}
#endif
