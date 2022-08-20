/* SD-card driver for STM32F4x MCU family for using with FreeRTOS+FAT */

/* Includes ------------------------------------------------------------------*/
/* Standard includes. */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "portmacro.h"

/* FreeRTOS+FAT includes. */
#include "ff_sddisk.h"
#include "ff_sys.h"

/* HAL includes. */
#include "stm32f4xx_hal.h"

#ifdef HAL_SD_MODULE_ENABLED

/* Constants and macroses ----------------------------------------------------*/
/* Define a time-out for all DMA transactions in msec. */
#ifndef sdMAX_TIME_TICKS
	#define sdMAX_TIME_TICKS			pdMS_TO_TICKS(2000UL)
#endif

#ifndef sdCARD_DETECT_DEBOUNCE_TIME_MS
	/* Debouncing time is applied only after card gets inserted. */
	#define sdCARD_DETECT_DEBOUNCE_TIME_MS (5000)
#endif

/* Private constants */
/* Misc definitions. */
#define sdSIGNATURE 				0x41404342UL
#define sdHUNDRED_64_BIT			(100ull)
#define sdBYTES_PER_MB				(1024ull * 1024ull)
#define sdSECTORS_PER_MB			(sdBYTES_PER_MB / 512ull)
#define sdIOMAN_MEM_SIZE			4096

/* Macroses */
#ifndef sdARRAY_SIZE
	#define	sdARRAY_SIZE(x)	(int)(sizeof(x) / sizeof(x)[ 0 ])
#endif

/* HW constants */
#ifndef SD_SDIO_CLK_DIV
	#define SD_SDIO_CLK_DIV 			4
#endif /*SD_SDIO_CLK_DIV*/

/* Debug options -------------------------------------------------------------*/
//#define DEBUG_SD_CHECK_STATUS_TIME
//#define DEBUG_SD_WRITE_OPERATION_TIME

/* Structures definitions ----------------------------------------------------*/
typedef struct
{
	/* Only after a card has been inserted, debouncing is necessary. */
	TickType_t xRemainingTime;
	TimeOut_t xTimeOut;
	UBaseType_t
		bLastPresent : 1,
		bStableSignal : 1;
} CardDetect_t;

/* Variables -----------------------------------------------------------------*/
/* Used to unblock the task that calls prvEventWaitFunction() after an event has
occurred. */
static SemaphoreHandle_t xSDCardSemaphore = NULL;

/* Handle of the SD card being used. */
static SD_HandleTypeDef xSDHandle;

/* Holds parameters for the detected SD card. */
static HAL_SD_CardInfoTypeDef xSDCardInfo;

/* Mutex for partition. */
static SemaphoreHandle_t xPlusFATMutex = NULL;

/* Remembers if the card is currently considered to be present. */
static BaseType_t xSDCardStatus = pdFALSE;

/* Maintains state for card detection. */
static CardDetect_t xCardDetect;

#if(SDIO_USES_DMA != 0)
/* Flags for complete read and write operations through DMA */
static BaseType_t SD_ReadCplt = pdFALSE;
static BaseType_t SD_WriteCplt = pdFALSE;

	#if (configUSE_CCRAM_FOR_HEAP != 0)
static uint8_t DMABuffer[512ul];
	#endif /*(configUSE_CCRAM_FOR_HEAP != 0)*/
#endif /*(SDIO_USES_DMA != 0)*/

/* Private function prototypes -----------------------------------------------*/
static BaseType_t prvSDMMCInit(BaseType_t xDriveNumber);

/* Return pdFALSE if the SD card is not inserted.  This function just reads the
 * value of the GPIO C/D pin. */
static BaseType_t prvSDDetect(void);

/* Read and write operations */
static int32_t prvFFRead(uint8_t *pucBuffer,
		uint32_t ulSectorNumber, uint32_t ulSectorCount, FF_Disk_t *pxDisk);
static int32_t prvFFWrite(uint8_t *pucBuffer,
		uint32_t ulSectorNumber, uint32_t ulSectorCount, FF_Disk_t *pxDisk);

/* Check if the card is present, and if so, print out some info on the card. */
static BaseType_t SD_CheckStatusWithTimeout(uint32_t timeout);

#if(SDIO_USES_DMA != 0)
/* A function will be called at the start of a DMA action. */
static void prvEventSetupFunction();

/* This function is supposed to wait for an event: SDIO or DMA.
 * Return non-zero if a timeout has been reached. */
static uint32_t prvEventWaitFunction();
#endif /*(SDIO_USES_DMA != 0)*/

/* Hardware initialisation. */
static void prvSDIO_SD_Init();
static void vGPIO_SD_Init();

#if(SDIO_USES_DMA != 0)
/* Initialise the DMA for SDIO cards. */
static void prvSDIO_DMA_Init();
#endif /*(SDIO_USES_DMA != 0)*/

/* Public functions ----------------------------------------------------------*/
FF_Disk_t* FF_SDDiskInit(const char *pcName)
{
	FF_Error_t xFFError;
	BaseType_t xPartitionNumber = 0;
	FF_CreationParameters_t xParameters;
	FF_Disk_t *pxDisk;

	xSDCardStatus = prvSDMMCInit(0);

	if(xSDCardStatus != pdPASS)
	{
		FF_PRINTF("FF_SDDiskInit: prvSDMMCInit failed\n");
		pxDisk = NULL;
	}
	else
	{
		pxDisk = (FF_Disk_t *)ffconfigMALLOC(sizeof(*pxDisk));
		if(pxDisk == NULL)
		{
			FF_PRINTF("FF_SDDiskInit: Malloc failed\n");
		}
		else
		{
			/* Initialise the created disk structure. */
			memset(pxDisk, '\0', sizeof(*pxDisk));

			pxDisk->ulNumberOfSectors =
					(xSDCardInfo.BlockNbr * xSDCardInfo.BlockSize) / 512;

			if(xPlusFATMutex == NULL)
			{
				xPlusFATMutex = xSemaphoreCreateRecursiveMutex();
			}
			pxDisk->ulSignature = sdSIGNATURE;

			if(xPlusFATMutex != NULL)
			{
				memset(&xParameters, '\0', sizeof(xParameters));
				xParameters.ulMemorySize = sdIOMAN_MEM_SIZE;
				xParameters.ulSectorSize = 512;
				xParameters.fnWriteBlocks = prvFFWrite;
				xParameters.fnReadBlocks = prvFFRead;
				xParameters.pxDisk = pxDisk;

				/* prvFFRead()/prvFFWrite() are not re-entrant and must be
				protected with the use of a semaphore. */
				xParameters.xBlockDeviceIsReentrant = pdFALSE;

				/* The semaphore will be used to protect critical sections in
				the +FAT driver, and also to avoid concurrent calls to
				prvFFRead()/prvFFWrite() from different tasks. */
				xParameters.pvSemaphore = (void *) xPlusFATMutex;

				pxDisk->pxIOManager =
						FF_CreateIOManger(&xParameters, &xFFError);

				if(pxDisk->pxIOManager == NULL)
				{
					FF_PRINTF("FF_SDDiskInit: FF_CreateIOManger: %s\n",
							(const char*)FF_GetErrMessage(xFFError));
					FF_SDDiskDelete(pxDisk);
					pxDisk = NULL;
				}
				else
				{
					pxDisk->xStatus.bIsInitialised = pdTRUE;
					pxDisk->xStatus.bPartitionNumber = xPartitionNumber;
					if(FF_SDDiskMount(pxDisk) == 0)
					{
						FF_SDDiskDelete(pxDisk);
						pxDisk = NULL;
					}
					else
					{
						if(pcName == NULL)
						{
							pcName = "/";
						}
						FF_FS_Add(pcName, pxDisk);
						FF_PRINTF("FF_SDDiskInit: Mounted SD-card as root \
\"%s\"\n", pcName);
						FF_SDDiskShowPartition(pxDisk);
					}
				}	/* if(pxDisk->pxIOManager != NULL) */
			}	/* if(xPlusFATMutex != NULL) */
		}	/* if(pxDisk != NULL) */
	}	/* if(xSDCardStatus == pdPASS) */

	return pxDisk;
}

BaseType_t FF_SDDiskReinit(FF_Disk_t *pxDisk)
{
	/*_RB_ parameter not used. */
	(void) pxDisk;

	BaseType_t xStatus = prvSDMMCInit(0); /* Hard coded index. */
	FF_PRINTF("FF_SDDiskReinit: rc %08x\n", (unsigned) xStatus);
	return xStatus;
}

BaseType_t FF_SDDiskDetect(FF_Disk_t *pxDisk)
{
	(void)pxDisk;
	uint32_t xReturn;

	xReturn = prvSDDetect();

	if(xReturn != pdFALSE)
	{
		if(xCardDetect.bStableSignal == pdFALSE)
		{
			/* The card seems to be present. */
			if(xCardDetect.bLastPresent == pdFALSE)
			{
				xCardDetect.bLastPresent = pdTRUE;
				xCardDetect.xRemainingTime = pdMS_TO_TICKS(
						(TickType_t)sdCARD_DETECT_DEBOUNCE_TIME_MS);
				/* Fetch the current time. */
				vTaskSetTimeOutState(&xCardDetect.xTimeOut);
			}
			/* Has the timeout been reached? */
			if(xTaskCheckForTimeOut(&xCardDetect.xTimeOut,
					&xCardDetect.xRemainingTime) != pdFALSE)
			{
				xCardDetect.bStableSignal = pdTRUE;
			}
			else
			{
				/* keep returning false until de time-out is reached. */
				xReturn = pdFALSE;
			}
		}
	}
	else
	{
		xCardDetect.bLastPresent = pdFALSE;
		xCardDetect.bStableSignal = pdFALSE;
	}

	return xReturn;
}

BaseType_t FF_SDDiskMount(FF_Disk_t *pxDisk)
{
	FF_Error_t xFFError;
	BaseType_t xReturn;

	/* Mount the partition */
	xFFError = FF_Mount(pxDisk, pxDisk->xStatus.bPartitionNumber);

	if(FF_isERR(xFFError))
	{
		FF_PRINTF("FF_SDDiskMount: %08lX\n", xFFError);
		xReturn = pdFAIL;
	}
	else
	{
		pxDisk->xStatus.bIsMounted = pdTRUE;
		FF_PRINTF("****** FreeRTOS+FAT initialized %lu sectors\n",
				pxDisk->pxIOManager->xPartition.ulTotalSectors);
		xReturn = pdPASS;
	}

	return xReturn;
}

BaseType_t FF_SDDiskUnmount(FF_Disk_t *pxDisk)
{
	FF_Error_t xFFError;
	BaseType_t xReturn = pdPASS;

	if((pxDisk != NULL) && (pxDisk->xStatus.bIsMounted != pdFALSE))
	{
		pxDisk->xStatus.bIsMounted = pdFALSE;
		xFFError = FF_Unmount(pxDisk);

		if(FF_isERR(xFFError))
		{
			FF_PRINTF("FF_SDDiskUnmount: rc %08x\n", (unsigned)xFFError);
			xReturn = pdFAIL;
		}
		else FF_PRINTF("Drive unmounted\n");
	}

	return xReturn;
}

void FF_SDDiskFlush(FF_Disk_t *pxDisk)
{
	if(	(pxDisk != NULL) &&
		(pxDisk->xStatus.bIsInitialised != pdFALSE) &&
		(pxDisk->pxIOManager != NULL))
	{
		FF_FlushCache(pxDisk->pxIOManager);
	}
}

/* Release all resources */
BaseType_t FF_SDDiskDelete(FF_Disk_t *pxDisk)
{
	if(pxDisk != NULL)
	{
		pxDisk->ulSignature = 0;
		pxDisk->xStatus.bIsInitialised = 0;
		if(pxDisk->pxIOManager != NULL)
		{
			if(FF_Mounted(pxDisk->pxIOManager) != pdFALSE) FF_Unmount(pxDisk);
			FF_DeleteIOManager(pxDisk->pxIOManager);
		}

		vPortFree(pxDisk);
	}
	return 1;
}

BaseType_t FF_SDDiskFormat(FF_Disk_t *pxDisk, BaseType_t xPartitionNumber)
{
	FF_Error_t xError;
	BaseType_t xReturn = pdFAIL;

	xError = FF_Unmount(pxDisk);

	if(FF_isERR(xError) != pdFALSE)
	{
		FF_PRINTF("FF_SDDiskFormat: unmount fails: %08x\n", (unsigned) xError);
	}
	else
	{
		/* Format the drive - try FAT32 with large clusters. */
		xError = FF_Format(pxDisk, xPartitionNumber, pdFALSE, pdFALSE);

		if(FF_isERR(xError))
		{
			FF_PRINTF("FF_SDDiskFormat: %s\n",
					(const char*)FF_GetErrMessage(xError));
		}
		else
		{
			FF_PRINTF("FF_SDDiskFormat: OK, now remounting\n");
			pxDisk->xStatus.bPartitionNumber = xPartitionNumber;
			xError = FF_SDDiskMount(pxDisk);
			FF_PRINTF("FF_SDDiskFormat: rc %08x\n", (unsigned)xError);
			if(FF_isERR(xError) == pdFALSE)
			{
				xReturn = pdPASS;
				FF_SDDiskShowPartition(pxDisk);
			}
		}
	}

	return xReturn;
}

BaseType_t FF_SDDiskShowPartition(FF_Disk_t *pxDisk)
{
	FF_Error_t xError;
	uint64_t ullFreeSectors;
	uint32_t ulTotalSizeMB, ulFreeSizeMB;
	int iPercentageFree;
	FF_IOManager_t *pxIOManager;
	const char *pcTypeName = "unknown type";
	BaseType_t xReturn = pdPASS;

	if(pxDisk == NULL) xReturn = pdFAIL;
	else
	{
		pxIOManager = pxDisk->pxIOManager;

		FF_PRINTF("Reading FAT and calculating Free Space\n");

		switch(pxIOManager->xPartition.ucType)
		{
			case FF_T_FAT12:
				pcTypeName = "FAT12";
				break;

			case FF_T_FAT16:
				pcTypeName = "FAT16";
				break;

			case FF_T_FAT32:
				pcTypeName = "FAT32";
				break;

			default:
				pcTypeName = "UNKOWN";
				break;
		}

		FF_GetFreeSize(pxIOManager, &xError);

		ullFreeSectors = pxIOManager->xPartition.ulFreeClusterCount *
				pxIOManager->xPartition.ulSectorsPerCluster;
		iPercentageFree = (int) ((sdHUNDRED_64_BIT * ullFreeSectors +
				pxIOManager->xPartition.ulDataSectors / 2) /
				((uint64_t)pxIOManager->xPartition.ulDataSectors));

		ulTotalSizeMB =
				pxIOManager->xPartition.ulDataSectors / sdSECTORS_PER_MB;
		ulFreeSizeMB = (uint32_t) (ullFreeSectors / sdSECTORS_PER_MB);

		/* It is better not to use the 64-bit format such as %Lu because it
		might not be implemented. */
		FF_PRINTF("Partition Nr   %8u\n",
				pxDisk->xStatus.bPartitionNumber);
		FF_PRINTF("Type           %8u (%s)\n",
				pxIOManager->xPartition.ucType, pcTypeName);
		FF_PRINTF("VolLabel       '%8s' \n",
				pxIOManager->xPartition.pcVolumeLabel);
		FF_PRINTF("TotalSectors   %8lu\n",
				pxIOManager->xPartition.ulTotalSectors);
		FF_PRINTF("SecsPerCluster %8lu\n",
				pxIOManager->xPartition.ulSectorsPerCluster);
		FF_PRINTF("Size           %8lu MB\n", ulTotalSizeMB);
		FF_PRINTF("FreeSize       %8lu MB (%d perc free)\n",
				ulFreeSizeMB, iPercentageFree);
	}

	return xReturn;
}

FF_IOManager_t* sddisk_ioman(FF_Disk_t *pxDisk)
{
	if((pxDisk != NULL) && (pxDisk->xStatus.bIsInitialised != pdFALSE))
		return pxDisk->pxIOManager;
	return NULL;
}

/* Private functions ---------------------------------------------------------*/
static BaseType_t prvSDMMCInit(BaseType_t xDriveNumber)
{
	/* 'xDriveNumber' not yet in use. */
	(void)xDriveNumber;

	if(xSDCardSemaphore == NULL) xSDCardSemaphore = xSemaphoreCreateBinary();

	/* Hardware initialisation */
	prvSDIO_SD_Init();
	vGPIO_SD_Init();

#if(SDIO_USES_DMA != 0)
	prvSDIO_DMA_Init();
#endif

	uint32_t SD_state = HAL_SD_ERROR_NONE;
	/* Check if the SD card is plugged in the slot */
	if(prvSDDetect() == pdFALSE)
	{
		FF_PRINTF("No SD card detected\n");
		return 0;
	}
	/* When starting up, skip debouncing of the Card Detect signal. */
	xCardDetect.bLastPresent = pdTRUE;
	xCardDetect.bStableSignal = pdTRUE;
	/* Initialise the SDIO device and read the card parameters. */
	if(HAL_SD_Init(&xSDHandle) != HAL_OK)
	{
		SD_state = HAL_SD_ERROR_GENERAL_UNKNOWN_ERR;
	}
	else
	{
		/* Get SD card Information */
		HAL_SD_GetCardInfo(&xSDHandle, &xSDCardInfo);

#if(BUS_4BITS != 0)
		if(SD_state == SD_OK)
		{
			uint32_t rc;

			xSDHandle.Init.BusWide = SDIO_BUS_WIDE_4B;
			rc = HAL_SD_WideBusOperation_Config(&xSDHandle, SDIO_BUS_WIDE_4B);
		}
#endif
	}
	return SD_state == HAL_SD_ERROR_NONE ? 1 : 0;
}

/* Raw SD-card detection, just return the GPIO status. */
static BaseType_t prvSDDetect(void)
{
	/*!< Check GPIO to detect SD */
	if(HAL_GPIO_ReadPin(SD_PRESENT_GPIO_PORT, SD_PRESENT_PIN) != 0)
	{
		/* The internal pull-up makes the signal high. */
		return pdFALSE;
	}

	/* The card will pull the GPIO signal down. */
	return pdTRUE;
}

/* Read and write operations */
static int32_t prvFFRead(uint8_t *pucBuffer,
		uint32_t ulSectorNumber, uint32_t ulSectorCount, FF_Disk_t *pxDisk)
{
	int32_t lReturnCode = FF_ERR_IOMAN_OUT_OF_BOUNDS_READ | FF_ERRFLAG;

	if(	(pxDisk == NULL) ||
		(xSDCardStatus != pdPASS) ||
		(pxDisk->ulSignature != sdSIGNATURE) ||
		(pxDisk->xStatus.bIsInitialised == pdFALSE) ||
		(ulSectorNumber >= pxDisk->ulNumberOfSectors) ||
		((pxDisk->ulNumberOfSectors - ulSectorNumber) < ulSectorCount))
	{
		/* Make sure no random data is in the returned buffer. */
		memset((void *) pucBuffer, '\0', ulSectorCount * 512UL);

		if(pxDisk->xStatus.bIsInitialised != pdFALSE)
		{
			FF_PRINTF("prvFFRead: warning: %lu + %lu > %lu\n",
					ulSectorNumber, ulSectorCount, pxDisk->ulNumberOfSectors);
		}
		return lReturnCode;
	}

	/* Ensure the SDCard is ready for a new operation */
	if(SD_CheckStatusWithTimeout(sdMAX_TIME_TICKS) < 0) return lReturnCode;

	HAL_StatusTypeDef result = HAL_ERROR;

#if(SDIO_USES_DMA == 0)
	result = HAL_SD_ReadBlocks(&xSDHandle, pucBuffer,
			ulSectorNumber, ulSectorCount, sdMAX_TIME_TICKS)
	if(result == HAL_OK) lReturnCode = 0;
#else /*(SDIO_USES_DMA != 0)*/

	#if !defined(configUSE_CCRAM_FOR_HEAP) || (configUSE_CCRAM_FOR_HEAP == 0)
	if((((size_t)pucBuffer) & (sizeof(size_t) - 1)) == 0)
	{
		/* The buffer is word-aligned, call DMA read directly. */
		/* Reset the read complete flag before DMA operation */
		SD_ReadCplt = pdFALSE;
		result = HAL_SD_ReadBlocks_DMA(&xSDHandle, pucBuffer,
				ulSectorNumber, ulSectorCount);
		if(result == HAL_OK)
		{
			/* Reset the timers for a DMA transfer */
			prvEventSetupFunction();

			/* Wait for the read complete flag or a timeout */
			while(1)
			{
				if(SD_ReadCplt)
				{
					lReturnCode = 0;
					break;
				}
				if(prvEventWaitFunction(&xSDHandle))
				{
					/* A timeout is reached. */
					result = HAL_TIMEOUT;
					break;
				}
			}
		}
	}
	else
	#endif /*!defined(configUSE_CCRAM_FOR_HEAP) || (configUSE_CCRAM_FOR_HEAP == 0)*/
	{
		/* The buffer is NOT word-aligned, copy first to an aligned buffer. */
	#if (configUSE_CCRAM_FOR_HEAP != 0)
		uint8_t* pucDMABuffer = DMABuffer;
	#else /*(configUSE_CCRAM_FOR_HEAP != 0)*/
		uint8_t* pucDMABuffer = ffconfigMALLOC(512ul);
		if(pucDMABuffer != NULL)
	#endif /*(configUSE_CCRAM_FOR_HEAP != 0)*/
		{
			uint32_t ulSector;
			for(ulSector = 0; ulSector < ulSectorCount; ulSector++)
			{
				/* Reset the read complete flag before DMA operation */
				SD_ReadCplt = pdFALSE;
				result = HAL_SD_ReadBlocks_DMA(&xSDHandle, pucDMABuffer,
						ulSectorNumber +  ulSector, 1);
				if(result == HAL_OK)
				{
					/* Reset the timers for a DMA transfer */
					prvEventSetupFunction();

					/* Wait for the read complete flag or a timeout */
					while(1)
					{
						if(SD_ReadCplt) break;
						if(prvEventWaitFunction(&xSDHandle))
						{
							/* A timeout is reached. */
							result = HAL_TIMEOUT;
							break;
						}
					}
					if(result == HAL_TIMEOUT)
					{
						/* A timeout is reached, no need to read further */
						break;
					}
					memcpy(pucBuffer + 512ul * ulSector, pucDMABuffer, 512ul);
				}
				else break;
			}

	#if !defined(configUSE_CCRAM_FOR_HEAP) || (configUSE_CCRAM_FOR_HEAP == 0)
			ffconfigFREE(pucDMABuffer);
	#endif /*!defined(configUSE_CCRAM_FOR_HEAP) || \
			(configUSE_CCRAM_FOR_HEAP == 0)*/

			if((ulSector == ulSectorCount) && (result == HAL_OK))
				lReturnCode = 0;
		}
	}
#endif /*SDIO_USES_DMA*/

	return lReturnCode;
}

static int32_t prvFFWrite(uint8_t *pucBuffer,
		uint32_t ulSectorNumber, uint32_t ulSectorCount, FF_Disk_t *pxDisk)
{
	int32_t lReturnCode = FF_ERR_IOMAN_OUT_OF_BOUNDS_READ | FF_ERRFLAG;

	if(	(pxDisk == NULL) ||
		(xSDCardStatus != pdPASS) ||
		(pxDisk->ulSignature != sdSIGNATURE) ||
		(pxDisk->xStatus.bIsInitialised == pdFALSE) ||
		(ulSectorNumber >= pxDisk->ulNumberOfSectors) ||
		((pxDisk->ulNumberOfSectors - ulSectorNumber) < ulSectorCount))
	{
		if(pxDisk->xStatus.bIsInitialised != pdFALSE)
		{
			FF_PRINTF("prvFFWrite: warning: %lu + %lu > %lu\n",
					ulSectorNumber, ulSectorCount, pxDisk->ulNumberOfSectors);
		}
		return lReturnCode;
	}

	/* Ensure the SDCard is ready for a new operation */
	if(SD_CheckStatusWithTimeout(sdMAX_TIME_TICKS) < 0) return lReturnCode;

	HAL_StatusTypeDef result = HAL_ERROR;

#if(SDIO_USES_DMA == 0)
	sd_result = HAL_SD_WriteBlocks(&xSDHandle, pucBuffer,
			ulSectorNumber, ulSectorCount, sdMAX_TIME_TICKS);
	if(result == HAL_OK) lReturnCode = 0;
#else /*(SDIO_USES_DMA != 0)*/

	#if (configUSE_CCRAM_FOR_HEAP != 0)
	uint8_t* pucDMABuffer = DMABuffer;
	#else /*(configUSE_CCRAM_FOR_HEAP != 0)*/
	BaseType_t aligned = pdFALSE;
	uint8_t* pucDMABuffer;
	#endif /*(configUSE_CCRAM_FOR_HEAP != 0)*/

	#if !defined(configUSE_CCRAM_FOR_HEAP) || (configUSE_CCRAM_FOR_HEAP == 0)
	if((((size_t)pucBuffer) & (sizeof(size_t) - 1)) == 0)
	{
		/* The buffer is word-aligned, call DMA write directly. */
		aligned = pdTRUE;
	}
	else
	{
		/* The buffer is NOT word-aligned, read to an aligned buffer and then
		copy the data to the user provided buffer. */
		pucDMABuffer = ffconfigMALLOC(512ul);
		if(pucDMABuffer == NULL)
		{
			/* Could not allocate the buffer */
			return lReturnCode;
		}
	}
	#endif /*!defined(configUSE_CCRAM_FOR_HEAP) || \
			(configUSE_CCRAM_FOR_HEAP == 0)*/

	uint32_t ulSector;
	for(ulSector = 0; ulSector < ulSectorCount; ulSector++)
	{
	#if !defined(configUSE_CCRAM_FOR_HEAP) || (configUSE_CCRAM_FOR_HEAP == 0)
		if(aligned == pdFALSE)
	#endif /*!defined(configUSE_CCRAM_FOR_HEAP) || (configUSE_CCRAM_FOR_HEAP == 0)*/
		{
			memcpy(pucDMABuffer, pucBuffer + 512ul * ulSector, 512ul);
		}

		/* Reset the write complete flag before DMA operation */
		SD_WriteCplt = pdFALSE;

#ifdef DEBUG_SD_WRITE_OPERATION_TIME
		HAL_GPIO_WritePin(
				TEST_0_GPIO_PORT, TEST_0_GPIO_PIN, GPIO_PIN_SET);
#endif /*DEBUG_SD_WRITE_OPERATION_TIME*/

	#if !defined(configUSE_CCRAM_FOR_HEAP) || (configUSE_CCRAM_FOR_HEAP == 0)
		if(aligned == pdFALSE)
	#endif /*!defined(configUSE_CCRAM_FOR_HEAP) || (configUSE_CCRAM_FOR_HEAP == 0)*/
		{
			/* Write data from aligned buffer */
			result = HAL_SD_WriteBlocks_DMA(&xSDHandle, pucDMABuffer,
					ulSectorNumber + ulSector, 1);
		}
	#if !defined(configUSE_CCRAM_FOR_HEAP) || (configUSE_CCRAM_FOR_HEAP == 0)
		else
		{
			/* Call DMA write directly */
			result = HAL_SD_WriteBlocks_DMA(&xSDHandle,
					pucBuffer + 512ul * ulSector,
					ulSectorNumber + ulSector, 1);
		}
	#endif /*!defined(configUSE_CCRAM_FOR_HEAP) || \
	(configUSE_CCRAM_FOR_HEAP == 0)*/

#ifdef DEBUG_SD_WRITE_OPERATION_TIME
		HAL_GPIO_WritePin(
				TEST_0_GPIO_PORT, TEST_0_GPIO_PIN, GPIO_PIN_RESET);
#endif /*DEBUG_SD_WRITE_OPERATION_TIME*/

		if(result == HAL_OK)
		{
			/* Reset the timers for a DMA transfer */
			prvEventSetupFunction();

			/* Wait for the write complete flag or a timeout */
			while(1)
			{
				if(SD_WriteCplt)
				{
					if(SD_CheckStatusWithTimeout(sdMAX_TIME_TICKS) < 0)
						result = HAL_TIMEOUT;
					break;
				}
				if(prvEventWaitFunction(&xSDHandle))
				{
					/* A timeout is reached. */
					result = HAL_TIMEOUT;
					break;
				}
			}
			if(result == HAL_TIMEOUT)
			{
				/* A timeout is reached, no need to write further */
				break;
			}
		}
		else break;
	}

	#if !defined(configUSE_CCRAM_FOR_HEAP) || (configUSE_CCRAM_FOR_HEAP == 0)
	if(aligned == pdFALSE) ffconfigFREE(pucDMABuffer);
	#endif /*!defined(configUSE_CCRAM_FOR_HEAP) || \
	(configUSE_CCRAM_FOR_HEAP == 0)*/

	if((ulSector == ulSectorCount) && (result == HAL_OK))
	{
		/* Ensure the SDCard complete the write operation */
		lReturnCode = 0;
	}
#endif	/*SDIO_USES_DMA*/

	return lReturnCode;
}

/* Gets the current SD card data status.
  Return data transfer state:
  0: No data transfer is acting
  -1: Data transfer is still acting and timeout is reached */
static BaseType_t SD_CheckStatusWithTimeout(uint32_t timeout)
{
#ifdef DEBUG_SD_CHECK_STATUS_TIME
	HAL_GPIO_WritePin(TEST_1_GPIO_PORT, TEST_1_GPIO_PIN, GPIO_PIN_SET);
#endif /*DEBUG_SD_CHECK_STATUS_TIME*/

	TickType_t startTime = xTaskGetTickCount();
	/* block until SDIO peripherial is ready again or a timeout occur */
	while(xTaskGetTickCount() - startTime < timeout)
	{
		/* Check for finished transmit state */
		if(HAL_SD_GetCardState(&xSDHandle) == HAL_SD_CARD_TRANSFER)
		{
			/* Transfer complete */
#ifdef DEBUG_SD_CHECK_STATUS_TIME
			HAL_GPIO_WritePin(TEST_1_GPIO_PORT, TEST_1_GPIO_PIN, GPIO_PIN_RESET);
#endif /*DEBUG_SD_CHECK_STATUS_TIME*/

			return 0;
		}
	}

#ifdef DEBUG_SD_CHECK_STATUS_TIME
	HAL_GPIO_WritePin(TEST_1_GPIO_PORT, TEST_1_GPIO_PIN, GPIO_PIN_RESET);
#endif /*DEBUG_SD_CHECK_STATUS_TIME*/

	return -1;
}

/* Used to handle timeouts. */
static TickType_t xDMARemainingTime;
static TimeOut_t xDMATimeOut;

#if(SDIO_USES_DMA != 0)
static void prvEventSetupFunction()
{
	/* A DMA transfer to or from the SD-card is about to start.
	Reset the timers that will be used in prvEventWaitFunction() */
	xDMARemainingTime = sdMAX_TIME_TICKS;
	vTaskSetTimeOutState(&xDMATimeOut);
}

static uint32_t prvEventWaitFunction()
{
	/* It was measured how quickly a DMA interrupt was received	*/
	if(xTaskCheckForTimeOut(&xDMATimeOut, &xDMARemainingTime) != pdFALSE)
	{
		/* The timeout has been reached, no need to block. */
		return 1;
	}

	/* The timeout has not been reached yet, block on the semaphore. */
	xSemaphoreTake(xSDCardSemaphore, xDMARemainingTime);
	if(xTaskCheckForTimeOut(&xDMATimeOut, &xDMARemainingTime) != pdFALSE)
		return 1;
	return 0;
}
#endif /*(SDIO_USES_DMA != 0)*/

/* Hardware initialisation. */
/* SDIO init function */
static void prvSDIO_SD_Init()
{
	xSDHandle.Instance = SD_SDIO;
	xSDHandle.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
	xSDHandle.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
	xSDHandle.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;

	/* Start as a 1-bit bus and switch to 4-bits later on. */
	xSDHandle.Init.BusWide = SDIO_BUS_WIDE_1B;

	xSDHandle.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;

	/* Use fastest CLOCK at 0. */
	xSDHandle.Init.ClockDiv = SD_SDIO_CLK_DIV;

	HAL_SD_SDIO_CLK_ENABLE();
}

static void vGPIO_SD_Init()
{
	GPIO_InitTypeDef GPIO_InitStruct;

	/* Peripheral clock enable */
	HAL_SD_SDIO_CLK_ENABLE();

	/* SDIO GPIO Configuration
	PC8     ------> SDIO_D0
	PC9     ------> SDIO_D1
	PC10    ------> SDIO_D2
	PC11    ------> SDIO_D3
	PC12    ------> SDIO_CK
	PD2     ------> SDIO_CMD
	*/

	/* SDIO data bus GPIO configuration */
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = SD_SDIO_Dx_AF;

	SD_SDIO_D0_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = SD_SDIO_D0_GPIO_PIN;
	HAL_GPIO_Init(SD_SDIO_D0_GPIO_PORT, &GPIO_InitStruct);

#if(BUS_4BITS != 0)
	SD_SDIO_D1_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = SD_SDIO_D1_GPIO_PIN;
	HAL_GPIO_Init(SD_SDIO_D1_GPIO_PORT, &GPIO_InitStruct);

	SD_SDIO_D2_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = SD_SDIO_D2_GPIO_PIN;
	HAL_GPIO_Init(SD_SDIO_D2_GPIO_PORT, &GPIO_InitStruct);

	SD_SDIO_D3_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = SD_SDIO_D3_GPIO_PIN;
	HAL_GPIO_Init(SD_SDIO_D3_GPIO_PORT, &GPIO_InitStruct);
#endif

	/* SDIO CK and CMD GPIO configuration */
	SD_SDIO_CK_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = SD_SDIO_CK_GPIO_PIN;
	HAL_GPIO_Init(SD_SDIO_CK_GPIO_PORT, &GPIO_InitStruct);

	SD_SDIO_CMD_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = SD_SDIO_CMD_GPIO_PIN;
	HAL_GPIO_Init(SD_SDIO_CMD_GPIO_PORT, &GPIO_InitStruct);

	/* SD-card present GPIO configuration */
	SD_PRESENT_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = SD_PRESENT_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(SD_PRESENT_GPIO_PORT, &GPIO_InitStruct);

	/* Enable and set EXTI Line Interrupt */
	HAL_NVIC_SetPriority(SD_PRESENT_GPIO_EXTI_IRQn,
			SD_PRESENT_GPIO_EXTI_I_PRIOR, 0);
	HAL_NVIC_EnableIRQ(SD_PRESENT_GPIO_EXTI_IRQn);
}

#if(SDIO_USES_DMA != 0)
static void prvSDIO_DMA_Init()
{
	static DMA_HandleTypeDef xRxDMAHandle;
	static DMA_HandleTypeDef xTxDMAHandle;

	/* Enable DMA clocks */
	SD_SDIO_DMA_CLK_ENABLE();

	/* NVIC configuration for SDIO interrupts */
	HAL_NVIC_SetPriority(SD_SDIO_DMA_IRQn, SD_SDIO_DMA_I_PRIOR, 0);
	HAL_NVIC_EnableIRQ(SD_SDIO_DMA_IRQn);

	/* Configure DMA Rx parameters */
	xRxDMAHandle.Init.Channel             = SD_SDIO_DMA_Rx_CHANNEL;
	xRxDMAHandle.Init.Direction           = DMA_PERIPH_TO_MEMORY;
	/* Peripheral address is fixed (FIFO). */
	xRxDMAHandle.Init.PeriphInc           = DMA_PINC_DISABLE;
	/* Memory address increases. */
	xRxDMAHandle.Init.MemInc              = DMA_MINC_ENABLE;
	xRxDMAHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
	xRxDMAHandle.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
	/* The peripheral has flow-control. */
	xRxDMAHandle.Init.Mode                = DMA_PFCTRL;
	xRxDMAHandle.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
	xRxDMAHandle.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
	xRxDMAHandle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
	xRxDMAHandle.Init.MemBurst            = DMA_MBURST_INC4;
	xRxDMAHandle.Init.PeriphBurst         = DMA_PBURST_INC4;

	/* DMA2_Stream3. */
	xRxDMAHandle.Instance = SD_SDIO_DMA_Rx_STREAM;

	/* Associate the DMA handle */
	__HAL_LINKDMA(&xSDHandle, hdmarx, xRxDMAHandle);

	/* Deinitialize the stream for new transfer */
	HAL_DMA_DeInit(&xRxDMAHandle);

	/* Configure the DMA stream */
	HAL_DMA_Init(&xRxDMAHandle);

	/* Configure DMA Tx parameters */
	xTxDMAHandle.Init.Channel             = SD_SDIO_DMA_Tx_CHANNEL;
	xTxDMAHandle.Init.Direction           = DMA_MEMORY_TO_PERIPH;
	xTxDMAHandle.Init.PeriphInc           = DMA_PINC_DISABLE;
	xTxDMAHandle.Init.MemInc              = DMA_MINC_ENABLE;
	xTxDMAHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
	xTxDMAHandle.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
	xTxDMAHandle.Init.Mode                = DMA_PFCTRL;
	xTxDMAHandle.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
	xTxDMAHandle.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
	xTxDMAHandle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
	xTxDMAHandle.Init.MemBurst            = DMA_MBURST_SINGLE;
	xTxDMAHandle.Init.PeriphBurst         = DMA_PBURST_INC4;

	/* DMA2_Stream6. */
	xTxDMAHandle.Instance = SD_SDIO_DMA_Tx_STREAM;

	/* Associate the DMA handle */
	__HAL_LINKDMA(&xSDHandle, hdmatx, xTxDMAHandle);

	/* Deinitialize the stream for new transfer */
	HAL_DMA_DeInit(&xTxDMAHandle);

	/* Configure the DMA stream */
	HAL_DMA_Init(&xTxDMAHandle);

	/* NVIC configuration for DMA transfer complete interrupt */
	HAL_NVIC_SetPriority(SD_SDIO_DMA_Rx_IRQn, SD_SDIO_DMA_I_PRIOR, 0);
	HAL_NVIC_EnableIRQ(SD_SDIO_DMA_Rx_IRQn);

	/* NVIC configuration for DMA transfer complete interrupt */
	HAL_NVIC_SetPriority(SD_SDIO_DMA_Tx_IRQn, SD_SDIO_DMA_I_PRIOR, 0);
	HAL_NVIC_EnableIRQ(SD_SDIO_DMA_Tx_IRQn);
}

/* Interrupts handler and callback functions */
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
	(void)hsd;
	SD_WriteCplt = pdTRUE;

	BaseType_t xHigherPriorityTaskWoken = 0;
	if(xSDCardSemaphore != NULL)
	{
		xSemaphoreGiveFromISR(xSDCardSemaphore, &xHigherPriorityTaskWoken);
	}
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
  * @brief Rx Transfer completed callback
  * @param hsd: SD handle
  * @retval None
  */
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
	(void)hsd;
	SD_ReadCplt = pdTRUE;

	BaseType_t xHigherPriorityTaskWoken = 0;
	if(xSDCardSemaphore != NULL)
	{
		xSemaphoreGiveFromISR(xSDCardSemaphore, &xHigherPriorityTaskWoken);
	}
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void SD_SDIO_DMA_IRQHandler(void)
{
	HAL_SD_IRQHandler(&xSDHandle);

	BaseType_t xHigherPriorityTaskWoken = 0;
	if(xSDCardSemaphore != NULL)
	{
		xSemaphoreGiveFromISR(xSDCardSemaphore, &xHigherPriorityTaskWoken);
	}
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void SD_SDIO_DMA_Tx_IRQHandler(void)
{
	/* DMA SDIO-TX interrupt handler. */
	HAL_DMA_IRQHandler (xSDHandle.hdmatx);

	BaseType_t xHigherPriorityTaskWoken = 0;
	if(xSDCardSemaphore != NULL)
	{
		xSemaphoreGiveFromISR(xSDCardSemaphore, &xHigherPriorityTaskWoken);
	}
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void SD_SDIO_DMA_Rx_IRQHandler(void)
{
	/* DMA SDIO-RX interrupt handler. */
	HAL_DMA_IRQHandler (xSDHandle.hdmarx);

	BaseType_t xHigherPriorityTaskWoken = 0;
	if(xSDCardSemaphore != NULL)
	{
		xSemaphoreGiveFromISR(xSDCardSemaphore, &xHigherPriorityTaskWoken);
	}
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#endif /*SDIO_USES_DMA != 0*/

void SD_PRESENT_GPIO_EXTI_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(SD_PRESENT_PIN);

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	/* The following 'hook' must be provided by the user of this module.  It
	 * will be called from a GPIO ISR after every change.  Note that during the
	 * ISR, the value of the GPIO is not stable and it can not be used.
	 * All you can do from this hook is wake-up some task, which will call
	 * FF_SDDiskDetect(). */
	extern void vApplicationCardDetectChangeHookFromISR(
			BaseType_t *pxHigherPriorityTaskWoken);
	vApplicationCardDetectChangeHookFromISR(&xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

#endif /*HAL_SD_MODULE_ENABLED*/
