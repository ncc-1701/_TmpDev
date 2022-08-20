/* This is debug module with driver to send debug data through UART
*/

/* Includes ----------------------------------------------------------------- */
/* Standard includes */
#include <stdint.h>
#include <stdbool.h>

/* Hardware includes */
#include "stm32f4xx_hal.h"

/* Check for needing debug module */
#ifdef DEBUG_MODULES

/* Check for HW definitions */
#ifdef DEBUG_UART

/* Private variables -------------------------------------------------------- */
/* For USART configure */
static UART_HandleTypeDef UART_Handle;

/* Private function prototypes ---------------------------------------------- */
/* Private low-level and HAL functions -------------------------------------- */
static void Error_Handler();
static void UART_Init();
static void Internal_UART_DeInit();
static HAL_StatusTypeDef UART_CheckOnFlag(
		UART_HandleTypeDef *huart, uint32_t Flag, FlagStatus Status);
static void UART_TransmitByte(UART_HandleTypeDef *huart, uint8_t* pData);


/* Public functions --------------------------------------------------------- */
void PrintfUART_DriverInit()
{
	/* Init HW */
	UART_Init();
}

void PrintfUART_DriverSendChar(char ch)
{
	/* Loop until the end of transmission */
	while (UART_CheckOnFlag(&UART_Handle, UART_FLAG_TXE, RESET) != HAL_OK) ;
	
	// Send character
	UART_TransmitByte(&UART_Handle, (uint8_t*)&ch);
}

void PrintfUART_DriverPrint(const char *msg)
{
	for(uint16_t i = 0; msg[i] != 0; i ++) PrintfUART_DriverSendChar(msg[i]);
}

/* Private functions -------------------------------------------------------- */
/* Low-level and HAL functions ---------------------------------------------- */
static void Error_Handler() {}

static void UART_Init()
{
	GPIO_InitTypeDef  GPIO_InitStruct;

	/* Deinit HW */
	Internal_UART_DeInit();

	/* Enable peripherals and GPIO Clocks */
#ifdef DEBUG_UART_RX_TX_GPIO_PORT
	DEBUG_UART_RX_TX_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = DEBUG_UART_RX_TX_GPIO_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed  = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(DEBUG_UART_RX_TX_GPIO_PORT, &GPIO_InitStruct);
	HAL_GPIO_WritePin(DEBUG_UART_RX_TX_GPIO_PORT,
			DEBUG_UART_RX_TX_GPIO_PIN, GPIO_PIN_SET);
#endif /* DEBUG_UART_RX_TX_GPIO_PORT */

	/* Enable GPIO TX/RX clock */
	DEBUG_UART_TX_GPIO_CLK_ENABLE();
	/*DEBUG_UART_RX_GPIO_CLK_ENABLE();*/

	/* Enable DEBUG_UART clock */
	DEBUG_UART_CLK_ENABLE();

	/* Configure peripheral GPIO */
	/* UART TX GPIO pin configuration  */
	GPIO_InitStruct.Pin       = DEBUG_UART_TX_GPIO_PIN;
	GPIO_InitStruct.Mode      = DEBUG_UART_TX_GPIO_MODE;
	GPIO_InitStruct.Pull      = DEBUG_UART_RX_GPIO_PULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = DEBUG_UART_RX_TX_AF;
	HAL_GPIO_Init(DEBUG_UART_TX_GPIO_PORT, &GPIO_InitStruct);

	/* UART RX GPIO pin configuration  */
	/*GPIO_InitStruct.Pin = DEBUG_UART_RX_GPIO_PIN;
	HAL_GPIO_Init(DEBUG_UART_RX_GPIO_PORT, &GPIO_InitStruct);*/

	UART_Handle.Instance        = DEBUG_UART;
	UART_Handle.Init.BaudRate   = DEBUG_UART_BAUDRATE;
	UART_Handle.Init.WordLength = UART_WORDLENGTH_8B;
	UART_Handle.Init.StopBits   = UART_STOPBITS_1;
	UART_Handle.Init.Parity     = UART_PARITY_NONE;
	UART_Handle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
	UART_Handle.Init.Mode       = UART_MODE_TX;
	if(HAL_UART_Init(&UART_Handle) != HAL_OK)
	{
		Error_Handler();
	}
}

static void Internal_UART_DeInit()
{
	/* Reset peripherals */
	DEBUG_UART_FORCE_RESET();
	DEBUG_UART_RELEASE_RESET();

	/* Disable peripherals and GPIO Clocks */
	/* Configure UART Tx as alternate function  */
	HAL_GPIO_DeInit(DEBUG_UART_TX_GPIO_PORT,
			DEBUG_UART_TX_GPIO_PIN);
	/* Configure UART Rx as alternate function  */
	/*HAL_GPIO_DeInit(DEBUG_UART_RX_GPIO_PORT,
			DEBUG_UART_RX_GPIO_PIN);*/
#ifdef DEBUG_UART_RX_TX_GPIO_PORT
	HAL_GPIO_DeInit(DEBUG_UART_RX_TX_GPIO_PORT,
			DEBUG_UART_RX_TX_PIN);
#endif /* DEBUG_UART_RX_TX_GPIO_PORT */

	if(HAL_UART_DeInit(&UART_Handle) != HAL_OK)
	{
		Error_Handler();
	}
}

static HAL_StatusTypeDef UART_CheckOnFlag(
		UART_HandleTypeDef *huart, uint32_t Flag, FlagStatus Status)
{
	/* Check the flag */
	if((__HAL_UART_GET_FLAG(huart, Flag) ? SET : RESET) == Status)
		return HAL_ERROR;
	return HAL_OK;
}

static void UART_TransmitByte(UART_HandleTypeDef *huart, uint8_t* pData)
{
	uint16_t* tmp;
	if(huart->Init.WordLength == UART_WORDLENGTH_9B)
	{
		tmp = (uint16_t*) pData;
		huart->Instance->DR = (*tmp & (uint16_t)0x01FF);
	}
	else huart->Instance->DR = (*pData & (uint8_t)0xFF);
}

#endif /*DEBUG_UART*/
#endif /*DEBUG_MODULES*/
