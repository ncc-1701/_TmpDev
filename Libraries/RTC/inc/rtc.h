/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _RTC_H_
#define _RTC_H_

/* Includes ------------------------------------------------------------------*/
/* Standard includes */
#include <stdint.h>
#include <stdbool.h>

/* Settings include */
#include "settings.h"

/* Public constants ----------------------------------------------------------*/
enum AM_PM
{
	AM,
	PM
};

/* Start year */
#define FIRSTYEAR   				1970//2000//
#define DAYS_TO_FIRSTYEAR 			719528UL//146097*5 - 30*365 + 7//730485UL//
/* First day of week of first day of start year (0 = Sunday) */
#define FIRST_DAY    				6
/* System first fay of week (0 = Sunday) */
#define FIRST_DAY_OFWEEK 			1

enum DayOfWeek {
	MONDAY = 0, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY};
enum {	JANUARY = 1, FEBRUARY, 	MARCH, 		APRIL, 		MAY, 		JUNE,
		JULY, 		AUGUST, 	SEPTEMBER, 	OCTOBER, 	NOVEMBER, 	DECEMBER};

/* Summer time constants */
#define DELTA_TIME_IN_HOUR			1

#define SUMMER_TIME_MONTH			MARCH
#define SUMMER_TIME_DAY_OF_WEEK		SUNDAY
#define SUMMER_TIME_HOUR			3

/* Winter time constants */
#define WINTER_TIME_MONTH			OCTOBER
#define WINTER_TIME_DAY_OF_WEEK		SUNDAY
#define WINTER_TIME_HOUR			4

/* Structs and classes definitions -------------------------------------------*/
struct DateTime
{
	uint16_t year;	// 1..4095
	uint8_t month;	// 1..12
	uint8_t day;	// 1..31
	uint8_t dayOfWeek;	// 0..6, Sunday = 0
	uint8_t hour;	// 0..23
	uint8_t minute;	// 0..59
	uint8_t second;	// 0..59
};

/* Public function prototypes ------------------------------------------------*/
/* Init and registration per-second task functions */
void RTC_Init();
bool RTC_AddPerSecondTask(void (*fun_ptr)());
void RTC_SetDefaults();

/* Settings functions */
int8_t RTC_GetGMT();
void RTC_SetGMT(int8_t val);
bool RTC_GetDST();
void RTC_SetDST(bool val);

/* Calendar functions */
enum AM_PM ConvertToAM_PM(uint8_t* hour);
void Local_To_UTC_DateTime(struct DateTime* dateTime);
void UTC_To_Local_DateTime(struct DateTime* dateTime);
bool IsUTC_DST_Now(struct DateTime* dateTime);
void CounterToStruct(uint32_t second, struct DateTime* dateTime);
uint32_t StructToCounter(struct DateTime* dateTime);

uint8_t GetDayOfWeek(uint16_t year, uint8_t month, uint8_t day);
void VerifyDateTime(struct DateTime* dayTime);
void VerifyDayInMonth(struct DateTime* dayTime);
bool IsEquDateTime(struct DateTime* dateTime, struct DateTime* cmpDateTime);
bool ValueAsGMT_IsValide(int8_t val);

/* RTC functions */
void RTC_GetLocalDateTime(struct DateTime* dateTime);
void RTC_GetLocalDateTimeWithTicks(struct DateTime* dateTime, uint16_t* ticks);
void RTC_GetSystemDateTime(struct DateTime* dateTime);
void RTC_GetSystemDateTimeWithTicks(struct DateTime* dateTime, uint16_t* ticks);

void RTC_SetLocalDateTime(struct DateTime* dateTime);
void RTC_SetLocalDateTimeWithTicks(struct DateTime* dateTime, uint16_t ticks);
void RTC_SetSystemDateTime(struct DateTime* dateTime);
void RTC_SetSystemDateTimeWithTicks(struct DateTime* dateTime, uint16_t ticks);

void RTC_GetSystemCounter(uint32_t* counter);
void RTC_GetSystemCounterWithTicks(uint32_t* counter, uint16_t* ticks);
void RTC_SetSystemCounter(uint32_t counter);
void RTC_SetSystemCounterWithTicks(uint32_t counter, uint16_t ticks);
bool RTC_GetTimeIsValide();

#endif /* _RTC_H_ */
