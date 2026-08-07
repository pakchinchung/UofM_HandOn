 /*
 * MAIN Generated Driver File
 * 
 * @file main.c
 * 
 * @defgroup main MAIN
 * 
 * @brief This is the generated driver implementation file for the MAIN driver.
 *
 * @version MAIN Driver Version 1.0.2
 *
 * @version Package Version: 3.1.2
*/

/*
� [2026] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.? 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
*/
#include <stdio.h>
#include "mcc_generated_files/system/system.h"
#include "millis.h"
#include "i2c_bus.h"
#include "ssd1306.h"
#include "mcp23008.h"
#include "pot.h"
#include "bringup.h"
#include "dino_game.h"
#include "breakout.h"
#include "menu.h"

/* What the shell is currently running. GP7 always returns this to the menu. */
typedef enum
{
    SHELL_MODE_MENU = 0,
    SHELL_MODE_DINO,
    SHELL_MODE_BREAKOUT,
    SHELL_MODE_DIAGNOSTIC,
} shell_mode_t;

#define LED_BLINK_INTERVAL_MS (500UL)

/* Bus speed selection policy for the boot sweep.
 *
 * 1 = only use a rung whose SCL timing is inside the I2C specification.
 * 0 = use the fastest rung that passes on this board, spec or not.
 *
 * Set to 0 deliberately: 960 kHz measured clean here and buys 2 ms per frame of
 * CPU back from the blocking push, which is worth more than the timing margin on
 * a self-contained board. The trade-off is that 960 kHz runs a 458 ns SCL low
 * time against the 500 ns Fast-mode-Plus minimum, so it is 8% out of spec and
 * could become marginal with a hotter part, a longer harness or weaker pull-ups.
 * The sweep prints the spec column either way, so a regression stays visible. */
#define I2C_SWEEP_REQUIRE_IN_SPEC (0)

/* Rung to use when it passes the sweep, regardless of the policy above. 1 MHz
 * requested resolves to 960 kHz actual on this board. Set to 0 to just take
 * whatever the sweep picks. */
#define I2C_PREFERRED_SPEED (1000000UL)

/*
    Main application
*/

int main(void)
{
    uint32_t ledLastToggle;
    bool oledOk;
    bool ioOk;
    shell_mode_t shellMode = SHELL_MODE_MENU;
    uint8_t pendingEdges = 0;

    SYSTEM_Initialize();
    MILLIS_Initialize();

    /* stdout is redirected to USART1 by USART1_Initialize(). */
    printf("\r\n=== TestAi2 boot ===\r\n");
    printf("Device : AVR128DA48 @ %lu Hz\r\n", (unsigned long)F_CPU);
    printf("UART   : USART1 115200 8N1\r\n");
    printf("Timer  : TCA0 1 ms tick, millis() %s\r\n",
           MILLIS_TickIsOneMs() ? "ready" : "TICK IS NOT 1 ms!");
    printf("I2C    : TWI0 on PC2/PC3, per-device speed\r\n");

    POT_Initialize();
    printf("POT    : ADC0 AIN7 (PD7), first read %u\r\n", POT_RawGet());

    /* Scan before touching either device, so a wrong address or a dead bus is
     * visible as data instead of guessed at from a failed init. */
    {
        uint8_t found[8];
        uint8_t count = I2C_BusScan(I2C_SPEED_STANDARD, found, (uint8_t)sizeof(found));
        uint8_t i;

        printf("I2Cscan: %u device(s) at 100 kHz:", count);
        for (i = 0; i < count; i++)
        {
            printf(" 0x%02X", found[i]);
        }
        if (0U == count)
        {
            printf(" none - check SDA=PC2 SCL=PC3, pull-ups and power");
        }
        printf("\r\n");
    }

    oledOk = SSD1306_Initialize();
    printf("OLED   : SSD1306 0x%02X %s", SSD1306_I2C_ADDR, oledOk ? "OK" : "FAILED");
    if (!oledOk)
    {
        /* Step 0 means the address never acknowledged. 0xF0 is the frame buffer
         * push, 0xF1 the final display-on. Error 1 is an address NACK, 2 a data
         * NACK, 3 a bus collision, 0x10 a timeout. */
        printf(" at step 0x%02X, i2c error %u",
               SSD1306_InitFailStepGet(), SSD1306_InitFailErrorGet());
    }
    printf("\r\n");

    ioOk = MCP23008_Initialize();
    printf("IOEXP  : MCP23008 0x%02X %s (GP5=JUMP GP6=START GP7=RESET)\r\n",
           MCP23008_I2C_ADDR, ioOk ? "OK" : "NO RESPONSE");
    printf("Buttons: active low, external pull-ups, GPPU off\r\n\r\n");

    /* Find the fastest bus rung this board actually tolerates, rather than
     * trusting the arithmetic. With eight devices hanging off SDA/SCL the total
     * capacitance and the pull-up strength set the real ceiling, and neither is
     * knowable from here. Each rung gets a full 1025 byte frame push, verified
     * and timed; the fastest that both succeeds and reads back a sane frame is
     * kept. Only the display is moved, so nothing else on the bus is affected. */
    if (oledOk)
    {
        static const uint32_t candidates[] =
        {
            100000UL, 200000UL, 300000UL, 400000UL,
            /* These only become distinct rungs once F_CPU is raised. At 4 MHz
             * they all collapse onto MBAUD 0 and are skipped as duplicates. */
            600000UL, 800000UL, 1000000UL
        };
        uint32_t best = candidates[0];
        uint16_t bestMs = 0xFFFFU;
        uint32_t fastest = candidates[0];
        uint16_t fastestMs = 0xFFFFU;
        uint16_t preferredMs = 0xFFFFU;
        bool preferredPassed = false;
        uint8_t i;

        printf("\r\nI2C speed sweep (full 1025-byte frame push):\r\n");
        printf("  SCL ceiling at F_CPU %lu Hz is %lu Hz (divisor floor of 10),\r\n",
               (unsigned long)F_CPU, (unsigned long)I2C_SPEED_CEILING);
        printf("  but the TWI peripheral tops out at %lu Hz (Fast-mode Plus).\r\n",
               (unsigned long)I2C_SPEED_FAST_PLUS);
        printf("  req     MBAUD  actual   Tlow   min   spec  result\r\n");

        for (i = 0; i < (uint8_t)(sizeof(candidates) / sizeof(candidates[0])); i++)
        {
            uint32_t request = candidates[i];
            uint8_t mbaud = I2C_MBaudFor(request);
            uint16_t lowNs = I2C_SclLowTimeNsFor(request);
            uint32_t start;
            uint16_t elapsed;
            bool pushOk;

            /* Skip a rung that resolves to the same MBAUD as the previous one;
             * it would be the identical bus timing measured twice. */
            if ((i > 0U) && (mbaud == I2C_MBaudFor(candidates[i - 1U])))
            {
                continue;
            }

            SSD1306_SpeedSet(request);
            SSD1306_DirtyAll();

            start = millis();
            pushOk = SSD1306_Update();
            elapsed = (uint16_t)(millis() - start);

            printf("  %6lu  %3u    %6lu  %4uns %4uns  %s  ",
                   (unsigned long)request, mbaud,
                   (unsigned long)I2C_SpeedActualFor(request), lowNs,
                   I2C_SclLowMinNsFor(request),
                   I2C_TimingIsInSpec(request) ? "ok  " : "OVER");

            if (pushOk)
            {
                printf("OK %ums", elapsed);

                /* Fastest overall, whether or not the timing is legal. */
                if (elapsed < fastestMs)
                {
                    fastestMs = elapsed;
                    fastest = request;
                }

                /* Candidate for actual use, filtered by the policy above. */
                if ((elapsed < bestMs) &&
                    ((0 == I2C_SWEEP_REQUIRE_IN_SPEC) || I2C_TimingIsInSpec(request)))
                {
                    bestMs = elapsed;
                    best = request;
                }

                if (I2C_PREFERRED_SPEED == request)
                {
                    preferredPassed = true;
                    preferredMs = elapsed;
                }
            }
            else
            {
                printf("FAIL err %u", I2C_LastErrorGet());
            }

            printf("\r\n");
        }

        /* Honour the preferred rung when it actually passed, otherwise fall back
         * to whatever the sweep found. Selecting a rung unconditionally would
         * also select it on a FAIL, and would report a frame time measured at a
         * different speed. */
        if (preferredPassed)
        {
            best = I2C_PREFERRED_SPEED;
            bestMs = preferredMs;
        }
        else if (0UL != I2C_PREFERRED_SPEED)
        {
            printf("  preferred %lu Hz did not pass, falling back\r\n",
                   (unsigned long)I2C_PREFERRED_SPEED);
        }
        else
        {
            /* No preference set; the sweep result stands. */
        }

        SSD1306_SpeedSet(best);
        printf("Using %lu Hz for the display (%lu Hz actual, %ums/frame, %s)\r\n",
               (unsigned long)best, (unsigned long)I2C_SpeedActualFor(best), bestMs,
               I2C_TimingIsInSpec(best) ? "in spec" : "OUT OF SPEC by choice");

        if (fastest != best)
        {
            printf("  (%lu Hz also passed at %ums/frame)\r\n",
                   (unsigned long)I2C_SpeedActualFor(fastest), fastestMs);
        }

        printf("\r\n");
    }

    if (!oledOk)
    {
        printf("No display; running headless. UART output only.\r\n");
    }
    else
    {
        MENU_Enter();
    }

    ledLastToggle = millis();

    while(1)
    {
        /* Input samplers. Both rate limit themselves internally. */
        MCP23008_Tasks();
        POT_Tasks();

        /* Accumulate button press edges here rather than in each game.
         *
         * MCP23008_Pressed() clears its latch on read, so only one caller can
         * have it. Latching centrally lets the shell act on GP7 and still hand
         * the remaining edges to the active mode. Accumulating also means a press
         * made between two 20 ms logic ticks is not lost: the bits stay set until
         * a tick consumes them. */
        pendingEdges |= MCP23008_Pressed();

        if (oledOk)
        {
            /* GP7 is global: back to the home menu from wherever we are. */
            if (0U != (pendingEdges & BTN_RESET))
            {
                pendingEdges = 0U;

                if (SHELL_MODE_MENU != shellMode)
                {
                    shellMode = SHELL_MODE_MENU;
                    MENU_Enter();
                    printf("GP7: back to menu\r\n");
                }
            }

            switch (shellMode)
            {
                case SHELL_MODE_MENU:
                {
                    menu_item_t choice = MENU_Tasks(&pendingEdges);

                    switch (choice)
                    {
                        case MENU_ITEM_DINO:
                            shellMode = SHELL_MODE_DINO;
                            GAME_Initialize();
                            break;

                        case MENU_ITEM_BREAKOUT:
                            shellMode = SHELL_MODE_BREAKOUT;
                            BRK_Initialize();
                            break;

                        case MENU_ITEM_DIAGNOSTIC:
                            shellMode = SHELL_MODE_DIAGNOSTIC;
                            BRINGUP_Initialize();
                            break;

                        case MENU_ITEM_NONE:
                        default:
                            /* Still choosing. */
                            break;
                    }
                    break;
                }

                case SHELL_MODE_DINO:
                    GAME_Tasks(&pendingEdges);
                    break;

                case SHELL_MODE_BREAKOUT:
                    BRK_Tasks(&pendingEdges);
                    break;

                case SHELL_MODE_DIAGNOSTIC:
                default:
                    BRINGUP_Tasks();
                    pendingEdges = 0U;
                    break;
            }
        }

        /* Heartbeat, so a wedged main loop is obvious without a terminal. */
        if(MILLIS_IntervalElapsed(&ledLastToggle, LED_BLINK_INTERVAL_MS))
        {
            IO_PC6_Toggle();
        }
    }
}