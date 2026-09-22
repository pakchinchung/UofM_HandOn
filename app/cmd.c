#include "cmd.h"
#include "uart.h"
#include "morse.h"

#define CMD_BUF_SIZE 128U
#define RX_BUF_SIZE  64U
#define CMD_TIMEOUT  50U

static uint8_t str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return (*a == *b) ? 1U : 0U;
}

static uint8_t str_starts(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

static char cmd_buf[CMD_BUF_SIZE];
static uint8_t cmd_pos;
static uint8_t in_command;
static volatile uint16_t cmd_idle_count;

static volatile char rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;

static uint16_t parse_number(const char *s)
{
    uint16_t val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10U + (uint16_t)(*s - '0');
        s++;
    }
    return val;
}

static void print_number(uint16_t val)
{
    char num[6];
    uint8_t i = 0;
    if (val == 0U) {
        num[i++] = '0';
    } else {
        char tmp[6];
        uint8_t j = 0;
        while (val > 0U) { tmp[j++] = '0' + (char)(val % 10U); val /= 10U; }
        while (j > 0U) { num[i++] = tmp[--j]; }
    }
    num[i] = '\0';
    UART_WriteString(num);
}

static void execute(void)
{
    if (cmd_pos == 0U) return;
    cmd_buf[cmd_pos] = '\0';

    if (str_eq(cmd_buf, "help")) {
        UART_WriteString("\r\n--- Commands ---\r\n");
        UART_WriteString("/help             - Show this help\r\n");
        UART_WriteString("/speed <ms>       - Set dot unit (10-1000ms)\r\n");
        UART_WriteString("/speed            - Show current speed\r\n");
        UART_WriteString("/loop             - Toggle replay loop on/off\r\n");
        UART_WriteString("/stop             - Stop loop and abort playback\r\n");
        UART_WriteString("/replay           - Replay last message once\r\n");
        UART_WriteString("/decode <morse>   - Decode morse to text\r\n");
        UART_WriteString("  Use . and - for dots/dashes\r\n");
        UART_WriteString("  Space between letters, / between words\r\n");
        UART_WriteString("  Example: /decode ... --- ...\r\n");
        UART_WriteString("----------------\r\n");
    } else if (str_eq(cmd_buf, "loop")) {
        Morse_ToggleLoop();
    } else if (str_eq(cmd_buf, "stop")) {
        Morse_Stop();
        UART_WriteString("[STOP]\r\n");
    } else if (str_eq(cmd_buf, "replay")) {
        UART_WriteString("[REPLAY]\r\n");
        Morse_Replay();
    } else if (str_eq(cmd_buf, "speed")) {
        UART_WriteString("Speed: ");
        print_number(Morse_GetSpeed());
        UART_WriteString("ms\r\n");
    } else if (str_starts(cmd_buf, "speed ")) {
        uint16_t val = parse_number(&cmd_buf[6]);
        if (val >= 10U && val <= 1000U) {
            Morse_SetSpeed(val);
            UART_WriteString("Speed set to ");
            print_number(val);
            UART_WriteString("ms\r\n");
        } else {
            UART_WriteString("Invalid speed (10-1000)\r\n");
        }
    } else if (str_starts(cmd_buf, "decode ")) {
        char *p = &cmd_buf[7];
        char token[8];
        uint8_t ti;
        UART_WriteString("Decoded: ");
        while (*p != '\0') {
            while (*p == ' ') p++;
            if (*p == '\0') break;
            if (*p == '/') {
                UART_WriteByte(' ');
                p++;
                continue;
            }
            ti = 0;
            while ((*p == '.' || *p == '-') && ti < 7U) {
                token[ti++] = *p++;
            }
            token[ti] = '\0';
            UART_WriteByte((uint8_t)Morse_DecodePattern(token));
        }
        UART_WriteString("\r\n");
    } else {
        UART_WriteString("Unknown command. Type /help\r\n");
    }
}

static void handle_byte(char c)
{
    if (!in_command) {
        if (c == '/') {
            in_command = 1;
            cmd_pos = 0;
            UART_WriteByte('/');
            return;
        }
        Morse_QueueChar(c);
        return;
    }

    if (c == '\r' || c == '\n') {
        UART_WriteString("\r\n");
        execute();
        in_command = 0;
        cmd_pos = 0;
    } else if (c == '\b' || c == 127) {
        if (cmd_pos > 0U) {
            cmd_pos--;
            UART_WriteString("\b \b");
        }
    } else {
        if (cmd_pos < CMD_BUF_SIZE - 1U) {
            cmd_buf[cmd_pos++] = c;
            UART_WriteByte((uint8_t)c);
        }
    }
}

void CMD_Init(void)
{
    cmd_pos = 0;
    in_command = 0;
    cmd_idle_count = 0;
    rx_head = 0;
    rx_tail = 0;
}

void CMD_ReceiveByte(char c)
{
    uint8_t next = (rx_head + 1U) % RX_BUF_SIZE;
    if (next != rx_tail) {
        rx_buf[rx_head] = c;
        rx_head = next;
    }
}

void CMD_Tick(void)
{
    if (in_command && cmd_pos > 0U) {
        if (cmd_idle_count < CMD_TIMEOUT) {
            cmd_idle_count++;
        }
    }
}

void CMD_Process(void)
{
    uint8_t got_data = 0;
    while (rx_tail != rx_head) {
        char c = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1U) % RX_BUF_SIZE;
        handle_byte(c);
        got_data = 1;
    }
    if (got_data) {
        cmd_idle_count = 0;
    }
    if (in_command && cmd_pos > 0U && cmd_idle_count >= CMD_TIMEOUT) {
        UART_WriteString("\r\n");
        execute();
        in_command = 0;
        cmd_pos = 0;
        cmd_idle_count = 0;
    }
}
