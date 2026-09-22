#ifndef UART_H
#define UART_H

#include <stdint.h>

void UART_Init(uint32_t baud_rate);
void UART_WriteByte(uint8_t data);
void UART_WriteString(const char *str);
uint8_t UART_ReadByte(void);

#endif
