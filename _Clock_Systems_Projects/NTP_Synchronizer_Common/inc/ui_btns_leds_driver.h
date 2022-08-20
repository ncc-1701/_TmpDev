/* It is driver for parallel IO exchange for STM32F4X families MCU */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _UI_BTNS_LEDS_DRIVER_H_
#define _UI_BTNS_LEDS_DRIVER_H_

/* Includes ------------------------------------------------------------------*/
/* Standard includes */
#include <stdint.h>

/* Public functions prototypes -----------------------------------------------*/
/* Init and loop driver functions */
void UI_BtnsLedsDriverInit();
void UI_BtnsLedsDriverTask();

/* Buttons' functions */
bool GetResetButtonState();

/* LED-indicators' functions */
void SetStatusLED_State(bool state);

#endif /* _UI_BTNS_LEDS_DRIVER_H_ */
