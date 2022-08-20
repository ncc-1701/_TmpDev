/* This is driver for proprietary TRS time-sync protocol
*/

/* Includes ----------------------------------------------------------------- */
/* Standard includes */
#include <stdint.h>
#include <stdbool.h>

/* Settings include */
#include "settings.h"

/* Hardware includes */
#include "stm32f4xx_hal.h"

/* Private constants -------------------------------------------------------- */
/* Hardware constants */
#define TRS_SYNC_PROTO_UART_BAUDRATE 	4800

/* Debug options ------------------------------------------------------------ */
//#define DEBUG_

/* Private variables -------------------------------------------------------- */
/* For transmitting bytes */
static uint8_t* pBuff;
static uint8_t dataLength = 0;

/* For USART configure */
static UART_HandleTypeDef UART_Handle;

/* Private function prototypes ---------------------------------------------- */
static void TxStateInit();
static bool SendBuffTx();

/* Private low-level and HAL functions -------------------------------------- */
static void Error_Handler();
static void UART_Init();
static void Internal_UART_DeInit();
static HAL_StatusTypeDef UART_CheckOnFlag(
		UART_HandleTypeDef *huart, uint32_t Flag, FlagStatus Status);
static void UART_TransmitByte(UART_HandleTypeDef *huart, uint8_t* pData);
static void UART_Receive_IT(UART_HandleTypeDef *huart);

/* Public functions --------------------------------------------------------- */
void TRS_SyncProtoDriverInit()
{
	/* Init HW */
	UART_Init();

	/* Init internal variables */
	TxStateInit();
}

#ifdef TRS_SNC_PRT_DIRECTION_FUNC
void TRS_SyncProtoDriverSetTransmitDirection(bool direction)
{
	#ifdef TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT
	if(direction)
	{
		HAL_GPIO_WritePin(TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT,
				TRS_SYNC_PROTO_UART_RX_TX_GPIO_PIN, GPIO_PIN_SET);
		return;
	}
	HAL_GPIO_WritePin(TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT,
			TRS_SYNC_PROTO_UART_RX_TX_GPIO_PIN, GPIO_PIN_RESET);
	#else /*!defined(TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT)*/
		#error "Undefined GPIO for TRS_SYNC_PROTO_UART_RX_TX"
	#endif /*TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT*/
}
#endif /*TRS_SNC_PRT_DIRECTION_FUNC*/

bool TRS_SyncProtoDriverSendTX_Buff(uint8_t *buff, uint8_t lenght)
{
	/* Check for empty state */
	if(dataLength != 0) return false;
	/* Nothing to send */
	if(lenght == 0) return false;

	/* Store buffer pointer and length */
	pBuff = buff;
	dataLength = lenght;

	/* Disable the UART Transmit Complete Interrupt */
	CLEAR_BIT(UART_Handle.Instance->CR1, USART_CR1_TCIE);
	/* Enable the UART Transmit data register empty Interrupt */
	SET_BIT(UART_Handle.Instance->CR1, USART_CR1_TXEIE);
	return true;
}

__attribute__((weak)) void TRS_SyncProtoDriverGetRX_Byte(uint8_t byte)
{
	(void)byte;
}

/* Private functions -------------------------------------------------------- */
static void TxStateInit()
{
	dataLength = 0;

	/* Disable the UART Transmit Complete and
	   the UART Transmit Complete Interrupt */
	CLEAR_BIT(UART_Handle.Instance->CR1, USART_CR1_TXEIE | USART_CR1_TCIE);
}

static bool SendBuffTx()
{
	if(dataLength != 0)
	{
		UART_TransmitByte(&UART_Handle, pBuff);
		pBuff ++;
		dataLength--;

		if(dataLength == 0)
		{
			/* Disable the UART Transmit Complete Interrupt */
			CLEAR_BIT(UART_Handle.Instance->CR1, USART_CR1_TXEIE);

			/* Enable the UART Transmit Complete Interrupt */
			SET_BIT(UART_Handle.Instance->CR1, USART_CR1_TCIE);
		}

		return true;
	}

	/* Disable the UART Transmit Complete Interrupt */
	CLEAR_BIT(UART_Handle.Instance->CR1, USART_CR1_TCIE);
	return false;
}

/* Low-level and HAL functions ---------------------------------------------- */
static void Error_Handler() {}

static void UART_Init()
{
	GPIO_InitTypeDef  GPIO_InitStruct;

	/* Deinit HW */
	Internal_UART_DeInit();

	/* Enable peripherals and GPIO Clocks */
#ifdef TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT
	TRS_SYNC_PROTO_UART_RX_TX_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = TRS_SYNC_PROTO_UART_RX_TX_GPIO_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed  = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT, &GPIO_InitStruct);

	#ifdef TRS_SYNC_PROTO_UART_SET_TX_DIRECT
	HAL_GPIO_WritePin(TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT,
			TRS_SYNC_PROTO_UART_RX_TX_GPIO_PIN, GPIO_PIN_SET);
	#else /*TRS_SYNC_PROTO_UART_SET_TX_DIRECT*/
	HAL_GPIO_WritePin(TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT,
			TRS_SYNC_PROTO_UART_RX_TX_GPIO_PIN, GPIO_PIN_RESET);
	#endif /*TRS_SYNC_PROTO_UART_SET_TX_DIRECT*/
#endif /*TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT*/

	/* Enable GPIO TX/RX clock */
	TRS_SYNC_PROTO_UART_TX_GPIO_CLK_ENABLE();
	TRS_SYNC_PROTO_UART_RX_GPIO_CLK_ENABLE();

	/* Enable TRS_SYNC_PROTO_UART clock */
	TRS_SYNC_PROTO_UART_CLK_ENABLE();

	/* Configure peripheral GPIO */
	/* UART TX GPIO pin configuration  */
	GPIO_InitStruct.Pin       = TRS_SYNC_PROTO_UART_TX_GPIO_PIN;
	GPIO_InitStruct.Mode      = TRS_SYNC_PROTO_UART_TX_GPIO_MODE;
	GPIO_InitStruct.Pull      = TRS_SYNC_PROTO_UART_RX_GPIO_PULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = TRS_SYNC_PROTO_UART_TX_AF;
	HAL_GPIO_Init(TRS_SYNC_PROTO_UART_TX_GPIO_PORT, &GPIO_InitStruct);

	/* UART RX GPIO pin configuration  */
	GPIO_InitStruct.Pin = TRS_SYNC_PROTO_UART_RX_GPIO_PIN;
	HAL_GPIO_Init(TRS_SYNC_PROTO_UART_RX_GPIO_PORT, &GPIO_InitStruct);

	UART_Handle.Instance        = TRS_SYNC_PROTO_UART;
	UART_Handle.Init.BaudRate   = TRS_SYNC_PROTO_UART_BAUDRATE;
	UART_Handle.Init.WordLength = UART_WORDLENGTH_8B;
	UART_Handle.Init.StopBits   = UART_STOPBITS_1;
	UART_Handle.Init.Parity     = UART_PARITY_NONE;
	UART_Handle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
	UART_Handle.Init.Mode       = UART_MODE_TX_RX;
	if(HAL_UART_Init(&UART_Handle) != HAL_OK)
	{
		Error_Handler();
	}

	/* Enable interrupts */
	/* NVIC configuration for USART, to catch the TX complete */
	HAL_NVIC_SetPriority(TRS_SYNC_PROTO_UART_IRQn,
			TRS_SYNC_PROTO_UART_I_PRIOR, 0);
	HAL_NVIC_EnableIRQ(TRS_SYNC_PROTO_UART_IRQn);

	/* Disable TXE, PE and ERR (Frame error, noise error, overrun error)
     * interrupts for the interrupt process */
	CLEAR_BIT(UART_Handle.Instance->CR1, (USART_CR1_PEIE |
			USART_CR1_TXEIE | USART_CR1_TCIE));
	/* Disable the UART Error Interrupt: (Frame error, noise error, overrun error) */
	CLEAR_BIT(UART_Handle.Instance->CR3, USART_CR3_EIE);

	/* Enable the UART Data Register not empty interrupt */
    SET_BIT(UART_Handle.Instance->CR1, USART_CR1_RXNEIE);
}

static void Internal_UART_DeInit()
{
	/* Reset peripherals */
	TRS_SYNC_PROTO_UART_FORCE_RESET();
	TRS_SYNC_PROTO_UART_RELEASE_RESET();

	/* Disable peripherals and GPIO Clocks */
	/* Configure UART Tx as alternate function  */
	HAL_GPIO_DeInit(TRS_SYNC_PROTO_UART_TX_GPIO_PORT,
			TRS_SYNC_PROTO_UART_TX_GPIO_PIN);
	/* Configure UART Rx as alternate function  */
	HAL_GPIO_DeInit(TRS_SYNC_PROTO_UART_RX_GPIO_PORT,
			TRS_SYNC_PROTO_UART_RX_GPIO_PIN);
#ifdef TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT
	HAL_GPIO_DeInit(TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT,
			TRS_SYNC_PROTO_UART_RX_TX_GPIO_PIN);
#endif /*TRS_SYNC_PROTO_UART_RX_TX_GPIO_PORT*/

	/* Disable the NVIC for UART */
	HAL_NVIC_DisableIRQ(TRS_SYNC_PROTO_UART_IRQn);

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

	if(UART_CheckOnFlag(huart, UART_FLAG_TXE, RESET) != HAL_OK)
	{
		/* Init internal variables */
		TxStateInit();
		return;
	}

	if(huart->Init.WordLength == UART_WORDLENGTH_9B)
	{
		tmp = (uint16_t*) pData;
		huart->Instance->DR = (*tmp & (uint16_t)0x01FF);
	}
	else huart->Instance->DR = (*pData & (uint8_t)0xFF);
}

static void UART_Receive_IT(UART_HandleTypeDef *huart)
{
	if(huart->Init.Parity == UART_PARITY_NONE)
	{
		TRS_SyncProtoDriverGetRX_Byte(
				(uint8_t)(huart->Instance->DR & (uint8_t)0x00FF));
	}
	else
	{
		TRS_SyncProtoDriverGetRX_Byte(
				(uint8_t)(huart->Instance->DR & (uint8_t)0x007F));
	}
}

#ifdef __cplusplus
extern "C" {
#endif
void TRS_SYNC_PROTO_UART_IRQHandler(void)
{
	uint32_t isrflags   = READ_REG(UART_Handle.Instance->SR);
	uint32_t cr1its     = READ_REG(UART_Handle.Instance->CR1);
	uint32_t errorflags = 0x00U;

	/* If no error occurs */
	errorflags = (isrflags & (uint32_t)(USART_SR_PE | USART_SR_FE |
			USART_SR_ORE | USART_SR_NE));

	/* UART in mode Receiver */
	if(((isrflags & USART_SR_RXNE) != RESET) &&
			((cr1its & USART_CR1_RXNEIE) != RESET))
	{
		UART_Receive_IT(&UART_Handle);
		return;
	}

	if(errorflags != RESET)
	{
		if((isrflags & USART_SR_ORE) != RESET)
		{
			/* Non Blocking error : transfer could go on. */
		}
		else
		{
			/* Something wrong: reset transmit state */
			TxStateInit();
			return;
		}
	}

	/* UART in mode Transmitter */
	if(((isrflags & USART_SR_TXE) != RESET) &&
			((cr1its & USART_CR1_TXEIE) != RESET))
	{
		SendBuffTx();
		return;
	}

	/* UART in mode Transmitter end */
	if(((isrflags & USART_SR_TC) != RESET) &&
			((cr1its & USART_CR1_TCIE) != RESET))
	{
		TxStateInit();
		return;
	}
}
#ifdef __cplusplus
}
#endif
