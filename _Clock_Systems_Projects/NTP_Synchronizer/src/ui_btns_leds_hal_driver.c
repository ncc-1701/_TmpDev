/* It is driver for parallel IO exchange for STM32F4X families MCU */

/* Includes ------------------------------------------------------------------*/
/* Standard includes */
#include <stdint.h>
#include <stdbool.h>

/* Hardware includes */
#include "stm32f4xx_hal.h"

/* Private function prototypes -----------------------------------------------*/

/* Public functions ----------------------------------------------------------*/
void UI_BtnsLedsDriverInit()
{
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

	/* Buttons IO init */
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Pin = BUTTON_RESET_GPIO_PIN;

	BUTTON_RESET_GPIO_CLK_ENABLE();
	HAL_GPIO_Init(BUTTON_RESET_GPIO_PORT, &GPIO_InitStruct);

#ifndef LED_STATUS_COMMON_ANODE
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
#else /* LED_STATUS_COMMON_ANODE */
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
#endif /* LED_STATUS_COMMON_ANODE */

	/* Leds IO init */
	LED_STATUS_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = LED_STATUS_GPIO_PIN;
	HAL_GPIO_Init(LED_STATUS_GPIO_PORT, &GPIO_InitStruct);
}

void UI_BtnsLedsDriverTask() {}

/* Buttons' functions */
bool GetResetButtonState()
{
	if(HAL_GPIO_ReadPin(BUTTON_RESET_GPIO_PORT, BUTTON_RESET_GPIO_PIN) ==
	   GPIO_PIN_RESET) return true;
	return false;
}

/* LED-indicators' functions */
void SetStatusLED_State(bool state)
{
	if(state)
	{
#ifndef LED_STATUS_COMMON_ANODE
		HAL_GPIO_WritePin(LED_STATUS_GPIO_PORT,
				LED_STATUS_GPIO_PIN, GPIO_PIN_SET);
#else /* LED_STATUS_COMMON_ANODE */
		HAL_GPIO_WritePin(LED_STATUS_GPIO_PORT,
				LED_STATUS_GPIO_PIN, GPIO_PIN_RESET);
#endif /* LED_STATUS_COMMON_ANODE */
		return;
	}

#ifndef LED_STATUS_COMMON_ANODE
	HAL_GPIO_WritePin(LED_STATUS_GPIO_PORT,
			LED_STATUS_GPIO_PIN, GPIO_PIN_RESET);
#else /* LED_STATUS_COMMON_ANODE */
	HAL_GPIO_WritePin(LED_STATUS_GPIO_PORT,
			LED_STATUS_GPIO_PIN, GPIO_PIN_SET);
#endif /* LED_STATUS_COMMON_ANODE */
}
