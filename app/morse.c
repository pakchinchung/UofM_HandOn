#include "morse.h"
#include "uart.h"
#include "led.h"
#include <ctype.h>

#define DEFAULT_UNIT_MS 100U
#define LOG_BUF_SIZE 128U

static uint16_t unit_ms = DEFAULT_UNIT_MS;

#define DOT_MS         (1U * unit_ms)
#define DASH_MS        (3U * unit_ms)
#define ELEMENT_GAP_MS (1U * unit_ms)
#define LETTER_GAP_MS  (3U * unit_ms)
#define WORD_GAP_MS    (7U * unit_ms)

static volatile char log_buf[LOG_BUF_SIZE];
static volatile uint8_t log_head;
static volatile uint8_t log_tail;

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

#define QUEUE_SIZE 64U

// Compact morse encoding: sentinel bit marks start.
// Read bits after sentinel left-to-right: 0=dot, 1=dash.
// E.g. 'A' = .- = 0b0110 (sentinel=1, then 0=dot, 1=dash)
static const uint8_t morse_alpha[26] = {
    0x06, // A .-     0b00000110
    0x11, // B -...   0b00010001
    0x15, // C -.-.   0b00010101
    0x09, // D -..    0b00001001
    0x02, // E .      0b00000010
    0x14, // F ..-.   0b00010100
    0x0B, // G --.    0b00001011
    0x10, // H ....   0b00010000
    0x04, // I ..     0b00000100
    0x1E, // J .---   0b00011110
    0x0D, // K -.-    0b00001101
    0x12, // L .-..   0b00010010
    0x07, // M --     0b00000111
    0x05, // N -.     0b00000101
    0x0F, // O ---    0b00001111
    0x16, // P .--.   0b00010110
    0x1B, // Q --.-   0b00011011
    0x0A, // R .-.    0b00001010
    0x08, // S ...    0b00001000
    0x03, // T -      0b00000011
    0x0C, // U ..-    0b00001100
    0x18, // V ...-   0b00011000
    0x0E, // W .--    0b00001110
    0x19, // X -..-   0b00011001
    0x1D, // Y -.--   0b00011101
    0x13, // Z --..   0b00010011
};

static const uint8_t morse_digit[10] = {
    0x3F, // 0 -----  0b00111111
    0x3E, // 1 .----  0b00111110
    0x3C, // 2 ..---  0b00111100
    0x38, // 3 ...--  0b00111000
    0x30, // 4 ....-  0b00110000
    0x20, // 5 .....  0b00100000
    0x21, // 6 -....  0b00100001
    0x23, // 7 --...  0b00100011
    0x27, // 8 ---..  0b00100111
    0x2F, // 9 ----.  0b00101111
};

typedef struct {
    char ch;
    uint8_t code;
} morse_punct_t;

static const morse_punct_t morse_punct[] = {
    { '.', 0x55 }, // .-.-.-  0b01010101
    { ',', 0x73 }, // --..--  0b01110011
    { '?', 0x4C }, // ..--..  0b01001100
    { '\'', 0x5E }, // .----.  0b01011110
    { '!', 0x75 }, // -.-.--  0b01110101
    { '/', 0x29 }, // -..-.   0b00101001
    { '(', 0x2D }, // -.--.   0b00101101
    { ')', 0x6D }, // -.--.-  0b01101101
    { '&', 0x22 }, // .-...   0b00100010
    { ':', 0x47 }, // ---...  0b01000111
    { ';', 0x55 }, // -.-.-.  0b01010101 (same as period in some standards)
    { '=', 0x31 }, // -...-   0b00110001
    { '+', 0x2A }, // .-.-.   0b00101010
    { '-', 0x61 }, // -....-  0b01100001
    { '_', 0x6C }, // ..--.-  0b01101100
    { '"', 0x52 }, // .-..-.  0b01010010
    { '@', 0x56 }, // .--.-.  0b01010110
};

typedef enum {
    STATE_IDLE,
    STATE_ELEMENT_ON,
    STATE_ELEMENT_GAP,
    STATE_LETTER_GAP,
    STATE_WORD_GAP
} morse_state_t;

#define MSG_BUF_SIZE 64U

static volatile char queue_buf[QUEUE_SIZE];
static volatile uint8_t queue_head;
static volatile uint8_t queue_tail;

static char msg_buf[MSG_BUF_SIZE];
static uint8_t msg_len;
static uint8_t msg_complete;
static volatile uint8_t loop_active;

static morse_state_t state;
static uint16_t delay_counter;
static uint8_t current_pattern;
static uint8_t bits_remaining;
static char current_char;

static uint8_t lookup_char(char c)
{
    if (c >= 'A' && c <= 'Z') return morse_alpha[c - 'A'];
    if (c >= 'a' && c <= 'z') return morse_alpha[c - 'a'];
    if (c >= '0' && c <= '9') return morse_digit[c - '0'];
    for (uint8_t i = 0; i < sizeof(morse_punct) / sizeof(morse_punct[0]); i++) {
        if (morse_punct[i].ch == c) return morse_punct[i].code;
    }
    return 0;
}

static uint8_t count_bits(uint8_t pattern)
{
    uint8_t count = 0;
    while (pattern > 1U) {
        pattern >>= 1;
        count++;
    }
    return count;
}

static uint8_t encode_pattern(const char *p)
{
    uint8_t len = 0;
    uint8_t bits = 0;
    const char *s = p;
    while (*s == '.' || *s == '-') { len++; s++; }
    for (uint8_t i = 0; i < len; i++) {
        if (p[i] == '-') {
            bits |= (1U << i);
        }
    }
    return (1U << len) | bits;
}

char Morse_DecodePattern(const char *pattern)
{
    uint8_t code = encode_pattern(pattern);
    if (code <= 1U) return '?';
    for (uint8_t i = 0; i < 26U; i++) {
        if (morse_alpha[i] == code) return (char)('A' + i);
    }
    for (uint8_t i = 0; i < 10U; i++) {
        if (morse_digit[i] == code) return (char)('0' + i);
    }
    for (uint8_t i = 0; i < sizeof(morse_punct) / sizeof(morse_punct[0]); i++) {
        if (morse_punct[i].code == code) return morse_punct[i].ch;
    }
    return '?';
}

void Morse_Init(void)
{
    queue_head = 0;
    queue_tail = 0;
    log_head = 0;
    log_tail = 0;
    state = STATE_IDLE;
    delay_counter = 0;
    current_pattern = 0;
    bits_remaining = 0;
    current_char = '\0';
    msg_len = 0;
    msg_complete = 0;
    loop_active = 0;
    LED_Off();
}

void Morse_FlushLog(void)
{
    while (log_tail != log_head) {
        UART_WriteByte((uint8_t)log_buf[log_tail]);
        log_tail = (log_tail + 1U) % LOG_BUF_SIZE;
    }
}

void Morse_QueueChar(char c)
{
    uint8_t next = (queue_head + 1U) % QUEUE_SIZE;
    if (next != queue_tail) {
        queue_buf[queue_head] = c;
        queue_head = next;
    }
    if (c == '\r' || c == '\n') {
        if (msg_len > 0U) {
            msg_complete = 1;
        }
    } else {
        if (msg_complete) {
            msg_len = 0;
            msg_complete = 0;
        }
        if (msg_len < MSG_BUF_SIZE - 1U) {
            msg_buf[msg_len++] = c;
        }
    }
}

void Morse_QueueString(const char *str)
{
    while (*str != '\0') {
        uint8_t next = (queue_head + 1U) % QUEUE_SIZE;
        if (next == queue_tail) break;
        queue_buf[queue_head] = *str++;
        queue_head = next;
    }
}

void Morse_Replay(void)
{
    if (msg_len == 0U) return;
    for (uint8_t i = 0; i < msg_len; i++) {
        uint8_t next = (queue_head + 1U) % QUEUE_SIZE;
        if (next == queue_tail) break;
        queue_buf[queue_head] = msg_buf[i];
        queue_head = next;
    }
}

void Morse_ToggleLoop(void)
{
    if (loop_active) {
        loop_active = 0;
        UART_WriteString("[LOOP OFF]\r\n");
    } else {
        loop_active = 1;
        UART_WriteString("[LOOP ON]\r\n");
        Morse_Replay();
    }
}

void Morse_SetSpeed(uint16_t ms)
{
    if (ms < 10U) ms = 10U;
    if (ms > 1000U) ms = 1000U;
    unit_ms = ms;
}

uint16_t Morse_GetSpeed(void)
{
    return unit_ms;
}

static char dequeue(void)
{
    if (queue_tail == queue_head) return '\0';
    char c = queue_buf[queue_tail];
    queue_tail = (queue_tail + 1U) % QUEUE_SIZE;
    return c;
}

static void start_next_element(void)
{
    if (bits_remaining == 0U) {
        LED_Off();
        state = STATE_LETTER_GAP;
        delay_counter = LETTER_GAP_MS;
        return;
    }
    uint8_t total = count_bits(current_pattern);
    uint8_t idx = total - bits_remaining;
    bits_remaining--;
    uint8_t bit = (current_pattern >> idx) & 0x01U;
    log_char(bit ? '-' : '.');
    LED_On();
    delay_counter = (bit != 0U) ? DASH_MS : DOT_MS;
    state = STATE_ELEMENT_ON;
}

static void start_next_char(void)
{
    char c = dequeue();
    if (c == '\0') {
        state = STATE_IDLE;
        return;
    }
    if (c == ' ' || c == '\r' || c == '\n') {
        LED_Off();
        log_str("  [SPACE]\r\n");
        state = STATE_WORD_GAP;
        delay_counter = WORD_GAP_MS;
        return;
    }
    uint8_t pattern = lookup_char(c);
    if (pattern == 0U) {
        start_next_char();
        return;
    }
    current_char = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    log_char(current_char);
    log_str(": ");
    bits_remaining = count_bits(pattern);
    current_pattern = pattern;
    start_next_element();
}

void Morse_Tick(void)
{
    if (state == STATE_IDLE) {
        if (queue_head != queue_tail) {
            start_next_char();
        } else if (loop_active && msg_len > 0U) {
            Morse_Replay();
        }
        return;
    }

    if (delay_counter > 0U) {
        delay_counter--;
        return;
    }

    switch (state) {
        case STATE_ELEMENT_ON:
            LED_Off();
            if (bits_remaining > 0U) {
                state = STATE_ELEMENT_GAP;
                delay_counter = ELEMENT_GAP_MS;
            } else {
                log_str("\r\n");
                state = STATE_LETTER_GAP;
                delay_counter = LETTER_GAP_MS;
            }
            break;
        case STATE_ELEMENT_GAP:
            start_next_element();
            break;
        case STATE_LETTER_GAP:
        case STATE_WORD_GAP:
            start_next_char();
            break;
        default:
            state = STATE_IDLE;
            break;
    }
}
