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

#define LED_BLINK_INTERVAL_MS (1000UL)

/*
    Main application
*/

int main(void)
{
    uint32_t ledLastToggle;

    SYSTEM_Initialize();
    MILLIS_Initialize();

    /* stdout is redirected to USART1 by USART1_Initialize(). */
    printf("\r\n=== TestAi2 boot ===\r\n");
    printf("Device : AVR128DA48 @ %lu Hz\r\n", (unsigned long)F_CPU);
    printf("UART   : USART1 115200 8N1\r\n");
    printf("Timer  : TCA0 1 ms tick, millis() ready\r\n");
    printf("LED    : PC6 toggling every %lu ms\r\n\r\n",
           (unsigned long)LED_BLINK_INTERVAL_MS);

    ledLastToggle = millis();

    while(1)
    {
        if(MILLIS_IntervalElapsed(&ledLastToggle, LED_BLINK_INTERVAL_MS))
        {
            IO_PC6_Toggle();
            printf("[%lu ms] LED PC6 = %d\r\n",
                   (unsigned long)millis(),
                   (IO_PC6_GetValue() != 0) ? 1 : 0);
        }
    }
}