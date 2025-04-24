
// void uart_tx_enable_interrupt(void);
// void uart_tx_disable_interrupt(void);
// void uart_tx_clear_ifg(void);
void uart_init(void);
void uart_putchar_polling(char);
void uart_putchar_interrupt(char);
void uart_tx_start(void);
void _putchar(char); // mpaland low-level output function needed for printf()
