#include "key.h"
#include "morse.h"
#include "uart.h"
#include "device.h"

// Morse key input on PA26, active-low, internal pull-up.
// User taps the key; short press = dot, long press = dash.
//
// Strict morse spacing (1 unit between elements, 3 to end a letter) is too
// tight for a human on a pushbutton: the release-to-press gap between two
// dashes routinely runs longer than a 3-unit letter gap, which splits letters
// apart. The gap thresholds below are therefore deliberately wider than the
// textbook values, and the letter gap is held clear of the dash threshold so
// keying a slow dash can never be read as the end of a letter.

#define KEY_PIN         26U
#define KEY_PIN_MASK    (1U << KEY_PIN)
#define KEY_GROUP       0U

#define UNIT_DEFAULT_MS 200U
#define DEBOUNCE_MS     10U
// Prosigns run longer than a plain letter: SOS is 9 elements keyed as one
// character. Allow room for those, and flag anything longer as overrun.
#define MAX_ELEMENTS    12U
#define TIMER_MAX       60000U
#define LOG_BUF_SIZE    256U

static uint16_t unit_ms = UNIT_DEFAULT_MS;

#define DASH_MIN_MS   (2U * unit_ms)   // hold this long for a dash
#define LETTER_GAP_MS (5U * unit_ms)   // release this long to end the letter
#define WORD_GAP_MS   (10U * unit_ms)  // release this long for a word space

typedef enum {
    KEY_STATE_IDLE,   // key up, nothing pending
    KEY_STATE_DOWN,   // key held, timing the element
    KEY_STATE_GAP     // key up, timing the gap after an element
} key_state_t;

static volatile char log_buf[LOG_BUF_SIZE];
static volatile uint8_t log_head;
static volatile uint8_t log_tail;

static key_state_t state;
static uint16_t press_ms;
static uint16_t gap_ms;

static uint8_t raw_last;      // last raw sample (1 = released)
static uint8_t stable;        // debounced level (1 = released)
static uint8_t debounce_ms;

static char pattern[MAX_ELEMENTS + 1U];
static uint8_t pattern_len;
static uint8_t word_pending;  // a letter was emitted, space not yet sent
static uint8_t verbose;       // print raw tap timing for debugging

static void log_char(char c)
{
    uint8_t next = (log_head + 1U) % LOG_BUF_SIZE;
    if (next != log_tail) {
        log_buf[log_head] = c;
        log_head = next;
    }
}

static void log_str(const char *s)
{
    while (*s != '\0') {
        log_char(*s++);
    }
}

static void log_number(uint16_t val)
{
    char tmp[6];
    uint8_t j = 0;
    if (val == 0U) {
        log_char('0');
        return;
    }
    while (val > 0U) { tmp[j++] = '0' + (char)(val % 10U); val /= 10U; }
    while (j > 0U) { log_char(tmp[--j]); }
}

static void add_element(uint8_t is_dash)
{
    if (pattern_len < MAX_ELEMENTS) {
        pattern[pattern_len++] = is_dash ? '-' : '.';
        log_char(is_dash ? '-' : '.');
    } else {
        log_str("[!overrun]");
    }
}

static void emit_letter(void)
{
    if (pattern_len == 0U) return;
    pattern[pattern_len] = '\0';
    log_str(" -> ");

    // Prosigns (SOS and friends) are run-together groups that decode to more
    // than one letter, so they are checked before the single-letter table.
    const char *prosign = Morse_DecodeProsign(pattern);
    if (prosign != (const char *)0) {
        log_str(prosign);
    } else {
        log_char(Morse_DecodePattern(pattern));
    }

    log_str("\r\n");
    pattern_len = 0;
    word_pending = 1;
}

void Key_Init(void)
{
    log_head = 0;
    log_tail = 0;
    state = KEY_STATE_IDLE;
    press_ms = 0;
    gap_ms = 0;
    pattern_len = 0;
    word_pending = 0;
    verbose = 0;
    debounce_ms = 0;
    raw_last = 1;
    stable = 1;

    PORT_REGS->GROUP[KEY_GROUP].PORT_DIRCLR = KEY_PIN_MASK;
    PORT_REGS->GROUP[KEY_GROUP].PORT_OUTSET = KEY_PIN_MASK; // pull direction = up
    PORT_REGS->GROUP[KEY_GROUP].PORT_PINCFG[KEY_PIN] =
        PORT_PINCFG_INEN_Msk | PORT_PINCFG_PULLEN_Msk;
}

void Key_Reset(void)
{
    state = KEY_STATE_IDLE;
    press_ms = 0;
    gap_ms = 0;
    pattern_len = 0;
    word_pending = 0;
}

void Key_SetUnit(uint16_t ms)
{
    if (ms < 20U) ms = 20U;
    if (ms > 2000U) ms = 2000U;
    unit_ms = ms;
}

uint16_t Key_GetUnit(void)
{
    return unit_ms;
}

void Key_SetVerbose(uint8_t on)
{
    verbose = on ? 1U : 0U;
}

uint8_t Key_GetVerbose(void)
{
    return verbose;
}

uint8_t Key_ReadPin(void)
{
    return (PORT_REGS->GROUP[KEY_GROUP].PORT_IN & KEY_PIN_MASK) ? 1U : 0U;
}

void Key_FlushLog(void)
{
    while (log_tail != log_head) {
        UART_WriteByte((uint8_t)log_buf[log_tail]);
        log_tail = (log_tail + 1U) % LOG_BUF_SIZE;
    }
}

void Key_Tick(void)
{
    // Key decoding is only active while playback is stopped (/stop). Anything
    // that queues morse for the LED clears that, so the two never overlap.
    if (!Morse_IsStopped()) {
        if (state != KEY_STATE_IDLE || pattern_len > 0U) {
            Key_Reset();
            log_str("[KEY OFF]\r\n");
        }
        // Keep the debounce tracker in sync so re-enabling does not see a
        // phantom edge from a level that changed while we were disabled.
        raw_last = (PORT_REGS->GROUP[KEY_GROUP].PORT_IN & KEY_PIN_MASK) ? 1U : 0U;
        stable = raw_last;
        debounce_ms = 0;
        return;
    }

    uint8_t raw = (PORT_REGS->GROUP[KEY_GROUP].PORT_IN & KEY_PIN_MASK) ? 1U : 0U;

    // Debounce: a new level must hold for DEBOUNCE_MS before it counts.
    if (raw != raw_last) {
        raw_last = raw;
        debounce_ms = 0;
    } else if (raw != stable) {
        if (debounce_ms < DEBOUNCE_MS) {
            debounce_ms++;
        } else {
            stable = raw;
            if (stable == 0U) {
                // Key went down: start timing the element.
                if (verbose) {
                    log_str("\r\n[DOWN gap=");
                    log_number(gap_ms);
                    log_str("ms letter=");
                    log_number(LETTER_GAP_MS);
                    log_str("ms]\r\n");
                }
                state = KEY_STATE_DOWN;
                press_ms = 0;
            } else {
                // Key came up: classify the element by how long it was held.
                uint8_t is_dash = (press_ms >= DASH_MIN_MS) ? 1U : 0U;
                if (verbose) {
                    log_str("[UP held=");
                    log_number(press_ms);
                    log_str("ms thr=");
                    log_number(DASH_MIN_MS);
                    log_str("ms -> ");
                    log_char(is_dash ? '-' : '.');
                    log_str("]\r\n");
                }
                add_element(is_dash);
                state = KEY_STATE_GAP;
                gap_ms = 0;
            }
        }
    }

    switch (state) {
        case KEY_STATE_DOWN:
            if (press_ms < TIMER_MAX) press_ms++;
            break;

        case KEY_STATE_GAP:
            if (gap_ms < TIMER_MAX) gap_ms++;
            if (pattern_len > 0U) {
                if (gap_ms >= LETTER_GAP_MS) {
                    emit_letter();
                }
            } else if (word_pending && gap_ms >= WORD_GAP_MS) {
                log_str("[SPACE]\r\n");
                word_pending = 0;
                state = KEY_STATE_IDLE;
            }
            break;

        default:
            break;
    }
}
