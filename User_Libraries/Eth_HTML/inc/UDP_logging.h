#ifndef _UDP_LOGGING_H_
#define _UDP_LOGGING_H_

// Includes --------------------------------------------------------------------
// Standard includes.
#include <stdbool.h>
#include <stddef.h>

// FreeRTOS includes.
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

// FreeRTOS+TCP includes.
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

// Application includes.
#include "html_txt_funcs.h"

// Public constants ------------------------------------------------------------
// Logging events types
enum LogEventType
{
	LOG_EVENT,
	LOG_WARNING,
	LOG_ERROR,
	UNSUPPORTED_LOG_EVENT
};

// Public function prototypes --------------------------------------------------
// Init functions
void UDP_LoggingInit();
void UDP_LoggingSetDefaults();
void CheckUDP_LoggingTask();

// UDP_Logging functions for sending logs
bool UDP_LoggingPrepareLog(enum LogEventType type);
bool UDP_LoggingAddToLog(char* log, uint16_t size);
bool UDP_LoggingAddNumberToLog(uint32_t num);
bool UDP_LoggingSend();

// Application settings functions
bool GetUDP_LoggingEnable();
void SetUDP_LoggingEnable(bool val);
uint32_t GetUDP_LoggingIP_Addr();
void SetUDP_LoggingIP_Addr(uint32_t val);
uint16_t GetUDP_LoggingPort();
void SetUDP_LoggingPort(uint16_t val);
bool GetUDP_LogEvents();
void SetUDP_LogEvents(bool val);
bool GetUDP_LogWarnings();
void SetUDP_LogWarnings(bool val);
bool GetUDP_LogErrors();
void SetUDP_LogErrors(bool val);

#endif // _UDP_LOGGING_H_
