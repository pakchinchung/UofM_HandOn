/**
 * TCA0 Generated Timer Driver API Header File
 * 
 * @file tca0.h
 * 
 * @ingroup timerdriver
 * 
 * @brief This file contains API prototypes and other data types for the TCA0 Timer Driver.
 *
 * @version TCA0 Timer Driver Version 3.0.0
 *
 * @version Package Version 7.1.0
*/
/*
© [2026] Microchip Technology Inc. and its subsidiaries.

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

#ifndef TCA0_H
#define TCA0_H

#include <stdint.h>
#include <stdbool.h>
#include "../system/utils/compiler.h"
#include "timer_interface.h"

/**
 * @ingroup timerdriver
 * @brief Defines the maximum count of the timer.
 */
#define TCA0_MAX_COUNT (65535U)

/**
 * @ingroup timerdriver
 * @brief Defines the timer prescaled clock frequency in hertz.
 */
#define TCA0_CLOCK_FREQ (24000000UL)


/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_MAX_COUNT.
 */
#define TIMER0_MAX_COUNT TCA0_MAX_COUNT

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_CLOCK_FREQ.
 */
#define TIMER0_CLOCK_FREQ TCA0_CLOCK_FREQ

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_Initialize API.
 */
#define Timer0_Initialize TCA0_Initialize

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_Deinitialize API.
 */
#define Timer0_Deinitialize TCA0_Deinitialize

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_Start API.
 */
#define Timer0_Start TCA0_Start

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_Stop API.
 */
#define Timer0_Stop TCA0_Stop

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_CounterGet API.
 */
#define Timer0_CounterGet TCA0_CounterGet

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_CounterSet API.
 */
#define Timer0_CounterSet TCA0_CounterSet

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_PeriodSet API.
 */
#define Timer0_PeriodSet TCA0_PeriodSet

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_PeriodGet API.
 */
#define Timer0_PeriodGet TCA0_PeriodGet

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_MaxCountGet API.
 */
#define Timer0_MaxCountGet TCA0_MaxCountGet

/**
 * @ingroup timerdriver
 * @brief Defines the Custom Name for the \ref TCA0_OverflowCallbackRegister API.
 */
#define Timer0_OverflowCallbackRegister TCA0_OverflowCallbackRegister

/**
 @ingroup timerdriver
 @struct TIMER_INTERFACE
 @brief Declares an instance of TIMER_INTERFACE for the TCA0 module.
 */
extern const struct TIMER_INTERFACE Timer0;

/**
 * @ingroup timerdriver
 * @brief Initializes the TCA0 module.
 *        This routine must be called before any other TCA0 routines.
 * @param None.
 * @return None.
 */
void TCA0_Initialize(void);

/**
 * @ingroup timerdriver
 * @brief Deinitializes the TCA0 module.
 * @param None.
 * @return None.
 */
void TCA0_Deinitialize(void);

/**
 * @ingroup timerdriver
 * @brief Starts TCA0.
 * @pre Initialize TCA0 with TCA0_Initialize() before calling this API.
 * @param None.
 * @return None.
 */
void TCA0_Start(void);

/**
 * @ingroup timerdriver
 * @brief Stops TCA0.
 * @pre Initialize TCA0 with TCA0_Initialize() before calling this API.
 * @param None.
 * @return None.
 */
void TCA0_Stop(void);

/**
 * @ingroup timerdriver
 * @brief Returns the current counter value.
 * @pre Initialize TCA0 with TCA0_Initialize() before calling this API.
 * @param None.
 * @return Counter value
 */
uint32_t TCA0_CounterGet(void);

/**
 * @ingroup timerdriver
 * @brief Sets the counter value.
 * @pre Initialize TCA0 with TCA0_Initialize() before calling this API.
 * @param counterValue - Counter value to be written to the CNT register
 * @return None.
 */
void TCA0_CounterSet(uint32_t counterValue);

/**
 * @ingroup timerdriver
 * @brief Sets the period count value.
 * @pre Initialize TCA0 with TCA0_Initialize() before calling this API.
 * @param periodCount - Period count value to be written to the PER register
 * @return None.
 */
void TCA0_PeriodSet(uint32_t periodCount);

/**
 * @ingroup timerdriver
 * @brief Returns the current period value.
 * @pre Initialize TCA0 with TCA0_Initialize() before calling this API.
 * @param None.
 * @return Period count value
 */
uint32_t TCA0_PeriodGet(void);

/**
 * @ingroup timerdriver
 * @brief Returns the maximum count value.
 * @param None.
 * @return Maximum count value
 */
uint32_t TCA0_MaxCountGet(void);

/**
 * @ingroup timerdriver
 * @brief Registers a callback function for the TCA0 overflow event.
 * @param CallbackHandler - Address of the custom callback function
 * @return None.
 */
 void TCA0_OverflowCallbackRegister(void (* CallbackHandler)(void));


#endif //TCA0_H