/* This is Eth-Phy driver source for using with FreeRTOS
   (only for STM32F4X series)
*/

// Includes --------------------------------------------------------------------
// Standard includes.
#include <stdint.h>

// FreeRTOS includes.
#include "FreeRTOS.h"
#include "task.h"

// FreeRTOS+TCP includes.
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"

// ST includes.
#include "stm32f4xx_hal.h"

// Private constants and macroses ----------------------------------------------
// The time to proper reset transmit chip, ms
#define TIME_ETH_CHIP_RESET_ON 		50
#define TIME_ETH_CHIP_RESET_OFF 	30

// Max blocking recv time
#define MAX_RECV_BLOCK_TIME_OUT 	100

#ifndef	PHY_LS_HIGH_CHECK_TIME_MS
	// Check if the LinkSStatus in the PHY is still high after 15 seconds of not
	// receiving packets.
	#define PHY_LS_HIGH_CHECK_TIME_MS	20000// 6000//
#endif

#ifndef	PHY_LS_LOW_CHECK_TIME_MS
	// Check if the LinkSStatus in the PHY is still low
	#define PHY_LS_LOW_CHECK_TIME_MS	8000// 1000//
#endif

/* Enumeration for PHY link up/down events. */
enum PHY_LinkStates
{
	PHY_LinkUpState,
	PHY_LinkDownState,
	PHY_LinkReinitedState
};

// Interrupt events to process.  Currently only the Rx event is processed
// although code for other events is included to allow for possible future
// expansion.
#define EMAC_IF_RX_EVENT        1UL
#define EMAC_IF_TX_EVENT        2UL
#define EMAC_IF_ERR_EVENT       4UL
#define EMAC_IF_ALL_EVENT       (EMAC_IF_RX_EVENT | EMAC_IF_TX_EVENT | EMAC_IF_ERR_EVENT)

#define ETH_DMA_ALL_INTS \
	(ETH_DMA_IT_TST | ETH_DMA_IT_PMT | ETH_DMA_IT_MMC | ETH_DMA_IT_NIS | ETH_DMA_IT_AIS | ETH_DMA_IT_ER | \
	  ETH_DMA_IT_FBE | ETH_DMA_IT_ET | ETH_DMA_IT_RWT | ETH_DMA_IT_RPS | ETH_DMA_IT_RBU | ETH_DMA_IT_R | \
	  ETH_DMA_IT_TU | ETH_DMA_IT_RO | ETH_DMA_IT_TJT | ETH_DMA_IT_TPS | ETH_DMA_IT_T)

/*
 * Most users will want a PHY that negotiates about
 * the connection properties: speed, dmix and duplex.
 */
#if !defined(ipconfigETHERNET_AN_ENABLE)
	/* Enable auto-negotiation */
	#define ipconfigETHERNET_AN_ENABLE				1
#endif

#if !defined(ipconfigETHERNET_AUTO_CROSS_ENABLE)
	#define ipconfigETHERNET_AUTO_CROSS_ENABLE		1
#endif

#if(ipconfigETHERNET_AN_ENABLE == 0)
	/*
	 * The following three defines are only used in case there
	 * is no auto-negotiation.
	 */
	#if !defined(ipconfigETHERNET_CROSSED_LINK)
		#define	ipconfigETHERNET_CROSSED_LINK			1
	#endif

	#if !defined(ipconfigETHERNET_USE_100MB)
		#define ipconfigETHERNET_USE_100MB				1
	#endif

	#if !defined(ipconfigETHERNET_USE_FULL_DUPLEX)
		#define ipconfigETHERNET_USE_FULL_DUPLEX		1
	#endif
#endif /* ipconfigETHERNET_AN_ENABLE == 0 */

/* Default the size of the stack used by the EMAC deferred handler task to twice
the size of the stack used by the idle task - but allow this to be overridden in
FreeRTOSConfig.h as configMINIMAL_STACK_SIZE is a user definable constant. */
#ifndef configEMAC_TASK_STACK_SIZE
	#define configEMAC_TASK_STACK_SIZE (2 * configMINIMAL_STACK_SIZE)
#endif

/* Debug options -------------------------------------------------------------*/
//#define DEBUG_ETH_INIT_SEQUENCE

/*-----------------------------------------------------------*/

/*
 * A deferred interrupt handler task that processes
 */
static void prvEMACHandlerTask(void *pvParameters);

/*
 * See if there is a new packet and forward it to the IP-task.
 */
static BaseType_t prvNetworkInterfaceInput(void);

static HAL_StatusTypeDef ReinitEthernet();
static void ethernetif_reset_chip(void* arg);
/*-----------------------------------------------------------*/

/* Bit map of outstanding ETH interrupt events for processing.  Currently only
the Rx interrupt is handled, although code is included for other events to
enable future expansion. */
static volatile uint32_t ulISREvents;

/* A copy of PHY register 1: 'PHY_BSR' */
static uint32_t ulPHYLinkStatus = 0;

/* Ethernet handle. */
static ETH_HandleTypeDef hETH;
/* Ethernet states. */
static HAL_StatusTypeDef hal_eth_init_status;
static volatile enum PHY_LinkStates PHY_LinkState = PHY_LinkDownState;

/* Value to be written into the 'Basic mode Control Register'. */
//static uint32_t ulBCRvalue;

/* Value to be written into the 'Advertisement Control Register'. */
//static uint32_t ulACRValue;

/* Holds the handle of the task used as a deferred interrupt processor.  The
handle is used so direct notifications can be sent to the task for all EMAC/DMA
related interrupts. */
static TaskHandle_t xEMACTaskHandle = NULL;

/* MAC buffers: ---------------------------------------------------------*/
__ALIGN_BEGIN ETH_DMADescTypeDef  DMARxDscrTab[ ETH_RXBUFNB ] __ALIGN_END;/* Ethernet Rx MA Descriptor */
__ALIGN_BEGIN ETH_DMADescTypeDef  DMATxDscrTab[ ETH_TXBUFNB ] __ALIGN_END;/* Ethernet Tx DMA Descriptor */
__ALIGN_BEGIN uint8_t Rx_Buff[ ETH_RXBUFNB ][ ETH_RX_BUF_SIZE ] __ALIGN_END; /* Ethernet Receive Buffer */
__ALIGN_BEGIN uint8_t Tx_Buff[ ETH_TXBUFNB ][ ETH_TX_BUF_SIZE ] __ALIGN_END; /* Ethernet Transmit Buffer */

/*-----------------------------------------------------------*/

void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *hETH)
{
BaseType_t xHigherPriorityTaskWoken = 0;
	// Prevent warning for unused parameters
	(void)hETH;
	/* Ethernet RX-Complete callback function, elsewhere declared as weak. */
    ulISREvents |= EMAC_IF_RX_EVENT;
	if(xEMACTaskHandle != NULL)
	{
		vTaskNotifyGiveFromISR(xEMACTaskHandle, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}
/*-----------------------------------------------------------*/

void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *hETH)
{
	/* This call-back would only be useful in case packets are being sent
	zero-copy.  Once they're sent, the buffers must be released. */
	// Prevent warning for unused parameters
	(void)hETH;
}
/*-----------------------------------------------------------*/

void xNetworkInterfaceForceInitialise()
{
	/* Emulate previous PHY link down state */
	PHY_LinkState = PHY_LinkDownState;
}
BaseType_t xNetworkInterfaceInitialise(void)
{
	if(xEMACTaskHandle == NULL)
	{
		// Init ETH
		hETH.Instance = ETH;
		hETH.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
		hETH.Init.PhyAddress = 1;

		// Set MAC
		extern uint8_t ucMACAddress[];
		hETH.Init.MACAddr = (uint8_t*)ucMACAddress;
		hETH.Init.RxMode = ETH_RXINTERRUPT_MODE;
		hETH.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
		hETH.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;

#if defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)
#	ifdef TEST_0_GPIO_PORT
		HAL_GPIO_WritePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN, GPIO_PIN_SET);
#	endif /*TEST_0_GPIO_PORT*/
#endif /*defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)*/

		hal_eth_init_status = HAL_ETH_Init(&hETH);

#if defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)
#	ifdef TEST_0_GPIO_PORT
		HAL_GPIO_WritePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN, GPIO_PIN_RESET);
#	endif /*TEST_0_GPIO_PORT*/
#endif /*defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)*/

		// Only for inspection by debugger
		(void) hal_eth_init_status;

		// Initialize Tx Descriptors list: Chain Mode
		HAL_ETH_DMATxDescListInit(&hETH, DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);

		// Initialize Rx Descriptors list: Chain Mode
		HAL_ETH_DMARxDescListInit(&hETH, DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);

		// Configure PHY to generate an interrupt when Eth Link state changes
		uint32_t regvalue = 0;

		// Read Register Configuration
		HAL_ETH_ReadPHYRegister(&hETH, PHY_MICR, &regvalue);

		regvalue |= (PHY_MICR_INT_EN | PHY_MICR_INT_OE);

		// Enable Interrupts
		HAL_ETH_WritePHYRegister(&hETH, PHY_MICR, regvalue);

		// Read Register Configuration
		HAL_ETH_ReadPHYRegister(&hETH, PHY_MISR, &regvalue);

		regvalue |= PHY_MISR_LINK_INT_EN;

		// Enable Interrupt on change of link status
		HAL_ETH_WritePHYRegister(&hETH, PHY_MISR, regvalue);

#if defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)
#	ifdef TEST_0_GPIO_PORT
		HAL_GPIO_WritePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN, GPIO_PIN_SET);
#	endif /*TEST_0_GPIO_PORT*/
#endif /*defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)*/

		// Enable MAC and DMA transmission and reception
		HAL_ETH_Start(&hETH);

#if defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)
#	ifdef TEST_0_GPIO_PORT
		HAL_GPIO_WritePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN, GPIO_PIN_RESET);
#	endif /*TEST_0_GPIO_PORT*/
#endif /*defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)*/

		/* The deferred interrupt handler task is created at the highest
		possible priority to ensure the interrupt handler can return directly
		to it.  The task's handle is stored in xEMACTaskHandle so interrupts can
		notify the task when there is something to process. */
		xTaskCreate(prvEMACHandlerTask, "EMAC", configEMAC_TASK_STACK_SIZE, 
					NULL, configMAX_PRIORITIES - 1, &xEMACTaskHandle);
	}
	else
	{
		/* Get PHY link status */
		HAL_ETH_ReadPHYRegister(&hETH, PHY_BSR, &ulPHYLinkStatus);

		/* Check for PHY link up event */
		if(ulPHYLinkStatus & PHY_LINKED_STATUS)
		{
			/* Check for last PHY link state */
			if(PHY_LinkState != PHY_LinkUpState)
			{
				/* PHY link is up */
				/* Suspend task */
				vTaskSuspend(xEMACTaskHandle);

				ReinitEthernet();

				/* Change PHY link state */
				PHY_LinkState = PHY_LinkReinitedState;

				/* Resume task */
				vTaskResume(xEMACTaskHandle);
			}
		}
	}
	
	/* Get PHY link status */
	HAL_ETH_ReadPHYRegister(&hETH, PHY_BSR, &ulPHYLinkStatus);

	/* Check for PHY link up event */
	if(ulPHYLinkStatus & PHY_LINKED_STATUS)
	{
		/* Change PHY link state */
		PHY_LinkState = PHY_LinkUpState;
		FreeRTOS_printf(("prvEMACHandlerTask: PHY link now is up"));
	}
	
	// When returning non-zero, the stack will become active and
    // start DHCP (in configured)
	return (ulPHYLinkStatus & PHY_LINKED_STATUS) != 0;
}
/*-----------------------------------------------------------*/

BaseType_t xNetworkInterfaceOutput(
	xNetworkBufferDescriptor_t* const pxDescriptor, 
	BaseType_t bReleaseAfterSend)
{
BaseType_t xReturn;
uint32_t ulTransmitSize = 0;
__IO ETH_DMADescTypeDef *pxDmaTxDesc;

	#if(ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM != 0)
	{
	ProtocolPacket_t *pxPacket;

		/* If the peripheral must calculate the checksum, it wants
		the protocol checksum to have a value of zero. */
		pxPacket = (ProtocolPacket_t *) (pxDescriptor->pucEthernetBuffer);

		if(pxPacket->xICMPPacket.xIPHeader.ucProtocol == ipPROTOCOL_ICMP)
		{
			pxPacket->xICMPPacket.xICMPHeader.usChecksum = (uint16_t)0u;
		}
	}
	#endif

	if((ulPHYLinkStatus & PHY_LINKED_STATUS) != 0)
	{
		/* This function does the actual transmission of the packet. The packet is
		contained in 'pxDescriptor' that is passed to the function. */
		pxDmaTxDesc = hETH.TxDesc;

		/* Is this buffer available? */
		if((pxDmaTxDesc->Status & ETH_DMATXDESC_OWN) != 0)
		{
			xReturn = pdFAIL;
		}
		else
		{
			/* Get bytes in current buffer. */
			ulTransmitSize = pxDescriptor->xDataLength;

			if(ulTransmitSize > ETH_TX_BUF_SIZE)
			{
				ulTransmitSize = ETH_TX_BUF_SIZE;
			}

			/* Copy the remaining bytes */
			memcpy((void *) pxDmaTxDesc->Buffer1Addr, pxDescriptor->pucEthernetBuffer, ulTransmitSize);

			/* Prepare transmit descriptors to give to DMA. */
			HAL_ETH_TransmitFrame(&hETH, ulTransmitSize);

			iptraceNETWORK_INTERFACE_TRANSMIT();
			xReturn = pdPASS;
		}
	}
	else
	{
		/* The PHY has no Link Status, packet shall be dropped. */
		xReturn = pdFAIL;
	}

	#if(ipconfigZERO_COPY_TX_DRIVER == 0)
	{
		/* The buffer has been sent so can be released. */
		if(bReleaseAfterSend != pdFALSE)
		{
			vReleaseNetworkBufferAndDescriptor(pxDescriptor);
		}
	}
	#endif

	return xReturn;
}
/*-----------------------------------------------------------*/

static BaseType_t prvNetworkInterfaceInput(void)
{
xNetworkBufferDescriptor_t *pxDescriptor;
uint16_t usReceivedLength;
__IO ETH_DMADescTypeDef *xDMARxDescriptor;
uint32_t ulSegCount;
xIPStackEvent_t xRxEvent = { eNetworkRxEvent, NULL };
const TickType_t xDescriptorWaitTime = pdMS_TO_TICKS(250);

	/* get received frame */
	if(HAL_ETH_GetReceivedFrame(&hETH) != HAL_OK)
	{
		usReceivedLength = 0;
	}
	else
	{
		/* Obtain the size of the packet and put it into the "usReceivedLength" variable. */
		usReceivedLength = hETH.RxFrameInfos.length;

		if(usReceivedLength > 0)
		{
			/* Create a buffer of the required length. */
			pxDescriptor = pxGetNetworkBufferWithDescriptor(usReceivedLength, xDescriptorWaitTime);

			if(pxDescriptor != NULL)
			{
				xDMARxDescriptor = hETH.RxFrameInfos.FSRxDesc;

				/* Copy remaining data. */
				if(usReceivedLength > pxDescriptor->xDataLength)
				{
					usReceivedLength = pxDescriptor->xDataLength;
				}

				memcpy(pxDescriptor->pucEthernetBuffer, (uint8_t *) hETH.RxFrameInfos.buffer, usReceivedLength);

				xRxEvent.pvData = (void *) pxDescriptor;

				/* Pass the data to the TCP/IP task for processing. */
				if(xSendEventStructToIPTask(&xRxEvent, xDescriptorWaitTime) == pdFALSE)
				{
					/* Could not send the descriptor into the TCP/IP stack, it
					must be released. */
					vReleaseNetworkBufferAndDescriptor(pxDescriptor);
					iptraceETHERNET_RX_EVENT_LOST();
				}
				else
				{
					iptraceNETWORK_INTERFACE_RECEIVE();
				}

				/* Release descriptors to DMA.  Point to first descriptor. */
				xDMARxDescriptor = hETH.RxFrameInfos.FSRxDesc;
				ulSegCount = hETH.RxFrameInfos.SegCount;

				/* Set Own bit in RX descriptors: gives the buffers back to
				DMA. */
				while(ulSegCount != 0)
				{
					xDMARxDescriptor->Status |= ETH_DMARXDESC_OWN;
					xDMARxDescriptor = (ETH_DMADescTypeDef *) xDMARxDescriptor->Buffer2NextDescAddr;
					ulSegCount--;
				}

				/* Clear Segment_Count */
				hETH.RxFrameInfos.SegCount = 0;
			}
			else
			{
				FreeRTOS_printf(("prvNetworkInterfaceInput: pxGetNetworkBuffer failed length %u\n", usReceivedLength));
			}
		}
		else
		{
			FreeRTOS_printf(("prvNetworkInterfaceInput: zero-sized packet?\n"));
			pxDescriptor = NULL;
		}

		/* When Rx Buffer unavailable flag is set clear it and resume
		reception. */
		if((hETH.Instance->DMASR & ETH_DMASR_RBUS) != 0)
		{
			/* Clear RBUS ETHERNET DMA flag. */
			hETH.Instance->DMASR = ETH_DMASR_RBUS;

			/* Resume DMA reception. */
			hETH.Instance->DMARPDR = 0;
		}
	}

	if(usReceivedLength > 0) return pdTRUE;
	return pdFALSE;
}
/*-----------------------------------------------------------*/

BaseType_t xGetPhyLinkStatus(void)
{
BaseType_t xReturn;

	if((ulPHYLinkStatus & PHY_LINKED_STATUS) != 0)
	{
		xReturn = pdPASS;
	}
	else
	{
		xReturn = pdFAIL;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

static void prvEMACHandlerTask(void *pvParameters)
{
	TimeOut_t xPhyTime;
	TickType_t xPhyRemTime;
	TickType_t xRecBlockTimeOut;

	UBaseType_t uxLastMinBufferCount = 0;
	UBaseType_t uxCurrentCount;
	BaseType_t xResult;
	const TickType_t ulMaxBlockTime = pdMS_TO_TICKS(100UL);

	// Remove compiler warnings about unused parameters.
	(void)pvParameters;

	vTaskSetTimeOutState(&xPhyTime);
	xPhyRemTime = pdMS_TO_TICKS(PHY_LS_LOW_CHECK_TIME_MS);

	for(;;)
	{
		xResult = 0;
		uxCurrentCount = uxGetMinimumFreeNetworkBuffers();
		if(uxLastMinBufferCount != uxCurrentCount)
		{
			// The logging produced below may be helpful
			// while tuning +TCP: see how many buffers are in use.
			uxLastMinBufferCount = uxCurrentCount;
			FreeRTOS_printf(("Network buffers: %lu lowest %lu\n",
				uxGetNumberOfFreeNetworkBuffers(), uxCurrentCount));
		}

#if(ipconfigCHECK_IP_QUEUE_SPACE != 0)
		{
			uxCurrentCount = uxGetMinimumIPQueueSpace();
			if(uxLastMinQueueSpace != uxCurrentCount)
			{
				// The logging produced below may be helpful
				// while tuning +TCP: see how many buffers are in use.
				uxLastMinQueueSpace = uxCurrentCount;
				FreeRTOS_printf(("Queue space: lowest %lu\n", uxCurrentCount));
			}
		}
#endif // ipconfigCHECK_IP_QUEUE_SPACE

		if((ulISREvents & EMAC_IF_ALL_EVENT) == 0)
		{
			// No events to process now, wait for the next.
			ulTaskNotifyTake(pdFALSE, ulMaxBlockTime);
		}

		if((ulISREvents & EMAC_IF_RX_EVENT) != 0)
		{
			ulISREvents &= ~EMAC_IF_RX_EVENT;

			xResult = prvNetworkInterfaceInput();
			if(xResult > 0)
			{
				uint16_t tooLongRecv = 0;
				xRecBlockTimeOut = xTaskGetTickCount();
				while(	(prvNetworkInterfaceInput() > 0)/* &&
						(FreeRTOS_IsNetworkUp() != pdFALSE)*/)
				{
					// Keep reading receiving data until timeout
					if(xTaskGetTickCount() - xRecBlockTimeOut >
							pdMS_TO_TICKS(MAX_RECV_BLOCK_TIME_OUT))
					{
						// Try to free RTOS for another tasks
						// before reset MCU,
						tooLongRecv++;
						if(tooLongRecv >= 10)
						{
							// Something wrong, reset MCU
							NVIC_SystemReset();
						}

						xRecBlockTimeOut = xTaskGetTickCount();
						vTaskDelay(1);
					}
				}
			}
		}

		if((ulISREvents & EMAC_IF_TX_EVENT) != 0)
		{
			// Future extension: code to release TX buffers 
			// if zero-copy is used.
			ulISREvents &= ~EMAC_IF_TX_EVENT;
		}

		if((ulISREvents & EMAC_IF_ERR_EVENT) != 0)
		{
			// Future extension: logging about errors that occurred.
			ulISREvents &= ~EMAC_IF_ERR_EVENT;
		}

		if(xResult > 0)
		{
			// A packet was received. No need to check for the PHY status now,
			// but set a timer to check it later on.
			vTaskSetTimeOutState(&xPhyTime);
			xPhyRemTime = pdMS_TO_TICKS(PHY_LS_HIGH_CHECK_TIME_MS);
			xResult = 0;
		}
		else if(xTaskCheckForTimeOut(&xPhyTime, &xPhyRemTime) != pdFALSE)
		{
			vTaskSetTimeOutState(&xPhyTime);

			/* Get PHY link status */
			HAL_ETH_ReadPHYRegister(&hETH, PHY_BSR, &ulPHYLinkStatus);

			/* Check for PHY link up/down state */
			if(ulPHYLinkStatus & PHY_LINKED_STATUS)
				xPhyRemTime = pdMS_TO_TICKS(PHY_LS_HIGH_CHECK_TIME_MS);
			else
			{
				xPhyRemTime = pdMS_TO_TICKS(PHY_LS_LOW_CHECK_TIME_MS);

				/* Check for PHY link up event: use last PHY link state */
				if(PHY_LinkState == PHY_LinkUpState)
				{
					/* Change PHY link state */
					PHY_LinkState = PHY_LinkDownState;
					FreeRTOS_printf(("\
prvEMACHandlerTask: PHY link now is down"));

					/* Send NetworkDown event */
					FreeRTOS_NetworkDown();
				}
			}

			/* Make delay after Eth fault */
			vTaskDelay(100);
		}
	}
}
/*-----------------------------------------------------------*/

static HAL_StatusTypeDef ReinitEthernet()
{
	// Stop MAC interface
	HAL_ETH_Stop(&hETH);
	HAL_ETH_DeInit(&hETH);

#if defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)
#	ifdef TEST_0_GPIO_PORT
		HAL_GPIO_WritePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN, GPIO_PIN_SET);
#	endif /*TEST_0_GPIO_PORT*/
#endif /*defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)*/

	hal_eth_init_status = HAL_ETH_Init(&hETH);

#if defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)
#	ifdef TEST_0_GPIO_PORT
		HAL_GPIO_WritePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN, GPIO_PIN_RESET);
#	endif /*TEST_0_GPIO_PORT*/
#endif /*defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)*/

	if(hal_eth_init_status != HAL_OK) return hal_eth_init_status;
	
	// Initialize Tx Descriptors list: Chain Mode
	HAL_ETH_DMATxDescListInit(&hETH, DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);

	// Initialize Rx Descriptors list: Chain Mode
	HAL_ETH_DMARxDescListInit(&hETH, DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);

	// Configure PHY to generate an interrupt when Eth Link state changes
	uint32_t regvalue = 0;
	
	// Read Register Configuration
	HAL_ETH_ReadPHYRegister(&hETH, PHY_MICR, &regvalue);

	regvalue |= (PHY_MICR_INT_EN | PHY_MICR_INT_OE);

	// Enable Interrupts
	HAL_ETH_WritePHYRegister(&hETH, PHY_MICR, regvalue);

	// Read Register Configuration
	HAL_ETH_ReadPHYRegister(&hETH, PHY_MISR, &regvalue);

	regvalue |= PHY_MISR_LINK_INT_EN;

	// Enable Interrupt on change of link status
	HAL_ETH_WritePHYRegister(&hETH, PHY_MISR, regvalue);
	
#if defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)
#	ifdef TEST_0_GPIO_PORT
		HAL_GPIO_WritePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN, GPIO_PIN_SET);
#	endif /*TEST_0_GPIO_PORT*/
#endif /*defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)*/

	// Enable MAC and DMA transmission and reception
	HAL_ETH_Start(&hETH);
	
#if defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)
#	ifdef TEST_0_GPIO_PORT
		HAL_GPIO_WritePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN, GPIO_PIN_RESET);
#	endif /*TEST_0_GPIO_PORT*/
#endif /*defined(DEBUG_MODULES) && defined(DEBUG_ETH_INIT_SEQUENCE)*/

	return HAL_OK;
}
/*-----------------------------------------------------------*/

// low-level functions ---------------------------------------------------------
void HAL_ETH_MspInit(ETH_HandleTypeDef* ethHandle)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	if(ethHandle->Instance==ETH)
	{
		// At first init Eth reset pin
#ifdef ETH_NRST_GPIO_PORT
		ETH_NRST_GPIO_CLK_ENABLE();
		GPIO_InitStruct.Pin = ETH_NRST_GPIO_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(ETH_NRST_GPIO_PORT, &GPIO_InitStruct);
#endif // ETH_NRST_GPIO_PORT

		// Enable Peripheral clock
		__HAL_RCC_ETH_CLK_ENABLE();

		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;

		ETH_MDC_GPIO_CLK_ENABLE();
		GPIO_InitStruct.Pin = ETH_MDC_GPIO_PIN;
		HAL_GPIO_Init(ETH_MDC_GPIO_PORT, &GPIO_InitStruct);

		ETH_MDIO_GPIO_CLK_ENABLE();
		GPIO_InitStruct.Pin = ETH_MDIO_GPIO_PIN;
		HAL_GPIO_Init(ETH_MDIO_GPIO_PORT, &GPIO_InitStruct);
		
		ETH_TX_EN_GPIO_CLK_ENABLE();
		GPIO_InitStruct.Pin = ETH_TX_EN_GPIO_PIN;
		HAL_GPIO_Init(ETH_TX_EN_GPIO_PORT, &GPIO_InitStruct);
		
		ETH_TXD0_GPIO_CLK_ENABLE();
		GPIO_InitStruct.Pin = ETH_TXD0_GPIO_PIN;
		HAL_GPIO_Init(ETH_TXD0_GPIO_PORT, &GPIO_InitStruct);

		ETH_TXD1_GPIO_CLK_ENABLE();
		GPIO_InitStruct.Pin = ETH_TXD1_GPIO_PIN;
		HAL_GPIO_Init(ETH_TXD1_GPIO_PORT, &GPIO_InitStruct);

		ETH_REF_CLK_GPIO_CLK_ENABLE();
		GPIO_InitStruct.Pin = ETH_REF_CLK_GPIO_PIN;
		HAL_GPIO_Init(ETH_REF_CLK_GPIO_PORT, &GPIO_InitStruct);

		ETH_CRS_DV_GPIO_CLK_ENABLE();
		GPIO_InitStruct.Pin = ETH_CRS_DV_GPIO_PIN;
		HAL_GPIO_Init(ETH_CRS_DV_GPIO_PORT, &GPIO_InitStruct);
		
		ETH_RXD0_GPIO_CLK_ENABLE();
		GPIO_InitStruct.Pin = ETH_RXD0_GPIO_PIN;
		HAL_GPIO_Init(ETH_RXD0_GPIO_PORT, &GPIO_InitStruct);

		ETH_RXD1_GPIO_CLK_ENABLE();
		GPIO_InitStruct.Pin = ETH_RXD1_GPIO_PIN;
		HAL_GPIO_Init(ETH_RXD1_GPIO_PORT, &GPIO_InitStruct);
		
#ifdef ETH_NRST_GPIO_PORT
		ethernetif_reset_chip(NULL);
#endif // ETH_NRST_GPIO_PORT
		
#ifdef ETH_NEED_REMAP
		__HAL_AFIO_REMAP_ETH_ENABLE();
#endif // ETH_NEED_REMAP

		// Peripheral interrupt init
		HAL_NVIC_SetPriority(ETH_IRQn, ETH_I_PRIOR, 0);
		HAL_NVIC_EnableIRQ(ETH_IRQn);
	}
}

void HAL_ETH_MspDeInit(ETH_HandleTypeDef* ethHandle)
{
	if(ethHandle->Instance==ETH)
	{
		// Disable Peripheral clock
		__HAL_RCC_ETH_CLK_DISABLE();

		// ETH GPIO Configuration    
		HAL_GPIO_DeInit(ETH_MDC_GPIO_PORT, ETH_MDC_GPIO_PIN);
		HAL_GPIO_DeInit(ETH_REF_CLK_GPIO_PORT, ETH_REF_CLK_GPIO_PIN);
		HAL_GPIO_DeInit(ETH_MDIO_GPIO_PORT, ETH_MDIO_GPIO_PIN);
		HAL_GPIO_DeInit(ETH_TX_EN_GPIO_PORT, ETH_TX_EN_GPIO_PIN);
		HAL_GPIO_DeInit(ETH_TXD0_GPIO_PORT, ETH_TXD0_GPIO_PIN);
		HAL_GPIO_DeInit(ETH_TXD1_GPIO_PORT, ETH_TXD1_GPIO_PIN);
		HAL_GPIO_DeInit(ETH_CRS_DV_GPIO_PORT, ETH_CRS_DV_GPIO_PIN);
		HAL_GPIO_DeInit(ETH_RXD0_GPIO_PORT, ETH_RXD0_GPIO_PIN);
		HAL_GPIO_DeInit(ETH_RXD1_GPIO_PORT, ETH_RXD1_GPIO_PIN);

#ifdef ETH_NEED_REMAP
		__HAL_AFIO_REMAP_ETH_DISABLE();
#endif // ETH_NEED_REMAP
		
		// Peripheral interrupt Deinit
		HAL_NVIC_DisableIRQ(ETH_IRQn);
	}
}

#ifdef __cplusplus
extern "C" {
#endif
void ETH_IRQHandler(void)
{
	HAL_ETH_IRQHandler(&hETH);
}
#ifdef __cplusplus
}
#endif

static void ethernetif_reset_chip(void* arg)
{
	// Prevent warning for unused parameters
	(void)arg;
#ifdef ETH_NRST_GPIO_PORT
	__IO uint32_t tickstart = HAL_GetTick();
	HAL_GPIO_WritePin(ETH_NRST_GPIO_PORT, ETH_NRST_GPIO_PIN, 
					  GPIO_PIN_RESET);
	while((HAL_GetTick() - tickstart) < TIME_ETH_CHIP_RESET_ON) ;
	HAL_GPIO_WritePin(ETH_NRST_GPIO_PORT, ETH_NRST_GPIO_PIN, 
					  GPIO_PIN_SET);
#endif // ETH_NRST_GPIO_PORT
}
