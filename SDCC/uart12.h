//  for the Small Device C Compiler (SDCC)

#ifndef __UART12_H__
#define __UART12_H__

void uart0_isr(void) __interrupt(4) __using(3);
void uart_init(unsigned long baudrate);
__bit uart_char_avail(void);
char uart_getchar(void);
char uart_putchar(char c);

#endif

