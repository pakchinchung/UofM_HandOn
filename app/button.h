#ifndef BUTTON_H
#define BUTTON_H

typedef void (*Button_Callback)(void);

void Button_Init(void);
void Button_SetPressedCallback(Button_Callback cb);
void Button_Tick(void);

#endif
