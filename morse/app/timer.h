#ifndef TIMER_H
#define TIMER_H

typedef void (*Timer_Callback)(void);

void Timer_Init(void);
void Timer_RegisterCallback(Timer_Callback cb);

#endif
