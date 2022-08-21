/* This is module application file (module function)
*/

// Includes --------------------------------------------------------------------
#include "settings.h"

// Application includes.
#include "UDP_logging.h"

// Private constants -----------------------------------------------------------
// Logging constants
#ifndef MAX_UDP_LOG_MSG_SIZE
#	define MAX_UDP_LOG_MSG_SIZE 		32
#endif // MAX_UDP_LOG_MSG_SIZE

// Socket constants
#ifndef UDP_LOGGING_DEFAULT_PORT
#	define UDP_LOGGING_DEFAULT_PORT 	1536
#endif // UDP_LOGGING_DEFAULT_PORT

// FreeRTOS constants
#ifndef UDP_LOGGING_TASK_PRIORITY
#	define UDP_LOGGING_TASK_PRIORITY 	(tskIDLE_PRIORITY + 1)
#endif /*UDP_LOGGING_TASK_PRIORITY*/
#define UDP_LOGGING_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE)

// Debug options ---------------------------------------------------------------
//#define DEBUG_

// Variables -------------------------------------------------------------------
// Application variables
static bool loggingEnable = false;
static uint32_t loggingIP_Addr = 0;
static bool logEvents = false;
static bool logWarnings = false;
static bool logErrors = false;

// Logging variables
static char logStr[MAX_UDP_LOG_MSG_SIZE];
static uint16_t logLenght = 0;
static bool logPrepared = false;

// Handle of the audio transactions task that service audio streams
static TaskHandle_t xAppTaskHandle = NULL;

// The UDP socket
static Socket_t xUDPSocket = NULL;
static volatile bool rebindSocked = false;
static uint16_t loggingPort = 0;

// Other variables

// Private function prototypes -------------------------------------------------
static void AppTask();
static BaseType_t UDP_LoggingRecv(
		Socket_t xSocket, void* pvData, size_t xLength,
		const struct freertos_sockaddr* pxFrom,
		const struct freertos_sockaddr* pxDest);

// Public functions ------------------------------------------------------------
void UDP_LoggingInit()
{
	UDP_LoggingSetDefaults();

	// Create transactions application task
	if(xTaskCreate(AppTask, "UDP_Logging",
		UDP_LOGGING_TASK_STACK_SIZE, NULL,
		UDP_LOGGING_TASK_PRIORITY,
		&xAppTaskHandle) !=  pdPASS)
	{
		FreeRTOS_printf(("Could not create UDP_Logging task\n"));
		return;
	}
}

void UDP_LoggingSetDefaults()
{
	/* Set default parameters */
	loggingPort = FreeRTOS_htons(UDP_LOGGING_DEFAULT_PORT);

	loggingEnable = false;
	loggingIP_Addr = FreeRTOS_inet_addr_quick(configIP_ADDR0, configIP_ADDR1,
			 configIP_ADDR2, configIP_ADDR3);
	logEvents = false;
	logWarnings = false;
	logErrors = false;
}

void CheckUDP_LoggingTask()
{
	if(xAppTaskHandle == NULL)
	{
		// Recreate transactions application task
		if(xTaskCreate(AppTask, "UDP_Logging",
			UDP_LOGGING_TASK_STACK_SIZE, NULL,
			UDP_LOGGING_TASK_PRIORITY,
			&xAppTaskHandle) !=  pdPASS)
		{
			FreeRTOS_printf(("Could not create UDP_Logging task\n"));
			return;
		}
	}
}

// UDP_Logging functions for sending logs
bool UDP_LoggingPrepareLog(enum LogEventType type)
{
	logPrepared = false;
	if(loggingEnable == false) return false;

	// Reset lenght variable
	logLenght = 0;

	if(type == LOG_EVENT)
	{
		if(logEvents == false) return false;
		if(logLenght + sizeof("EV:") - 1 >= MAX_UDP_LOG_MSG_SIZE)
			return false;
		SetValue("EV:", logStr, sizeof("EV:"));
		logLenght += sizeof("EV:") - 1;
	}
	else if(type == LOG_WARNING)
	{
		if(logWarnings == false) return false;
		if(logLenght + sizeof("WR:") - 1 >= MAX_UDP_LOG_MSG_SIZE)
			return false;
		SetValue("WR:", logStr, sizeof("WR:"));
		logLenght += sizeof("WR:") - 1;
	}
	else if(type == LOG_ERROR)
	{
		if(logErrors == false) return false;
		if(logLenght + sizeof("ER:") - 1 >= MAX_UDP_LOG_MSG_SIZE)
			return false;
		SetValue("ER:", logStr, sizeof("ER:"));
		logLenght += sizeof("ER:") - 1;
	}
	else return false;

	logPrepared = true;
	return true;
}

bool UDP_LoggingAddToLog(char* log, uint16_t size)
{
	// Check, is log prepared
	if(logPrepared == false) return false;

	if(logLenght + size >= MAX_UDP_LOG_MSG_SIZE) return false;
	SetValue(log, &logStr[GetSizeOfStr(logStr, MAX_UDP_LOG_MSG_SIZE)], size);

	if(log[size - 1] == 0) logLenght += size - 1;
	else logLenght += size;

	// Add terminal symbol
	logStr[logLenght] = 0;

	return true;
}

bool UDP_LoggingAddNumberToLog(uint32_t num)
{
	// String for numeric values
	static char tmpValStr[8];
	SetNumToStr(num, tmpValStr, sizeof(tmpValStr));
	return UDP_LoggingAddToLog(tmpValStr,
			GetSizeOfStr(tmpValStr, sizeof(tmpValStr)));
}

bool UDP_LoggingSend()
{
	// Check, is log prepared
	if(logPrepared == false) return false;

	if(loggingEnable == false) return false;

	// Create address structure
	struct freertos_sockaddr xAddress;
	xAddress.sin_addr = loggingIP_Addr;
	xAddress.sin_port = loggingPort;

	// Send log
	FreeRTOS_sendto(xUDPSocket,
			logStr, GetSizeOfStr(logStr, MAX_UDP_LOG_MSG_SIZE), 0,
			&xAddress, sizeof(xAddress));

	return true;
}

// Application settings functions
bool GetUDP_LoggingEnable()
{
	return loggingEnable;
}

void SetUDP_LoggingEnable(bool val)
{
	// Story value
	loggingEnable = val;
}

uint32_t GetUDP_LoggingIP_Addr()
{
	return loggingIP_Addr;
}

void SetUDP_LoggingIP_Addr(uint32_t addr)
{
	loggingIP_Addr = addr;
}

uint16_t GetUDP_LoggingPort()
{
	return loggingPort;
}

void SetUDP_LoggingPort(uint16_t val)
{
	// Do not allow to set zero port
	if(val == 0) return;

	// Check for changes
	if(loggingPort == val) return;

	// Port numbers above 49408 (0xC100) to 65280 (0xff00) are considered
	// private numbers available to the IP stack for dynamic allocation,
	// and should therefore be avoided
	if((val >= 0xC100) && (val <= 0xff00)) return;

	// Update port ant make trigger for rebinding
	loggingPort = val;
	rebindSocked = true;

	// Wake up task
	vTaskResume(xAppTaskHandle);
}

bool GetUDP_LogEvents()
{
	return logEvents;
}

void SetUDP_LogEvents(bool val)
{
	// Story value
	logEvents = val;
}

bool GetUDP_LogWarnings()
{
	return logWarnings;
}

void SetUDP_LogWarnings(bool val)
{
	// Story value
	logWarnings = val;
}

bool GetUDP_LogErrors()
{
	return logErrors;
}

void SetUDP_LogErrors(bool val)
{
	// Story value
	logErrors = val;
}

// Private functions -----------------------------------------------------------
static void AppTask()
{
	// Create and init UDP socket
	xUDPSocket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM,
								 FREERTOS_IPPROTO_UDP);
	if(xUDPSocket != NULL)
	{
		struct freertos_sockaddr xAddress;
#if (ipconfigUSE_CALLBACKS != 0)
		BaseType_t xReceiveTimeOut = pdMS_TO_TICKS(0);
#else
		BaseType_t xReceiveTimeOut = pdMS_TO_TICKS(SNTP_RECV_TIMEOUT);
#endif

		// Check, is network up
		while(FreeRTOS_IsNetworkUp() == pdFALSE)
		{
			FreeRTOS_printf(("Wait for network up event\n"));
			vTaskDelay(300);
		}

		xAddress.sin_addr = 0ul;
		xAddress.sin_port = loggingPort;

		FreeRTOS_bind(xUDPSocket, &xAddress, sizeof(xAddress));
		FreeRTOS_setsockopt(xUDPSocket, 0, FREERTOS_SO_RCVTIMEO,
							&xReceiveTimeOut, sizeof(xReceiveTimeOut));

#if(ipconfigUSE_CALLBACKS != 0)
		// Create listener handler
		F_TCP_UDP_Handler_t xHandler;
		memset(&xHandler, '\0', sizeof(xHandler));
		xHandler.pxOnUDPReceive = UDP_LoggingRecv;
		FreeRTOS_setsockopt(xUDPSocket, 0, FREERTOS_SO_UDP_RECV_HANDLER,
							(void*) &xHandler, sizeof(xHandler));
#endif
	}
	else
	{
		FreeRTOS_printf(("Creating socket failed\n"));
		return;
	}

	for(;;)
	{
		// Every time check UDP-socket binded port
		if(rebindSocked)
		{
			rebindSocked = false;

			// Attempt graceful shutdown
			FreeRTOS_shutdown(xUDPSocket, FREERTOS_SHUT_RDWR);
			FreeRTOS_closesocket(xUDPSocket);

			// Must not drop off the end of the RTOS task -
			// delete the RTOS task
			xAppTaskHandle = NULL;
			vTaskDelete(xAppTaskHandle);
		}

		vTaskSuspend(xAppTaskHandle);

		// Wait for next cycle
		//vTaskDelay(1);
	}
}

static BaseType_t UDP_LoggingRecv(
		Socket_t xSocket, void* pvData, size_t xLength,
		const struct freertos_sockaddr* pxFrom,
		const struct freertos_sockaddr* pxDest)
{
	(void)xSocket;
	(void)pvData;
	(void)xLength;
	(void)pxFrom;
	(void)pxDest;

	if(FreeRTOS_GetIPAddress() == pxFrom->sin_addr)
	{
		// Do not receive own data
		return 1;
	}

	if(xLength == 0)
	{
		// It is not impossible to separate protocols
		return 1;
	}

	// Tell the driver not to store the RX data
	return 1;
}


