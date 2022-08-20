/* This is settings manager application file (store/restore global settings)
*/

/* Preincludes -------------------------------------------------------------- */
#include "settings_NV_manager.h"

/* Includes ----------------------------------------------------------------- */
/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Drivers includes */
#include "rtc_driver.h"

/* Application includes */
#include "settings.h"
#include "main_app.h"
#include "html_txt_funcs.h"
#include "httpserver-netconn.h"
#include "web-server.h"
#include "rtc.h"
#include "sntp.h"
#include "TRS_sync_proto.h"
#include "UDP_logging.h"

/* Private constants -------------------------------------------------------- */
/* FreeRTOS constants */
#ifndef SETTINGS_MANAGER_APP_TASK_PRIORITY
#	define SETTINGS_MANAGER_APP_TASK_PRIORITY (tskIDLE_PRIORITY + 1)
#endif /*SETTINGS_MANAGER_APP_TASK_PRIORITY*/
#define SETTINGS_MANAGER_APP_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE)

/* Debug options ------------------------------------------------------------ */
//#define DEBUG_WAIT_FOR_NETWORK_CONNECT
//#define DEBUG_WAIT_FOR_5_SEC

/* Private structures and classes definitions ------------------------------- */
struct __attribute__ ((__packed__)) BackUpSettingsNV_Struct
{
	/* Network settings */
	enum ProtocolType protocolType;
	uint32_t IP_Addr;
	uint32_t netMask;
	uint32_t IP_GW;
	uint32_t IP_DNS;

	/* Login and password for UI */
	char login[HTML_LOGIN_MAX_LEN];
	char password[HTML_PASSW_MAX_LEN];

	/* Date and time settings */
	int8_t RTC_GMT;
	bool RTC_DST;

	/* NTP settings */
	bool SNTP_SyncEnabled;
	uint32_t SNTP_SyncPeriod;
	uint32_t SNTP_StartupDelay;

	/* Service settings (do not reset with other settings) -------------------*/
	/* RTC correction functions */
	uint8_t RTC_DriverAddedPulses;
	uint32_t RTC_DriverPulsesValue;

	/* Logging settings */
	bool loggingEnable;
	uint32_t loggingIP_Addr;
	uint16_t loggingPort;
	bool logEvents;
	bool logWarnings;
	bool logErrors;

	uint16_t dataIsValide;
};

/* Variables ---------------------------------------------------------------- */
/* Handle of the application task */
static TaskHandle_t xAppTaskHandle = NULL;
static struct BackUpSettingsNV_Struct bkSettingsStruct;
static volatile bool storeSettingsAndReset_MCU = false;

/* Private function prototypes ---------------------------------------------- */
static void AppTask();
static void RestoreAllSettings();
static void StoreAllSettings();
static bool IsChangedSettings();

/* Public functions --------------------------------------------------------- */
void SettingsNV_ManagerInit()
{
	/* Init driver */
	SettingsNV_ManagerDriverInit();
	
	/* Restore settings before RAM settings but after app init */
	RestoreAllSettings();

	/* Create application task */
	if(xTaskCreate(AppTask, "SettingsNV_Manager",
			SETTINGS_MANAGER_APP_TASK_STACK_SIZE, NULL,
			SETTINGS_MANAGER_APP_TASK_PRIORITY, &xAppTaskHandle) !=  pdPASS)
	{
		FreeRTOS_printf(("Could not create SettingsNV_Manager task\n"));
		return;
	}
}

void StoreSettingsAndReset_MCU()
{
	storeSettingsAndReset_MCU = true;
}

/* Private functions -------------------------------------------------------- */
/* Post-include ------------------------------------------------------------- */
#ifdef DEBUG_WAIT_FOR_NETWORK_CONNECT
/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#endif /*DEBUG_WAIT_FOR_NETWORK_CONNECT*/

static void AppTask()
{
#ifdef DEBUG_WAIT_FOR_5_SEC
	TickType_t xTimeOut;
	xTimeOut = xTaskGetTickCount();
#endif /*DEBUG_WAIT_FOR_5_SEC*/

	for(;;)
	{
		bool check = true;
#ifdef DEBUG_WAIT_FOR_NETWORK_CONNECT
		if(FreeRTOS_IsNetworkUp() == pdFALSE) check = false;
#endif /*DEBUG_WAIT_FOR_NETWORK_CONNECT*/

#ifdef DEBUG_WAIT_FOR_5_SEC
		if((xTaskGetTickCount() - xTimeOut) < 5000) check = false;
#endif /*DEBUG_WAIT_FOR_5_SEC*/

		if(check)
		{
			/* Check valid-data properties */
			if(bkSettingsStruct.dataIsValide != 0xA5A5)
			{
				bkSettingsStruct.dataIsValide = 0xA5A5;
				StoreAllSettings();
			}
			else if(IsChangedSettings())
			{
				/* Add one second delay before store settings */
				if(storeSettingsAndReset_MCU == false) vTaskDelay(1000);
				StoreAllSettings();
			}

			/* Additional checking flag "storeSettingsAndReset_MCU" */
			if(storeSettingsAndReset_MCU)
			{
				storeSettingsAndReset_MCU = false;

				/* Reset MCU */
				SoftResetCPU();
			}
		}

		/* Wait for next cycle (every second) */
		vTaskDelay(1000);
	}
}

static void RestoreAllSettings()
{
	uint8_t* arr = (uint8_t*) &bkSettingsStruct;

#if defined (STM32F4x_FAMILY)
	if(GetBackUpNV_Memory(arr, sizeof(bkSettingsStruct)) == false)
#endif /*STM32F1x_FAMILY*/
	{
		/* Some wrong with data size: reset valid-data properties */
		bkSettingsStruct.dataIsValide = 0;
	}

	/* Check valid-data properties */
	if(bkSettingsStruct.dataIsValide != 0xA5A5)
	{
		/* Data is invalid */
		return;
	}

	/* If need, validate data before restoring */
	/* Network settings */
	currProtocolType = bkSettingsStruct.protocolType;
	staticIP_Addr = bkSettingsStruct.IP_Addr;
	staticNetMask = bkSettingsStruct.netMask;
	staticIP_GW = bkSettingsStruct.IP_GW;
	staticIP_DNS = bkSettingsStruct.IP_DNS;
	//WebServer_ApplyNetworkSettings();

	/* Login and password for UI */
	SetLogin(bkSettingsStruct.login);
	SetPassword(bkSettingsStruct.password);

	/* Date and time settings */
	RTC_SetGMT(bkSettingsStruct.RTC_GMT);
	RTC_SetDST(bkSettingsStruct.RTC_DST);

	/* NTP settings */
	SNTP_SetSyncEnabled(bkSettingsStruct.SNTP_SyncEnabled);
	SNTP_SetSyncPeriod(bkSettingsStruct.SNTP_SyncPeriod);
	SNTP_SetStartupDelay(bkSettingsStruct.SNTP_StartupDelay);

	/* Service settings (do not reset with other settings) -------------------*/
	/* RTC correction functions */
	RTC_DriverSetCorrection(bkSettingsStruct.RTC_DriverAddedPulses,
			bkSettingsStruct.RTC_DriverPulsesValue);

	/* Logging settings */
	SetUDP_LoggingEnable(bkSettingsStruct.loggingEnable);
	SetUDP_LoggingIP_Addr(bkSettingsStruct.loggingIP_Addr);
	SetUDP_LoggingPort(bkSettingsStruct.loggingPort);
	SetUDP_LogEvents(bkSettingsStruct.logEvents);
	SetUDP_LogWarnings(bkSettingsStruct.logWarnings);
	SetUDP_LogErrors(bkSettingsStruct.logErrors);
}

static void StoreAllSettings()
{
	static bool storingChanges = false;

	/* Watch for re-entrance (if any) */
	if(storingChanges) return;
	storingChanges = true;

	taskENTER_CRITICAL();
	{
		/* Network settings */
		bkSettingsStruct.protocolType = currProtocolType;
		bkSettingsStruct.IP_Addr = staticIP_Addr,
		bkSettingsStruct.netMask = staticNetMask,
		bkSettingsStruct.IP_GW = staticIP_GW;
		bkSettingsStruct.IP_DNS = staticIP_DNS;

		/* Login and password for UI */
		SetValue(GetLogin(), bkSettingsStruct.login, HTML_LOGIN_MAX_LEN);
		SetValue(GetPassword(), bkSettingsStruct.password, HTML_PASSW_MAX_LEN);

		/* Date and time settings */
		bkSettingsStruct.RTC_GMT = RTC_GetGMT();
		bkSettingsStruct.RTC_DST = RTC_GetDST();

		/* NTP settings */
		bkSettingsStruct.SNTP_SyncEnabled = SNTP_GetSyncEnabled();
		bkSettingsStruct.SNTP_SyncPeriod = SNTP_GetSyncPeriod();
		bkSettingsStruct.SNTP_StartupDelay = SNTP_GetStartupDelay();

		/* Service settings (do not reset with other settings) ---------------*/
		/* RTC correction functions */
		RTC_DriverGetCorrection(&bkSettingsStruct.RTC_DriverAddedPulses,
				&bkSettingsStruct.RTC_DriverPulsesValue);

		/* Logging settings */
		bkSettingsStruct.loggingEnable = GetUDP_LoggingEnable();
		bkSettingsStruct.loggingIP_Addr = GetUDP_LoggingIP_Addr();
		bkSettingsStruct.loggingPort = GetUDP_LoggingPort();
		bkSettingsStruct.logEvents = GetUDP_LogEvents();
		bkSettingsStruct.logWarnings = GetUDP_LogWarnings();
		bkSettingsStruct.logErrors = GetUDP_LogErrors();

		uint8_t* arr = (uint8_t*) &bkSettingsStruct;

#if defined (STM32F4x_FAMILY)
		SetBackUpNV_Memory(arr, sizeof(bkSettingsStruct));
#endif /*STM32F1x_FAMILY*/
	}
	taskEXIT_CRITICAL();

	if(storeSettingsAndReset_MCU)
	{
		storeSettingsAndReset_MCU = false;

		/* Reset MCU */
		SoftResetCPU();
	}

	storingChanges = false;
}

static bool IsChangedSettings()
{
	/* Network settings */
	if(bkSettingsStruct.protocolType != currProtocolType) return true;
	if(bkSettingsStruct.IP_Addr != staticIP_Addr) return true;
	if(bkSettingsStruct.netMask != staticNetMask) return true;
	if(bkSettingsStruct.IP_GW != staticIP_GW) return true;
	if(bkSettingsStruct.IP_DNS != staticIP_DNS) return true;

	/* Login and password for UI */
	if(ValueCmp(GetLogin(), bkSettingsStruct.login) == false) return true;
	if(ValueCmp(GetPassword(), bkSettingsStruct.password) == false) return true;

	/* Date and time settings */
	if(bkSettingsStruct.RTC_GMT != RTC_GetGMT()) return true;
	if(bkSettingsStruct.RTC_DST != RTC_GetDST()) return true;

	/* NTP settings */
	if(bkSettingsStruct.SNTP_SyncEnabled != SNTP_GetSyncEnabled())
		return true;
	if(bkSettingsStruct.SNTP_SyncPeriod != SNTP_GetSyncPeriod())
		return true;
	if(bkSettingsStruct.SNTP_StartupDelay != SNTP_GetStartupDelay())
		return true;

	/* Service settings (do not reset with other settings) -------------------*/
	/* RTC correction functions */
	uint8_t addedPulses;
	uint32_t pulsesValue;
	RTC_DriverGetCorrection(&addedPulses, &pulsesValue);
	if(bkSettingsStruct.RTC_DriverAddedPulses != addedPulses) return true;
	if(bkSettingsStruct.RTC_DriverPulsesValue != pulsesValue) return true;

	/* Logging settings */
	if(bkSettingsStruct.loggingEnable != GetUDP_LoggingEnable()) return true;
	if(bkSettingsStruct.loggingIP_Addr != GetUDP_LoggingIP_Addr()) return true;
	if(bkSettingsStruct.loggingPort != GetUDP_LoggingPort()) return true;
	if(bkSettingsStruct.logEvents != GetUDP_LogEvents()) return true;
	if(bkSettingsStruct.logWarnings != GetUDP_LogWarnings()) return true;
	if(bkSettingsStruct.logErrors != GetUDP_LogErrors()) return true;

	/* Nothing changes */
	return false;
}
