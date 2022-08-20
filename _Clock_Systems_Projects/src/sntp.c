/* SNTP client module
  (minimal implementation of SNTPv4 as specified in RFC 4330) */

/* Includes ------------------------------------------------------------------*/
/* Standard includes. */
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_TCP_server.h"
#include "FreeRTOS_DHCP.h"
#include "FreeRTOS_DNS.h"

#include "settings.h"
#include "httpserver-netconn.h"
#include "sntp.h"

/* Private constants ---------------------------------------------------------*/
/* FreeRTOS constants */
#ifndef SNTP_APP_TASK_PRIORITY
	#define SNTP_APP_TASK_PRIORITY 		(tskIDLE_PRIORITY + 3)
#endif /*SNTP_APP_TASK_PRIORITY*/
#define SNTP_APP_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE)

/* SNTP server port */
#define SNTP_PORT                   123

/* Sanity check:
   Define this to
   == 0 to turn off sanity checks(default; smaller code)
   >= 1 to check address and port of the response packet to ensure the
        response comes from the server we sent the request to.
   >= 2 to check returned Originate Timestamp against Transmit Timestamp
        sent to the server(to ensure response to older request).
   >= 3 @todo: discard reply if any of the LI, Stratum, or Transmit Timestamp
        fields is 0 or the Mode field is not 4(unicast) or 5(broadcast).
   >= 4 @todo: to check that the Root Delay and Root Dispersion fields are each
        greater than or equal to 0 and less than infinity, where infinity is
        currently a cozy number like one second. This check avoids using a
        server whose synchronization source has expired for a very long time. */
#ifndef SNTP_CHECK_RESPONSE
#	define SNTP_CHECK_RESPONSE         	0
#endif

/* According to the RFC, this shall be a random delay
   between 1 and 5 minutes(in milliseconds) to prevent load peaks.
   This can be defined to a random generation function,
   which must return the delay in milliseconds as uint32_t.
   Turned off by default. */
#ifndef DEFAULT_NTP_STARTUP_DELAY
#	define DEFAULT_NTP_STARTUP_DELAY 	0
#endif /*DEFAULT_NTP_STARTUP_DELAY*/

#ifndef DEFAULT_NTP_SYNC_PERIOD
#	define DEFAULT_NTP_SYNC_PERIOD 		30
#endif /*DEFAULT_NTP_SYNC_PERIOD*/

/* SNTP receive timeout - in milliseconds
   Also used as retry timeout - this shouldn't be too low.
   Default is 3 seconds. */
#ifndef SNTP_RECV_TIMEOUT
#	define SNTP_RECV_TIMEOUT          	3000
#endif

#ifndef SNTP_ACTUAL_TIME_TIMEOUT
#	define SNTP_ACTUAL_TIME_TIMEOUT 	(5*60)
#endif /*SNTP_ACTUAL_TIME_TIMEOUT*/

/* SNTP macro to change system time including microseconds */
#ifdef SNTP_SET_ACCURATE_TIME
#	define SNTP_CALC_TIME_US 			1
#	define SNTP_RECEIVE_TIME_SIZE 		2
#else /*SNTP_SET_ACCURATE_TIME*/
#	define SNTP_CALC_TIME_US 			0
#	define SNTP_RECEIVE_TIME_SIZE 		1
#endif /*SNTP_SET_ACCURATE_TIME*/

/* Default retry timeout(in milliseconds) if the response
   received is invalid.
   This is doubled with each retry until SNTP_RETRY_TIMEOUT_MAX is reached. */
#ifndef SNTP_RETRY_TIMEOUT
#	define SNTP_RETRY_TIMEOUT          SNTP_RECV_TIMEOUT
#endif

/* Maximum retry timeout(in milliseconds). */
#ifndef SNTP_RETRY_TIMEOUT_MAX
#	define SNTP_RETRY_TIMEOUT_MAX     (SNTP_RETRY_TIMEOUT * 10)
#endif

enum SNTP_status
{
	SNTP_StatusSendRequest,
	SNTP_StatusTryNextServer
};

/* SNTP protocol defines -----------------------------------------------------*/
#define SNTP_MSG_LEN                48

enum SNTP_RESULT_KOD
{
	SNTP_ERR_UNDEF,
	SNTP_ERR_KOD,
	SNTP_ERR_OK
};

#define SNTP_OFFSET_LI_VN_MODE      0
#define SNTP_LI_MASK                0xC0
#define SNTP_LI_NO_WARNING          0x00
#define SNTP_LI_LAST_MINUTE_61_SEC  0x01
#define SNTP_LI_LAST_MINUTE_59_SEC  0x02
#define SNTP_LI_ALARM_CONDITION     0x03 //(clock not synchronized)

#define SNTP_VERSION_MASK           0x38
#define SNTP_VERSION                (4 << 3) //NTP Version 4

#define SNTP_MODE_MASK              0x07
#define SNTP_MODE_CLIENT            0x03
#define SNTP_MODE_SERVER            0x04
#define SNTP_MODE_BROADCAST         0x05

#define SNTP_OFFSET_STRATUM         1
#define SNTP_STRATUM_KOD            0x00

#define SNTP_OFFSET_ORIGINATE_TIME  24
#define SNTP_OFFSET_RECEIVE_TIME    32
#define SNTP_OFFSET_TRANSMIT_TIME   40

/* Number of seconds between 1900 and 1970 */
#define DIFF_SEC_1900_1970 			(2208988800UL)

/* SNTP packet format(without optional fields)
   Timestamps are coded as 64 bits:
   - 32 bits seconds since Jan 01, 1970, 00:00
   - 32 bits seconds fraction(0-padded)
   For future use, if the MSB in the seconds part is set, seconds are based
   on Feb 07, 2036, 06:28:16. */
__attribute__ ((packed))
struct sntp_msg
{
	uint8_t li_vn_mode;
	uint8_t stratum;
	uint8_t poll;
	uint8_t precision;
	uint32_t root_delay;
	uint32_t root_dispersion;
	uint32_t reference_identifier;
	uint32_t reference_timestamp[2];
	uint32_t originate_timestamp[2];
	uint32_t receive_timestamp[2];
	uint32_t transmit_timestamp[2];
};

/* Variables -----------------------------------------------------------------*/
/* Settings variables */
/* Addresses of servers */
struct NTP_ServerSettings NTP_Servers[QUANT_NTP_SERVERS];
static bool NTP_SyncEnabled;
static uint32_t syncPeriod;
static uint32_t startupDelay;

/* FreeRTOS variables */
/* Handle of the task that runs NTP synchronization. */
static TaskHandle_t xSNTP_WorkTaskHandle = NULL;
static SemaphoreHandle_t xNTPWakeupSem = NULL;

/* FreeRTOS IP variables */
/* The UDP socket used by the SNTP client */
static Socket_t xUDPSocket = NULL;

/* NTP task timeout */
static uint32_t ntpTimeout = 0;

/* NTP state variables */
static enum SNTP_status ntpStatus;
static uint8_t pCurrNTP_Serv;
static bool SNTP_Received;
static TickType_t xActualTimer;

/* NTP showing only state variables */
static uint32_t lastSyncTime;
static bool lastSyncTimeIsValide = false;
static enum NTP_TimeStatus timeStatus;
uint8_t sntpRequestedServer;
enum NTP_RequestStatus lastNTP_RequestStatus;

/* Other variables */
/* Structure member for received data */
static struct sntp_msg sntpmsg;

#if SNTP_CHECK_RESPONSE >= 1
/* Saves the last server address to compare with response */
static uint32_t sntp_last_server_address;
#endif /*SNTP_CHECK_RESPONSE >= 1*/

#if SNTP_CHECK_RESPONSE >= 2
/* Saves the last timestamp sent(which is sent back by the server)
   to compare against in response */
static uint32_t sntp_last_timestamp_sent[2];
#endif /*SNTP_CHECK_RESPONSE >= 2*/

/* Private function prototypes -----------------------------------------------*/
static void xSNTP_WorkTask(void *pvParameters);
static void SNTP_Request(void *arg);
static void SyncTimeWithTimestamp(uint32_t *receive_timestamp);
static void SNTP_InitializeRequest(struct sntp_msg *req);
static void SNTP_MakeRetryTimeout(void* arg);
static void SNTP_TryNextServer(void* arg);
static void SNTP_SelectFirstServer();
static BaseType_t SNTP_Recv(Socket_t xSocket, void* pvData, size_t xLength,
		const struct freertos_sockaddr* pxFrom, 
		const struct freertos_sockaddr* pxDest);

static void SNTP_SendRequest(uint32_t* server_addr);
static void SNTP_DNS_Found(const char* hostname, void* pvSearchID, 
						   uint32_t ipaddr);
static void SetSNTP_TaskStatus(enum SNTP_status status, uint32_t timeout);
static void SNTP_CheckForActualTimeout();

/* Public functions ----------------------------------------------------------*/
/* Initialize this module. Send out request  after startup delay. */
void SNTP_Init()
{
	/* Init variables before runing task */
	SNTP_SetDefaults();
	
	/* Init internal variables */
	SNTP_Received = false;
	lastSyncTimeIsValide = false;
	lastNTP_RequestStatus = NTP_RequestUndefStatus;
	timeStatus = NTP_TimeInvalid;
	
	/* Create SNTP task */
	if(xSNTP_WorkTaskHandle == NULL)
	{
		xTaskCreate(xSNTP_WorkTask, "SNTP_WorkTask", 
					configMINIMAL_STACK_SIZE, NULL,
					SNTP_APP_TASK_PRIORITY, &xSNTP_WorkTaskHandle);
		
		if(xSNTP_WorkTaskHandle == NULL)
		{
			FreeRTOS_printf("Could not create SNTP task\n");
			return;
		}
	}
	
	/* Create binary semaphore */
	xNTPWakeupSem = xSemaphoreCreateBinary();
	if(xNTPWakeupSem == NULL)
	{
		FreeRTOS_printf("Could not create SNTP binary semaphore\n");
		return;
	}
}

/* Stop this module. */
void SNTP_Deinit()
{
	/* Delete SNTP task */
	if(xSNTP_WorkTaskHandle != NULL)
	{
		vTaskDelete(xSNTP_WorkTaskHandle);
		xSNTP_WorkTaskHandle = NULL;
	}
	
	/* Close socked */
	if(xUDPSocket != NULL) 
	{
		FreeRTOS_closesocket(xUDPSocket);
		xUDPSocket = NULL;
	}
}

void SNTP_SetDefaults()
{
	/* Set default state for variables */
	NTP_SyncEnabled = true;
	startupDelay = DEFAULT_NTP_STARTUP_DELAY;
	SNTP_SetStartupDelay(DEFAULT_NTP_STARTUP_DELAY);
	SNTP_SetSyncPeriod(DEFAULT_NTP_SYNC_PERIOD);

	SetValue("ua.pool.ntp.org", NTP_Servers[0].NTP, sizeof(NTP_Servers[0].NTP));
	NTP_Servers[0].enabled = true;
	SetValue("de.pool.ntp.org", NTP_Servers[1].NTP, sizeof(NTP_Servers[1].NTP));
	NTP_Servers[1].enabled = true;
	SetValue("pool.ntp.org", NTP_Servers[2].NTP, sizeof(NTP_Servers[2].NTP));
	NTP_Servers[2].enabled = true;
	SetValue("192.168.0.1", NTP_Servers[3].NTP, sizeof(NTP_Servers[2].NTP));
	NTP_Servers[3].enabled = false;
}

bool SNTP_GetLastSyncTime(struct DateTime* dateTime)
{
	if(lastSyncTimeIsValide == false) return false;

	/* Convert counter to time structure */
	CounterToStruct(lastSyncTime, dateTime);
	return true;
}

bool SNTP_SyncNow()
{
	if(xUDPSocket == NULL) return false;
	if(NTP_SyncEnabled == false) return false;

	/* Reset time status */
	taskENTER_CRITICAL();
	{
		SNTP_Received = false;
		lastSyncTimeIsValide = false;
		lastNTP_RequestStatus = NTP_RequestUndefStatus;
		timeStatus = NTP_TimeInvalid;
	}
	taskEXIT_CRITICAL();

	/* Select first NTP server */
	SNTP_SelectFirstServer();

	/* Send out request immediately */
	if(xUDPSocket != NULL)
		SetSNTP_TaskStatus(SNTP_StatusSendRequest, 0);
	return true;
}

enum NTP_TimeStatus GetNTP_TimeStatus()
{
	/* Check for time status */
	SNTP_CheckForActualTimeout();
	return timeStatus;
}

/* Settings functions */
bool SNTP_GetSyncEnabled()
{
	return NTP_SyncEnabled;
}

void SNTP_SetSyncEnabled(bool enabled)
{
	/* Store value */
	NTP_SyncEnabled = enabled;
	
	if(enabled == false) 
	{
		/* Reset time status */
		taskENTER_CRITICAL(); 
		{
			SNTP_Received = false;
			lastSyncTimeIsValide = false;
			lastNTP_RequestStatus = NTP_RequestUndefStatus;
			timeStatus = NTP_TimeInvalid;
		}
		taskEXIT_CRITICAL();
		
		return;
	}
	
	/* After enable action - sync time */
	SNTP_SyncNow();
}

uint32_t SNTP_GetSyncPeriod()
{
	return syncPeriod;
}

void SNTP_SetSyncPeriod(uint32_t seconds)
{
	/* Validate period
	   "SNTPv4 RFC 4330 enforces a minimum update time of 15 seconds!" */
	if(seconds < 20) seconds = 20;
	
	/* One day - max value */
	if(seconds > 24*3600) seconds = 24*3600;
	
	/* Store value */
	syncPeriod = seconds;
	
	/* Update timeout */
	if(xUDPSocket != NULL) 
		SetSNTP_TaskStatus(SNTP_StatusSendRequest, (syncPeriod * 1000));
}

uint32_t SNTP_GetStartupDelay()
{
	return startupDelay;
}

void SNTP_SetStartupDelay(uint32_t seconds)
{
	/* Validate startup delay */
	/* One day - max value */
	if(seconds > 24*3600) seconds = 24*3600;

	/* Store value */
	startupDelay = seconds;
}

__attribute__((weak)) void SNTP_RTC_SetSystemCounter(uint32_t counter)
{
	RTC_SetSystemCounter(counter);
}

__attribute__((weak))
void SNTP_RTC_SetSystemCounterWithTicks(uint32_t counter, uint16_t ticks)
{
	RTC_SetSystemCounterWithTicks(counter, ticks);
}

/* Private functions ---------------------------------------------------------*/
static void xSNTP_WorkTask(void *pvParameters)
{
	/* Remove compiler warning about unused parameter. */
	(void)pvParameters;

	enum SNTP_status status = SNTP_StatusSendRequest;
	uint32_t timeout = 0;
	
	/* Create and init UDP socket */
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

		/* Check, is network up */
		while(FreeRTOS_IsNetworkUp() == pdFALSE)
		{
			FreeRTOS_printf(("Wait for network up event\n"));
			vTaskDelay(300);
		}

		xAddress.sin_addr = 0ul;
		xAddress.sin_port = FreeRTOS_htons(SNTP_PORT);

		FreeRTOS_bind(xUDPSocket, &xAddress, sizeof(xAddress));
		FreeRTOS_setsockopt(xUDPSocket, 0, FREERTOS_SO_RCVTIMEO, 
							&xReceiveTimeOut, sizeof(xReceiveTimeOut));
		
#if(ipconfigUSE_CALLBACKS != 0)
		/* Create listener handler */
		F_TCP_UDP_Handler_t xHandler;
		memset(&xHandler, '\0', sizeof(xHandler));
		xHandler.pxOnUDPReceive = SNTP_Recv;
		FreeRTOS_setsockopt(xUDPSocket, 0, FREERTOS_SO_UDP_RECV_HANDLER, 
							(void*) &xHandler, sizeof(xHandler));
#endif		
	}
	else 
	{
		FreeRTOS_printf(("Creating socket failed\n"));
		
		/* Try to next time */
		//SetSNTP_SendRequestTimeout(SNTP_RETRY_TIMEOUT);
		return;
	}
	
	/* Send first request after startup delay */
	timeout = startupDelay * 1000;
	
	/* Select first NTP server */
	SNTP_SelectFirstServer();

	for(;;)
	{
		/* Wait for event */
		while(xSemaphoreTake(xNTPWakeupSem, timeout) == pdTRUE)
		{
			/* Get current status and timeout */
			taskENTER_CRITICAL();
			{
				status = ntpStatus;
				timeout = ntpTimeout;
				
				/* Reset global variables */
				ntpStatus = SNTP_StatusSendRequest;
				ntpTimeout = 0;
			}
			taskEXIT_CRITICAL();
			
			/* Execute an action immediately if timeout is zero */
			//if(timeout == 0) break;
		}
		
		/* Could not take semaphore, so delay has been formed and
		   status is untouched */
		switch(status)
		{
		case SNTP_StatusTryNextServer:
			SNTP_TryNextServer(NULL);
			break;
			
		default:
			SNTP_Request(NULL);
			break;
		}
		
		/* Check for time status */
		SNTP_CheckForActualTimeout();

		/* Set default timeout for next cycle */
		timeout = syncPeriod * 1000;
	}
}

/* Send out an sntp request.
   @param arg is unused(only necessary to conform to sys_timeout) */
static void SNTP_Request(void *arg)
{
	/* Remove compiler warning about unused parameter. */
	(void) arg;
	
	/* Check for global flag */
	if(NTP_SyncEnabled == false) return;
	
	/* Initialize SNTP server address */
	/* Try to convert string to number */
	uint32_t  NTP_Serv_IP =
			FreeRTOS_inet_addr(NTP_Servers[pCurrNTP_Serv].NTP);
  	if(NTP_Serv_IP == 0)
  	{
  		/* Try to get IP thought DNS */
#if (ipconfigUSE_DNS == 1)
	#if (ipconfigDNS_USE_CALLBACKS == 1)
		FreeRTOS_gethostbyname_a(NTP_Servers[pCurrNTP_Serv].NTP,
								 SNTP_DNS_Found, (void*)NULL, 
								 (pdMS_TO_TICKS(SNTP_RETRY_TIMEOUT)));
	#else /*(ipconfigDNS_USE_CALLBACKS == 0)*/
		/* Lower own task’s priority to idle */
		vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
		NTP_Serv_IP = FreeRTOS_gethostbyname(NTP_Servers[pCurrNTP_Serv].NTP);
		SNTP_DNS_Found(NULL, NULL, NTP_Serv_IP);
		/* Restore own task’s priority */
		vTaskPrioritySet(NULL, SNTP_APP_TASK_PRIORITY);
	#endif /*ipconfigDNS_USE_CALLBACKS*/
		
		/* DNS request sent, wait for SNTP_DNS_Found being called */
		FreeRTOS_debug_printf(("SNTP_Request: Waiting for server address \
to be resolved.\n"));
#else /*(ipconfigUSE_DNS == 1)*/
		/* Address conversion failed, try another server */
		FreeRTOS_debug_printf(("SNTP_Request: Invalid server address, \
trying next server.\n"));

		SetSNTP_TaskStatus(SNTP_StatusTryNextServer, SNTP_RETRY_TIMEOUT);
#endif /*(ipconfigUSE_DNS == 1)*/
		return;
	}

  	/* NTP server IP is valid, send request */
	SNTP_SendRequest(&NTP_Serv_IP);
}

/* SNTP processing of received timestamp */
static void SyncTimeWithTimestamp(uint32_t *receive_timestamp)
{
	/* Convert SNTP time(1900-based) to unix GMT time (1970-based) */
	/* @todo: if MSB is 1, SNTP time is 2036-based! */
	uint32_t s = (FreeRTOS_htonl(receive_timestamp[0]) - DIFF_SEC_1900_1970);

#ifdef SNTP_SET_ACCURATE_TIME
	uint32_t ms = FreeRTOS_htonl(receive_timestamp[1])/4294967;
	s += ms / 1000;
	ms = ms % 1000;
	SNTP_RTC_SetSystemCounterWithTicks(s, (uint16_t)(ms));
	
	/* Display local time from GMT time */
	FreeRTOS_debug_printf(("SyncTimeWithTimestamp: %s, %u ms", ctime(&s), ms));
#else /*SNTP_SET_ACCURATE_TIME*/
	/* Change system time and/or the update the RTC clock */
	SNTP_RTC_SetSystemCounter(s);
	/* Display local time from GMT time */
	FreeRTOS_debug_printf(("SyncTimeWithTimestamp: %s", ctime(&s)));
#endif /*SNTP_SET_ACCURATE_TIME*/

	/* Store time of last successful synchronization */
	lastSyncTime = s;
	lastSyncTimeIsValide = true;
}

/* Initialize request struct to be sent to server. */
static void SNTP_InitializeRequest(struct sntp_msg *req)
{
	memset(req, 0, SNTP_MSG_LEN);
	req->li_vn_mode = SNTP_LI_NO_WARNING | SNTP_VERSION | SNTP_MODE_CLIENT;

#if SNTP_CHECK_RESPONSE >= 2
	{
		uint32_t sntp_time_sec, sntp_time_us;
		
		/* Fill in transmit timestamp and save it
		   in 'sntp_last_timestamp_sent' */
		RTC_GetSystemCounterWithTicks(&sntp_time_sec, &sntp_time_us);
		sntp_time_us *= 1000;
		sntp_last_timestamp_sent[0] = htonl(sntp_time_sec + DIFF_SEC_1900_1970);
		req->transmit_timestamp[0] = sntp_last_timestamp_sent[0];
		
		/* We send/save us instead of fraction to be faster... */
		sntp_last_timestamp_sent[1] = htonl(sntp_time_us);
		req->transmit_timestamp[1] = sntp_last_timestamp_sent[1];
	}
#endif /*SNTP_CHECK_RESPONSE >= 2*/
}

/* Retry: send a new request(and increase retry timeout).
   @param arg is unused(only necessary to conform to sys_timeout) */
static void SNTP_MakeRetryTimeout(void* arg)
{
	/* Remove compiler warning about unused parameter. */
	(void) arg;

	FreeRTOS_debug_printf(("SNTP_MakeRetryTimeout: Next request will be sent \
in %u ms\n", SNTP_RETRY_TIMEOUT));

	/* Set up a timer to send a retry */
	SetSNTP_TaskStatus(SNTP_StatusSendRequest, (SNTP_RETRY_TIMEOUT));
}

/* If Kiss-of-Death is received(or another packet parsing error),
   try the next server or retry the current server and increase the retry
   timeout if only one server is available.
   @param arg is unused(only necessary to conform to sys_timeout) */
static void SNTP_TryNextServer(void* arg)
{
	/* Remove compiler warning about unused parameter. */
	(void) arg;

	if(SNTP_Received) SNTP_Received = false;
	else
	{
		/* Store request status */
		taskENTER_CRITICAL(); 
		{
			/* Store request status */
			lastNTP_RequestStatus = NTP_RequestTimeOut;
			sntpRequestedServer = pCurrNTP_Serv;
		}
		taskEXIT_CRITICAL(); 
	}
	
#if (QUANT_NTP_SERVERS > 1)
	bool found = false;
	for(uint8_t i = 0; i < QUANT_NTP_SERVERS; i++)
	{
		pCurrNTP_Serv++;
		if(pCurrNTP_Serv >= QUANT_NTP_SERVERS) pCurrNTP_Serv = 0;
		if(NTP_Servers[pCurrNTP_Serv].enabled)
		{
			found = true;
			break;
		}
	}
	
	/* If all NTP disabled - reenable first NTP */
	if(found == false)
	{
		pCurrNTP_Serv = 0;
		NTP_Servers[pCurrNTP_Serv].enabled = true;
	}

	FreeRTOS_debug_printf(("SNTP_TryNextServer: Sending request to server %hu\n",
			(uint16_t )pCurrNTP_Serv));
#endif /*QUANT_NTP_SERVERS*/
						   
	SNTP_MakeRetryTimeout(NULL);
}

static void SNTP_SelectFirstServer()
{
	/* Search for first enabled NTP server */
	for(uint8_t i = 0; i < QUANT_NTP_SERVERS; i++)
	{
		if(NTP_Servers[i].enabled)
		{
			/* Store index to current server pointer */
			pCurrNTP_Serv = i;
			return;
		}
	}
	
	/* All NTP disabled: reenable first NTP and select it */
	NTP_Servers[0].enabled = true;
	pCurrNTP_Serv = 0;
}

/* UDP recv callback for the sntp pcb */
static BaseType_t SNTP_Recv(Socket_t xSocket, void* pvData, size_t xLength,
		const struct freertos_sockaddr* pxFrom, 
		const struct freertos_sockaddr* pxDest)
{
	/* Remove compiler warning about unused parameter. */
	(void)xSocket;

	SNTP_Received = true;
	struct sntp_msg* rec_msg;
	
	/* Packet received: prepare for next synchronization cycle */
	SetSNTP_TaskStatus(SNTP_StatusSendRequest, syncPeriod * 1000);
	
	enum SNTP_RESULT_KOD result = SNTP_ERR_UNDEF;
#if SNTP_CHECK_RESPONSE >= 1
	/* Check server address and port */
	if(ip_addr_cmp(addr, &sntp_last_server_address) && (port == SNTP_PORT))
#else /*SNTP_CHECK_RESPONSE >= 1*/
	/* Remove compiler warning about unused parameter. */
	(void) pxFrom;
	(void) pxDest;
#endif /*SNTP_CHECK_RESPONSE >= 1*/
	{
		/* Process the response */
		if(xLength == SNTP_MSG_LEN) 
		{
			rec_msg = (struct sntp_msg*)pvData;
			rec_msg->li_vn_mode &= SNTP_MODE_MASK;
			/* If this is a SNTP response... */
			if((rec_msg->li_vn_mode == SNTP_MODE_SERVER) || 
			   (rec_msg->li_vn_mode == SNTP_MODE_BROADCAST)) 
			{
				if(rec_msg->stratum == SNTP_STRATUM_KOD) 
				{
					/* Kiss-of-death packet.
					   Use another server or increase UPDATE_DELAY. */
					result = SNTP_ERR_KOD;
					FreeRTOS_debug_printf(("SNTP_Recv: \
Received Kiss-of-Death\n"));
				} 
				else 
				{
#if SNTP_CHECK_RESPONSE >= 2
					/* Check originate_timetamp against
					   sntp_last_timestamp_sent */
					if((rec_msg->originate_timestamp[0] != 
						sntp_last_timestamp_sent[0]) ||
					   (rec_msg->originate_timestamp[1] != 
						sntp_last_timestamp_sent[1]))
					{
						FreeRTOS_debug_printf(("SNTP_Recv: Invalid originate \
timestamp in response\n"));
					} 
					else
#endif /*SNTP_CHECK_RESPONSE >= 2*/
					/* @todo: add code for SNTP_CHECK_RESPONSE >= 3 and >= 4
					   here */
					{
						/* Correct answer */
						result = SNTP_ERR_OK;
					}
				}
			} 
			else FreeRTOS_debug_printf(("SNTP_Recv: Invalid mode in response:\
%hu\n", (uint16_t )(rec_msg->li_vn_mode)));
		} 
		else FreeRTOS_debug_printf
(("SNTP_Recv: Invalid packet length: %hu\n", xLength));
	}
	if(result == SNTP_ERR_OK) 
	{
		SyncTimeWithTimestamp((uint32_t*)(rec_msg->receive_timestamp));
		
		taskENTER_CRITICAL(); 
		{
			/* Store request status */
			lastNTP_RequestStatus = NTP_RequestComplete;
			sntpRequestedServer = pCurrNTP_Serv;
			
			/* Set actual timeout */
			xActualTimer = xTaskGetTickCount();

			/* Update time status */
			timeStatus = NTP_TimeValid;
		}
		taskEXIT_CRITICAL(); 
		
		/* After successful synchronization try again send request to
		   first NTP-server */
		SNTP_SelectFirstServer();
		
		/* Set up timeout for next request */
		//SetSNTP_SendRequestTimeout(syncPeriod * 1000);
	
		FreeRTOS_debug_printf(("SNTP_Recv: Scheduled next time request: \
%u ms\n", (uint32_t)syncPeriod * 1000));
	} 
	else if(result == SNTP_ERR_KOD) 
	{
		taskENTER_CRITICAL(); 
		{
			/* Store request status */
			lastNTP_RequestStatus = NTP_RequestFailed;
			sntpRequestedServer = pCurrNTP_Serv;
		}
		taskEXIT_CRITICAL(); 
		
		/* Kiss-of-death packet. Use another server or increase UPDATE_DELAY. */
		SNTP_TryNextServer(NULL);
	} 
	else 
	{
		/* Store request status */
		lastNTP_RequestStatus = NTP_RequestFailed;
		sntpRequestedServer = pCurrNTP_Serv;
		
		/* Another error, try the same server again */
		SNTP_MakeRetryTimeout(NULL);
	}
							   
	/* Tell the driver not to store the RX data */
	return 1;
}

/* Actually send an sntp request to a server.
   @param server_addr resolved IP address of the SNTP server */
static void SNTP_SendRequest(uint32_t* server_addr)
{
	struct freertos_sockaddr xAddress;
	memset (&sntpmsg, '\0', sizeof(sntpmsg));
	FreeRTOS_debug_printf(("SNTP_SendRequest: Sending request to server\n"));
	
	/* Initialize request message */
	SNTP_InitializeRequest(&sntpmsg);
	
	/* Send request */
	xAddress.sin_addr = *server_addr;
	xAddress.sin_port = FreeRTOS_htons(SNTP_PORT);
	FreeRTOS_sendto(xUDPSocket, (void*)&sntpmsg, sizeof(sntpmsg), 0, 
					&xAddress, sizeof(xAddress));
	
	/* Set up receive timeout: try next server or retry on timeout */
	SetSNTP_TaskStatus(SNTP_StatusTryNextServer, SNTP_RECV_TIMEOUT);
#if SNTP_CHECK_RESPONSE >= 1
	/* Save server address to verify it in SNTP_Recv */
	ip_addr_set(&sntp_last_server_address, server_addr);
#endif /*SNTP_CHECK_RESPONSE >= 1*/
	
	if(SNTP_Received) SNTP_Received = false;
	else
	{
		/* Store requested server index */
		sntpRequestedServer = pCurrNTP_Serv;
	}
}

/* DNS found callback when using DNS names as server address. */
static void SNTP_DNS_Found(const char* hostname, void* pvSearchID, 
						   uint32_t ipaddr)
{
	/* Remove compiler warning about unused parameter. */
	(void)hostname;
	(void)pvSearchID;

	//xTimerStop(xSNTP_StatusTryNextServer, 0);
	
	if(ipaddr != 0) 
	{
		/* Address resolved, send request */
		FreeRTOS_debug_printf(("SNTP_DNS_Found: Server address resolved, \
sending request\n"));
		SNTP_SendRequest(&ipaddr);
	} 
	else 
	{
		/* DNS resolving failed -> try another server */
		FreeRTOS_debug_printf(("SNTP_DNS_Found: Failed to resolve server \
address resolved, trying next server\n"));
		SNTP_TryNextServer(NULL);
	}
}

static void SetSNTP_TaskStatus(enum SNTP_status status, uint32_t timeout)
{
	taskENTER_CRITICAL(); 
	{
		ntpStatus = status;
		ntpTimeout = timeout;
	}
	taskEXIT_CRITICAL(); 	
	
	if(xNTPWakeupSem != NULL) xSemaphoreGive(xNTPWakeupSem);
}

static void SNTP_CheckForActualTimeout()
{
	if(timeStatus != NTP_TimeValid)
	{
		/* No need to watch for time status */
		return;
	}

	/* Time is valid: watch for actual timeout */
	TickType_t xCurrTime = xTaskGetTickCount();
	if(xCurrTime - xActualTimer < (syncPeriod + SNTP_ACTUAL_TIME_TIMEOUT)*1000)
	{
		/* Time is still valid */
		return;
	}

	/* Time is not valid any more */
	timeStatus = NTP_TimeNoActual;
}
