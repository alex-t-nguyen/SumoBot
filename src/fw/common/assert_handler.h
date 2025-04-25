#ifndef ASSERT_HANDLER_H
#include <stdint.h>

#define ASSERT(expression)                                                     \
    do {                                                                       \
        if (!expression) {                                                   \
            uint16_t pc; \
            pc = (uint16_t)__builtin_return_address(0); \
            assert_handler(pc);                                                  \
        }                                                                      \
    } while (0);

#define ASSERT_INTERRUPT(expression)                                                     \
    do {                                                                       \
        if (!(expression)) {                                                   \
            while(1) {}  \
        }                                                                     \
    } while (0);

void assert_trace(uint16_t);
void assert_blink_led(void);
void assert_handler(uint16_t);

#endif // ASSERT_HANDLER_H
