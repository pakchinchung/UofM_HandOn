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

#define LED_BLINK_INTERVAL_MS (500UL)

/*
    Main application
*/

int main(void)
{
    uint32_t ledLastToggle;
    bool oledOk;
    bool ioOk;

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

    if (oledOk)
    {
        BRINGUP_Initialize();
    }
    else
    {
        printf("Bring-up screen skipped, no display.\r\n");
    }

    ledLastToggle = millis();

    while(1)
    {
        /* Input samplers. Both rate limit themselves internally. */
        MCP23008_Tasks();
        POT_Tasks();

        if (oledOk)
        {
            BRINGUP_Tasks();
        }

        /* Heartbeat, so a wedged main loop is obvious without a terminal. */
        if(MILLIS_IntervalElapsed(&ledLastToggle, LED_BLINK_INTERVAL_MS))
        {
            IO_PC6_Toggle();
        }
    }
}