#include "button.h"
#include "device.h"

// SW0 on PB03, active-low, internal pull-up required
#define SW0_PIN_MASK  (1U << 3)
#define DEBOUNCE_MS   250U

static Button_Callback pressed_cb;
static uint16_t lockout_counter;
static uint8_t last_stable;

void Button_Init(void)
{
    pressed_cb = (Button_Callback)0;
    lockout_counter = 0;
    last_stable = 1;

    PORT_REGS->GROUP[1].PORT_DIRCLR = SW0_PIN_MASK;
    PORT_REGS->GROUP[1].PORT_OUTSET = SW0_PIN_MASK;
    PORT_REGS->GROUP[1].PORT_PINCFG[3] = PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
}

void Button_SetPressedCallback(Button_Callback cb)
{
    pressed_cb = cb;
}

void Button_Tick(void)
{
    if (lockout_counter > 0U) {
        lockout_counter--;
        return;
    }

    uint8_t current = (PORT_REGS->GROUP[1].PORT_IN & SW0_PIN_MASK) ? 1U : 0U;

    if (current == 0U && last_stable == 1U) {
        lockout_counter = DEBOUNCE_MS;
        if (pressed_cb != (Button_Callback)0) {
            pressed_cb();
        }
    }
    last_stable = current;
}
