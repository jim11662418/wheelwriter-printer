// For the Keil C51 compiler.

#ifndef __UART12_H__
#define __UART12_H__

void uart_init(unsigned long baudrate);
bit uart_char_avail(void);
char uart_getchar(void);
char uart_putchar(char c);

#endif
