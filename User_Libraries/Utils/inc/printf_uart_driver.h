/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _PRINTF_DRIVER_H_
#define _PRINTF_DRIVER_H_

/* Includes ----------------------------------------------------------------- */
/* Standard includes */
#include <stdint.h>

/* Hardware includes */
#include "stm32f4xx_hal.h"

/* Check for needing debug module */
#ifdef DEBUG_MODULES
/* Check for HW definitions */
#ifdef DEBUG_UART

/* Public functions prototypes -----------------------------------------------*/
/* Init driver functions */
void PrintfUART_DriverInit();

/* Print functions */
void PrintfUART_DriverSendChar(char ch);
void PrintfUART_DriverPrint(const char *msg);

#endif /*DEBUG_UART*/
#endif /*DEBUG_MODULES*/
#endif /*_PRINTF_DRIVER_H_*/
