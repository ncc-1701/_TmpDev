/* Include functions for transmitting synchro protocol
*/

/* Includes ----------------------------------------------------------------- */
/* Standard includes */
#include <stdint.h>
#include <stdbool.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Drivers includes */
#include "TRS_sync_proto_driver.h"

/* Application includes */
#include "settings.h"
#include "rtc.h"
#include "TRS_sync_proto.h"

/* Constants ---------------------------------------------------------------- */
/* Private constants */
/* FreeRTOS constants */
#ifndef TRS_SNC_PRT_APP_TASK_PRIORITY
#	define TRS_SNC_PRT_APP_TASK_PRIORITY (tskIDLE_PRIORITY + 3)
#endif /*TRS_SNC_PRT_APP_TASK_PRIORITY*/
#define TRS_SNC_PRT_APP_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE)

/* Structure of sync data package */
enum TRS_SNC_PRT_DATA_STRUCT
{
	TRS_SNC_PRT_SECOND = 0,
	TRS_SNC_PRT_MINUTE,
	TRS_SNC_PRT_HOUR,
	TRS_SNC_PRT_DAY,
	TRS_SNC_PRT_MONTH,
	TRS_SNC_PRT_YEAR,
};

/* Marker of data-package begin */
#define TRS_SNC_PRT_MARKER 			0x55

/* Max data package duration, ms */
#define TRS_SNC_PRT_MAX_DATA_PCKG_DUR 300

/* Other critical data-package fields position */
enum
{
	TRS_SNC_PRT_MARKER_POS = 0,
	TRS_SNC_PRT_DATA_LENG_POS,
	TRS_SNC_PRT_HEADER_SIZE
};

#define MAX_DATA_LENG			25
#define CRC_SIZE				1
#define DATA_PACKAGE_SIZE_MAX	TRS_SNC_PRT_HEADER_SIZE + \
									MAX_DATA_LENG + CRC_SIZE

static const uint8_t CRC_Table[] = 
{
	  0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
	157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
	 35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
	190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
	 70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
	219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
	101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
	248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
	140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
	 17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
	175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
	 50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
	202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
	 87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
	233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
	116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53,
};

/* Other constants */
#define TRS_SYNC_PROTO_FIRSTYEAR 	2000

/* Variables ---------------------------------------------------------------- */
/* Handle of the application task */
static TaskHandle_t xAppTaskHandle = NULL;
static SemaphoreHandle_t xNewTimeEventWakeupSem = NULL;

/* For transmitting answer's bytes */
static uint8_t transBuff[DATA_PACKAGE_SIZE_MAX];

/* Private function prototypes ---------------------------------------------- */
static void TRS_SyncProtoTask();
static void TRS_SyncProtoPerSecondTask();
static void TRS_SyncProtoSendData();

static void GetHeader(uint8_t* pointer);
static void SendCmd(uint8_t* pointer);
static uint8_t CheckCRC(uint8_t* buff, uint8_t size);

/* Public functions --------------------------------------------------------- */
void TRS_SyncProtoInit()
{
	/* Init driver */
	TRS_SyncProtoDriverInit();

	/* Init variables */
	/* Set default settings */
	TRS_SyncProtoSetDefaults();

	/* Register per-second task */
	if(RTC_AddPerSecondTask(TRS_SyncProtoPerSecondTask) == false)
	{
		FreeRTOS_printf(("Could not register TRS_SyncProto per-second task\n"));
		return;
	}

	/* Create application task */
	if(xTaskCreate(TRS_SyncProtoTask, "TRS_SyncProto",
			TRS_SNC_PRT_APP_TASK_STACK_SIZE, NULL,
			TRS_SNC_PRT_APP_TASK_PRIORITY, &xAppTaskHandle) !=  pdPASS)
	{
		FreeRTOS_printf(("Could not create TRS_SyncProto task\n"));
		return;
	}

	/* Create binary semaphore */
	xNewTimeEventWakeupSem = xSemaphoreCreateBinary();
	if(xNewTimeEventWakeupSem == NULL)
	{
		FreeRTOS_printf("Could not create TRS_SyncProto binary semaphore\n");
		return;
	}
}

void TRS_SyncProtoSetDefaults()
{
	/* Set default settings */

}

/* Private functions ---------------------------------------------------------*/
static void TRS_SyncProtoTask()
{
	if(xNewTimeEventWakeupSem == NULL)
	{
		/* Semaphore is not exist: end task */
		return;
	}
	for(;;)
	{
		/* Wait for new second event */
		while(xSemaphoreTake(xNewTimeEventWakeupSem, portMAX_DELAY) == pdTRUE)
		{
			/* Send data package to secondary devices */
			TRS_SyncProtoSendData();
		}
	}
}

static void TRS_SyncProtoPerSecondTask()
{
	/* Send newTimeEvent */
	if(xNewTimeEventWakeupSem != NULL) xSemaphoreGive(xNewTimeEventWakeupSem);
}

static void TRS_SyncProtoSendData()
{
	static uint8_t transPointer;

	/* Check RTC status */
	if(RTC_GetTimeIsValide() == false) return;

	/* Cache sending data */
	struct DateTime transDateTime;
	
	/* Cache the chronometric data */
	RTC_GetSystemDateTime(&transDateTime);

	/* Form data package */
	GetHeader(&transPointer);
	/* Store to buffer the chronometric data */
	transBuff[transPointer++] = transDateTime.second;
	transBuff[transPointer++] = transDateTime.minute;
	transBuff[transPointer++] = transDateTime.hour;
	transBuff[transPointer++] = transDateTime.day;
	transBuff[transPointer++] = transDateTime.month;
	transBuff[transPointer++] = (uint8_t)(transDateTime.year%100);

	/* Send formed data package */
	SendCmd(&transPointer);
}

static void SendCmd(uint8_t* pointer)
{
	transBuff[TRS_SNC_PRT_DATA_LENG_POS] =
			(*pointer) - TRS_SNC_PRT_HEADER_SIZE;
	uint8_t crc = CheckCRC(&transBuff[1], (*pointer) - 1);
	transBuff[(*pointer)++] = crc;
	TRS_SyncProtoDriverSendTX_Buff(transBuff, *pointer);
}

static void GetHeader(uint8_t* pointer)
{
	transBuff[TRS_SNC_PRT_MARKER_POS] = TRS_SNC_PRT_MARKER;
	*pointer = TRS_SNC_PRT_HEADER_SIZE;
}

static uint8_t CheckCRC(uint8_t* buff, uint8_t size)
{
	if(size <= 0) return 0xFF;
	uint8_t crc = 0;
	for(uint8_t i = 0; i < size; i ++)
	{
		crc ^= buff[i];
		crc = CRC_Table[crc];
	}
	return crc;
}
