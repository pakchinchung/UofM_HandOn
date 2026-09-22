#ifndef KEY_H
#define KEY_H

#include <stdint.h>

void Key_Init(void);
void Key_Tick(void);
void Key_FlushLog(void);
void Key_SetUnit(uint16_t ms);
uint16_t Key_GetUnit(void);
void Key_Reset(void);
void Key_SetVerbose(uint8_t on);
uint8_t Key_GetVerbose(void);
uint8_t Key_ReadPin(void);

#endif
