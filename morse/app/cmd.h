#ifndef CMD_H
#define CMD_H

#include <stdint.h>

void CMD_Init(void);
void CMD_ReceiveByte(char c);
void CMD_Tick(void);
void CMD_Process(void);

#endif
