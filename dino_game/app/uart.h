#ifndef UART_H
#define UART_H

#include <stdint.h>

void UART_Init(uint32_t baud_rate);
void UART_WriteByte(uint8_t data);
void UART_WriteString(const char *str);

/* printf-style output. The MCC stdio stub discards stdout, so this is the
 * console for the whole application. Output longer than the internal buffer
 * is truncated. */
void UART_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
