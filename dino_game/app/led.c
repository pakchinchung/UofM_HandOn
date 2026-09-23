#include "led.h"
#include "device.h"

#define LED0_PIN_MASK (1U << 2) // PB02

void LED_Init(void)
{
    PORT_REGS->GROUP[1].PORT_DIRSET = LED0_PIN_MASK;
    PORT_REGS->GROUP[1].PORT_OUTSET = LED0_PIN_MASK; // LED off (active-low)
}

void LED_On(void)
{
    PORT_REGS->GROUP[1].PORT_OUTCLR = LED0_PIN_MASK; // Active-low: clear = on
}

void LED_Off(void)
{
    PORT_REGS->GROUP[1].PORT_OUTSET = LED0_PIN_MASK; // Active-low: set = off
}

void LED_Toggle(void)
{
    PORT_REGS->GROUP[1].PORT_OUTTGL = LED0_PIN_MASK;
}
