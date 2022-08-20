#ifndef _SNTP_H_
#define _SNTP_H_

/* Includes ----------------------------------------------------------------- */
#include <stdint.h>
#include <stdbool.h>
#include "rtc.h"

/* Public constants --------------------------------------------------------- */
#define QUANT_NTP_SERVERS 				4
#define SNTP_TIMERS_USAGE 				5

enum NTP_RequestStatus
{
	NTP_RequestTimeOut,
	NTP_RequestFailed,
	NTP_RequestComplete,
	NTP_RequestUndefStatus
};

enum NTP_TimeStatus
{
	NTP_TimeInvalid,
	NTP_TimeNoActual,
	NTP_TimeValid,
};

/* Structs and classes definitions ------------------------------------------ */
struct NTP_ServerSettings
{
	bool enabled;
	char NTP[0xFF];
};

/* Public variables --------------------------------------------------------- */
extern struct NTP_ServerSettings NTP_Servers[];
extern enum NTP_RequestStatus lastNTP_RequestStatus;
extern uint8_t sntpRequestedServer;

/* Public function prototypes ----------------------------------------------- */
void SNTP_Init();
void SNTP_Deinit();
void SNTP_SetDefaults();

bool SNTP_GetLastSyncTime(struct DateTime* dateTime);
bool SNTP_SyncNow();
enum NTP_TimeStatus GetNTP_TimeStatus();

/* Settings functions */
bool SNTP_GetSyncEnabled();
void SNTP_SetSyncEnabled(bool enabled);
uint32_t SNTP_GetSyncPeriod();
void SNTP_SetSyncPeriod(uint32_t seconds);
uint32_t SNTP_GetStartupDelay();
void SNTP_SetStartupDelay(uint32_t seconds);

/* Functions, which can be overriden */
void SNTP_SetSystemCounter(uint32_t counter);
void SNTP_SetSystemCounterWithTicks(uint32_t counter, uint16_t ticks);

#endif /*_SNTP_H_*/
