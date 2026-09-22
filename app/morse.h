#ifndef MORSE_H
#define MORSE_H

#include <stdint.h>

void Morse_Init(void);
void Morse_QueueChar(char c);
void Morse_QueueString(const char *str);
void Morse_Replay(void);
void Morse_ToggleLoop(void);
void Morse_SetSpeed(uint16_t unit_ms);
uint16_t Morse_GetSpeed(void);
void Morse_Tick(void);
void Morse_FlushLog(void);
char Morse_DecodePattern(const char *pattern);

#endif
