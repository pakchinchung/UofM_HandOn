/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes
#include "../../app/led.h"
#include "../../app/timer.h"
#include "../../app/uart.h"
#include "../../app/morse.h"
#include "../../app/button.h"
#include "../../app/cmd.h"
#include "../../app/key.h"



// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

static void tick_handler(void)
{
    Morse_Tick();
    Button_Tick();
    Key_Tick();
    CMD_Tick();
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    LED_Init();
    Timer_Init();
    UART_Init(115200);
    Morse_Init();
    CMD_Init();
    Button_Init();
    Key_Init();
    Button_SetPressedCallback(Morse_ToggleLoop);
    Timer_RegisterCallback(tick_handler);

    UART_WriteString("Morse Ready - type /help for commands\r\n");
    UART_WriteString("Send /stop to tap morse on the PA26 key\r\n");

    while ( true )
    {
        CMD_Process();
        Morse_FlushLog();
        Key_FlushLog();
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

