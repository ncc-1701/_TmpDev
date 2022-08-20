/* This is RTC source file (get/set time functions) */

/* Includes ------------------------------------------------------------------*/
/* Standard includes */
#include <stdint.h>
#include <stdbool.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Drivers includes */
#include "rtc_driver.h"

/* Application includes */
#include "settings.h"
#include "rtc.h"

/* Constants -----------------------------------------------------------------*/
/* FreeRTOS constants */
#ifndef RTC_APP_TASK_PRIORITY
#	define RTC_APP_TASK_PRIORITY 		(tskIDLE_PRIORITY + 3)
#endif /*RTC_APP_TASK_PRIORITY*/

#ifndef LOCAL_GMT
#	define LOCAL_GMT 					2
#endif /* LOCAL_GMT */

/* Constants for registration per-second tasks */
#ifndef MAX_RTC_PER_SECOND_TASKS
#	define MAX_RTC_PER_SECOND_TASKS 	8
#endif /* MAX_RTC_PER_SECOND_TASKS */

/* Private constants */
/* FreeRTOS constants */
#define RTC_APP_TASK_STACK_SIZE 	(configMINIMAL_STACK_SIZE)

/* UTC summer and winter time constants */
#define SUMMER_TIME_HOUR_UTC		(int8_t)(SUMMER_TIME_HOUR - GMT)
#define WINTER_TIME_HOUR_UTC		(int8_t)(WINTER_TIME_HOUR - GMT -\
										DELTA_TIME_IN_HOUR)

/* Days in months */
static const uint8_t daysInMonth[] =
{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* Variables -----------------------------------------------------------------*/
/* Handle of the application task */
static TaskHandle_t xAppTaskHandle = NULL;
static SemaphoreHandle_t xNewTimeEventWakeupSem = NULL;

/* Settings variables */
static bool DST;
static int8_t GMT;

/* Date and time state */
bool timeIsValide;

/* Variables for registration per-second tasks (they have to be preinited!!!) */
void (*perSecondTasks[MAX_RTC_PER_SECOND_TASKS])();
static uint8_t pTasks = 0;

/* Private function prototypes -----------------------------------------------*/
static void RTC_Task();
static uint8_t GetDaysInMonth(uint8_t numMonth, uint16_t year);
static void GetCorrectDateTime(
		struct DateTime* correctDateTime, uint8_t correctMonth,
		uint8_t correctDayOfWeek, uint8_t correctHour);
static void GetWinterDateTime(struct DateTime* correctDateTime);
static void GetSummerDateTime(struct DateTime* correctDateTime);
static void GetWinterDateTime_UTC(struct DateTime* correctDateTime);
static void GetSummerDateTime_UTC(struct DateTime* correctDateTime);
static int8_t GetWinterSummerDelta(
		struct DateTime* currentDateTime,
		struct DateTime* winterDateTime, struct DateTime* summerDateTime);
static void CorrectDateTime_Hour(struct DateTime* dateTime, int8_t deltaHour);

/* Public functions --------------------------------------------------------- */
void RTC_Init()
{

	RTC_DriverInit();

	/* Set defaults */
	RTC_SetDefaults();

	/* Init internal variables */
	timeIsValide = false;

	/* Create application task */
	if(xTaskCreate(RTC_Task, "RTC",
			RTC_APP_TASK_STACK_SIZE, NULL,
			RTC_APP_TASK_PRIORITY, &xAppTaskHandle) !=  pdPASS)
	{
		FreeRTOS_printf(("Could not create RTC task\n"));
		return;
	}

	/* Create binary semaphore */
	xNewTimeEventWakeupSem = xSemaphoreCreateBinary();
	if(xNewTimeEventWakeupSem == NULL)
	{
		FreeRTOS_printf("Could not create RTC binary semaphore\n");
		return;
	}

	/* Driver and all FreeRTOS operations successful: set valid flag */
	timeIsValide = true;
}

void RTC_SetDefaults()
{
#ifdef DST_PRESENT
	DST = true;
#else /* DST_PRESENT */
	DST = false;
#endif /* DST_PRESENT */

#ifdef LOCAL_GMT
GMT = LOCAL_GMT;
#else /* LOCAL_GMT */
GMT = 2;
#endif /* LOCAL_GMT */
}

bool RTC_AddPerSecondTask(void (*fun_ptr)())
{
	/* Check for buffer full state */
	if(pTasks >= MAX_RTC_PER_SECOND_TASKS) return false;

	/* Register function */
	perSecondTasks[pTasks] = fun_ptr;

	pTasks++;
	return true;
}

/* Settings functions */
int8_t RTC_GetGMT()
{
	return GMT;
}

void RTC_SetGMT(int8_t val)
{
	if(ValueAsGMT_IsValide(val) == false) return;
	GMT = val;
}

bool RTC_GetDST()
{
	return DST;
}

void RTC_SetDST(bool val)
{
	DST = val;
}

/* Calendar functions */
enum AM_PM ConvertToAM_PM(uint8_t* hour)
{
	uint8_t storedHour = *hour;
	*hour = *hour%12;
	if(*hour == 0) *hour = 12;
	if(storedHour < 12) return AM;
	return PM;
}

void Local_To_UTC_DateTime(struct DateTime* dateTime)
{
	int8_t deltaHours = 0;
	if(DST)
	{
		/* Create values for winter and summer correction time */
		struct DateTime winterDateTime = *dateTime;
		struct DateTime summerDateTime = *dateTime;
		/* Calculate them */
		GetWinterDateTime(&winterDateTime);
		GetSummerDateTime(&summerDateTime);
		deltaHours -= GMT + GetWinterSummerDelta(dateTime, 
						&winterDateTime, &summerDateTime);
	}
	else deltaHours -= GMT;
	CorrectDateTime_Hour(dateTime, deltaHours);
}

void UTC_To_Local_DateTime(struct DateTime* dateTime)
{
	int8_t deltaHours = 0;
	if(DST)
	{
		/* Create values for winter and summer correction time */
		struct DateTime winterDateTime = *dateTime;
		struct DateTime summerDateTime = *dateTime;
		/* Calculate them */
		GetWinterDateTime_UTC(&winterDateTime);
		GetSummerDateTime_UTC(&summerDateTime);
		deltaHours += GMT + GetWinterSummerDelta(dateTime, 
						&winterDateTime, &summerDateTime);
	}
	else deltaHours += GMT;
	CorrectDateTime_Hour(dateTime, deltaHours);
}

bool IsUTC_DST_Now(struct DateTime* dateTime)
{
	/* Create values for winter and summer correction time */
	struct DateTime winterDateTime = *dateTime;
	struct DateTime summerDateTime = *dateTime;
	/* Calculate them */
	GetWinterDateTime_UTC(&winterDateTime);
	GetSummerDateTime_UTC(&summerDateTime);
	if(GetWinterSummerDelta(dateTime, &winterDateTime, &summerDateTime) != 0)
		return true;
	return false;
}

void CounterToStruct(uint32_t second, struct DateTime* dateTime)
{
	uint16_t day;
	uint8_t year;
	uint16_t dayofyear;
	uint8_t leap400;
	uint8_t month;

	dateTime->second = second % 60;
	second /= 60;
	dateTime->minute = second % 60;
	second /= 60;
	dateTime->hour = second % 24;
	day = (uint16_t)(second / 24);

	dateTime->dayOfWeek = (day + 7 + FIRST_DAY - FIRST_DAY_OFWEEK) % 7;

	year = FIRSTYEAR % 100;	// 0..99
	leap400 = 4 - ((FIRSTYEAR - 1) / 100 & 3);	// 4, 3, 2, 1

	for(;;)
	{
		dayofyear = 365;
		if( (year & 3) == 0 )
		{
			dayofyear = 366;	// leap year
			if( year == 0 || year == 100 || year == 200 )
			{
				// 100 year exception
				if( --leap400 )
				{
					// 400 year exception
					dayofyear = 365;
				}
			}
		}
		if( day < dayofyear ) break;

		day -= dayofyear;
		year++;	// 00..136 / 99..235
	}
	dateTime->year = year + FIRSTYEAR / 100 * 100;	// + century

	if((dayofyear & 1) && (day > 58))
	{
		// no leap year and after 28.2.
		day++;	// skip 29.2.
	}

	for( month = 1; day >= daysInMonth[month-1]; month++ )
	{
		day -= daysInMonth[month-1];
	}

	dateTime->month = month;	// 1..12
	dateTime->day = day + 1;	// 1..31
}

uint32_t StructToCounter(struct DateTime* dateTime)
{
	uint8_t i;
	uint32_t result = 0;
	uint16_t idx, year;

	year = dateTime->year;

	// Calculate days of years before
	result = (uint32_t)year * 365;
	if (dateTime->year >= 1)
	{
		result += (year + 3) / 4;
		result -= (year - 1) / 100;
		result += (year - 1) / 400;
	}

	/* Substrate days from 0 to FIRST_YEAR */
	result -= DAYS_TO_FIRSTYEAR;

	// Make month an array index
	idx = dateTime->month - 1;

	// Loop thru each month, adding the days
	for (i = 0; i < idx; i++)
	{
		result += daysInMonth[i];
	}

	// Leap year? adjust February
	if (year%400 == 0 || (year%4 == 0 && year%100 !=0)){}
	else if (dateTime->month > 2) result--;

	// Add remaining days
	result += dateTime->day;

	// Convert to seconds, add all the other stuff
	result = (result-1) * 86400L + (uint32_t)dateTime->hour * 3600 +
	(uint32_t)dateTime->minute * 60 + dateTime->second;

	return result;
}

uint8_t GetDayOfWeek(uint16_t year, uint8_t month, uint8_t day)
{
    uint8_t a = (14 - month) / 12;
    int16_t y = year - a;
    int8_t m = month + 12 * a - 2;
    return (uint8_t)((7000 - FIRST_DAY_OFWEEK + 
		(day + y + y / 4 - y / 100 + y / 400 + (31 * m) / 12)) % 7);
}

/* Verifying date and time for correctly value */
void VerifyDateTime(struct DateTime* dateTime)
{
	if(dateTime->second >= 60) dateTime->second = 0;
	if(dateTime->minute >= 60) dateTime->minute = 0;
	if(dateTime->hour >= 24) dateTime->hour = 0;
	if((dateTime->day > 31) ||(dateTime->day == 0) ) dateTime->day = 1;
	if((dateTime->month > 12) ||(dateTime->month == 0) ) dateTime->month = 1;
	if(dateTime->year >= 2100) dateTime->year = 2000;
}

void VerifyDayInMonth(struct DateTime* dateTime)
{
	if(dateTime->day == 0) dateTime->day = 1;
	else 
	{
		uint8_t currDaysInMonth = GetDaysInMonth(dateTime->month, dateTime->year);
		if(dateTime->day > currDaysInMonth) dateTime->day = currDaysInMonth;
	}
}

bool IsEquDateTime(struct DateTime* dateTime, struct DateTime* cmpDateTime)
{
	if(dateTime->second != cmpDateTime->second) return false;
	if(dateTime->minute != cmpDateTime->minute) return false;
	if(dateTime->hour != cmpDateTime->hour) return false;
	if(dateTime->day != cmpDateTime->day) return false;
	if(dateTime->month != cmpDateTime->month) return false;
	if(dateTime->year != cmpDateTime->year) return false;
	return true;
}

bool ValueAsGMT_IsValide(int8_t val)
{
	if(val < -11) return false;
	if(val > 13) return false;
	return true;
}

/* RTC functions */
void RTC_GetLocalDateTime(struct DateTime* dateTime)
{
	RTC_GetLocalDateTimeWithTicks(dateTime, NULL);
}

void RTC_GetLocalDateTimeWithTicks(struct DateTime* dateTime, uint16_t* ticks)
{
	/* Check for RTC driver valid state */
	if(timeIsValide == false) return;

	RTC_DriverGetDateTime(dateTime, ticks);
	UTC_To_Local_DateTime(dateTime);
}

void RTC_GetSystemDateTime(struct DateTime* dateTime)
{
	RTC_GetSystemDateTimeWithTicks(dateTime, NULL);
}

void RTC_GetSystemDateTimeWithTicks(struct DateTime* dateTime, uint16_t* ticks)
{
	/* Check for RTC driver valid state */
	if(timeIsValide == false) return;

	RTC_DriverGetDateTime(dateTime, ticks);
}

void RTC_SetLocalDateTime(struct DateTime* dateTime)
{
	RTC_SetLocalDateTimeWithTicks(dateTime, 0);
}

void RTC_SetLocalDateTimeWithTicks(struct DateTime* dateTime, uint16_t ticks)
{
	/* Check for RTC driver valid state */
	if(timeIsValide == false) return;

	VerifyDateTime(dateTime);
	VerifyDayInMonth(dateTime);
	Local_To_UTC_DateTime(dateTime);

	/* Calculate day of week and store date and time */
	dateTime->dayOfWeek =
			GetDayOfWeek(dateTime->year, dateTime->month, dateTime->day);
	RTC_DriverSetDateTime(dateTime, ticks);
}

void RTC_SetSystemDateTime(struct DateTime* dateTime)
{
	RTC_SetSystemDateTimeWithTicks(dateTime, 0);
}

void RTC_SetSystemDateTimeWithTicks(struct DateTime* dateTime, uint16_t ticks)
{
	/* Check for RTC driver valid state */
	if(timeIsValide == false) return;

	VerifyDateTime(dateTime);
	VerifyDayInMonth(dateTime);

	/* Calculate day of week and store date and time */
	dateTime->dayOfWeek =
			GetDayOfWeek(dateTime->year, dateTime->month, dateTime->day);
	RTC_DriverSetDateTime(dateTime, ticks);
}

void RTC_GetSystemCounter(uint32_t* counter)
{
	RTC_GetSystemCounterWithTicks(counter, NULL);
}

void RTC_GetSystemCounterWithTicks(uint32_t* counter, uint16_t* ticks)
{
	/* Check for RTC driver valid state */
	if(timeIsValide == false) return;

	struct DateTime dateTime;
	RTC_GetSystemDateTimeWithTicks(&dateTime, ticks);
	*counter = StructToCounter(&dateTime);
}

void RTC_SetSystemCounter(uint32_t counter)
{
	RTC_SetSystemCounterWithTicks(counter, 0);
}

void RTC_SetSystemCounterWithTicks(uint32_t counter, uint16_t ticks)
{
	/* Check for RTC driver valid state */
	if(timeIsValide == false) return;

	struct DateTime dateTime;
	CounterToStruct(counter, &dateTime);
	RTC_SetSystemDateTimeWithTicks(&dateTime, ticks);
	return;
}

bool RTC_GetTimeIsValide()
{
	return timeIsValide;
}

/* Override some external driver functions */
void RTC_DriverPerSecondEvent()
{
	/* Send newTimeEvent */
	if(xNewTimeEventWakeupSem != NULL)
	{
		xSemaphoreGiveFromISR(xNewTimeEventWakeupSem, NULL);
	}
}

/* Private functions ---------------------------------------------------------*/
static void RTC_Task()
{
	for(;;)
	{
		/* Wait for new second event */
		while(xSemaphoreTake(xNewTimeEventWakeupSem, portMAX_DELAY) == pdTRUE)
		{
			/* Execute all external tasks */
			void (*fun_ptr)();
			for(uint8_t i = 0; i < pTasks; i++)
			{
				fun_ptr = perSecondTasks[i];
				fun_ptr();
			}
		}
	}
}

static uint8_t GetDaysInMonth(uint8_t numMonth, uint16_t year)
{
	if((numMonth == 0) || (numMonth > sizeof(daysInMonth))) return 0;
	else if(numMonth != 2) return(uint8_t)(daysInMonth[numMonth - 1]);
	else
	{
		if((year & 0x03) ||(year == 0)) return 28;
		else return 29;
	}
}

/* Get date for summer/winter correction (it depend of current year) */
static void GetCorrectDateTime(
		struct DateTime* correctDateTime, uint8_t correctMonth,
		uint8_t correctDayOfWeek, uint8_t correctHour)
{
	/* Get last day of month for correction */
	uint8_t dayForCorrectTime = GetDaysInMonth(correctMonth, correctDateTime->year);
	/* Calculate day of week */
	uint8_t dayOfWeekForCorrectTime = 
		GetDayOfWeek(correctDateTime->year, correctMonth, dayForCorrectTime);
	/* Recalculate date for needing last day of week (for ex. last Sunday) */
	if(dayOfWeekForCorrectTime < correctDayOfWeek) dayOfWeekForCorrectTime += 7;
	while(dayOfWeekForCorrectTime != correctDayOfWeek)
	{
		dayOfWeekForCorrectTime--;
		dayForCorrectTime--;
	}
	/* Change value */
	correctDateTime->month = correctMonth;
	correctDateTime->day = dayForCorrectTime;
	correctDateTime->hour = correctHour;
	correctDateTime->minute = 0;
	correctDateTime->second = 0;
}

static void GetWinterDateTime_UTC(struct DateTime* correctDateTime)
{
	GetCorrectDateTime(correctDateTime, WINTER_TIME_MONTH,
						WINTER_TIME_DAY_OF_WEEK, WINTER_TIME_HOUR - GMT);
}
static void GetSummerDateTime_UTC(struct DateTime* correctDateTime)
{
	GetCorrectDateTime(correctDateTime, SUMMER_TIME_MONTH,
						SUMMER_TIME_DAY_OF_WEEK, SUMMER_TIME_HOUR - GMT);
}
static void GetWinterDateTime(struct DateTime* correctDateTime)
{
	GetCorrectDateTime(correctDateTime, WINTER_TIME_MONTH,
						WINTER_TIME_DAY_OF_WEEK, WINTER_TIME_HOUR);
}
static void GetSummerDateTime(struct DateTime* correctDateTime)
{
	GetCorrectDateTime(correctDateTime, SUMMER_TIME_MONTH,
						SUMMER_TIME_DAY_OF_WEEK, SUMMER_TIME_HOUR);
}

static int8_t GetWinterSummerDelta(
		struct DateTime* currentDateTime,
		struct DateTime* winterDateTime, struct DateTime* summerDateTime)
{
	uint16_t curr = (currentDateTime->month << 10) +
		(currentDateTime->day << 5) + currentDateTime->hour;
	uint16_t winter = (winterDateTime->month << 10) +
		(winterDateTime->day << 5) + winterDateTime->hour;
	uint16_t summer = (summerDateTime->month << 10) +
		(summerDateTime->day << 5) + summerDateTime->hour;
	
	if(curr < summer) return 0;
	if(curr < winter) return 1;
	return 0;
}

static void CorrectDateTime_Hour(struct DateTime* dateTime, int8_t deltaHour)
{
	int8_t tmpHour = dateTime->hour += deltaHour;
	
	/* Correction */
	if(tmpHour < 0)
	{
		tmpHour += 24;
		if(dateTime->dayOfWeek == 0) dateTime->dayOfWeek = 6;
		else dateTime->dayOfWeek--;
		dateTime->day--;
		if(dateTime->day == 0)
		{
			dateTime->month--;
			if(dateTime->month == 0)
			{
				dateTime->month = DECEMBER;
				if(dateTime->year == 0) dateTime->year = 99;
				else dateTime->year--;
			}
			dateTime->day = GetDaysInMonth(dateTime->month, dateTime->year);
		}
	}
	else if(tmpHour >= 24)
	{
		tmpHour -= 24;
		dateTime->dayOfWeek++;
		if(dateTime->dayOfWeek >= 7) dateTime->dayOfWeek = 0;
		dateTime->day++;
		if(dateTime->day > GetDaysInMonth(dateTime->month, dateTime->year))
		{
			dateTime->day = 1;
			dateTime->month++;
			if(dateTime->month > DECEMBER)
			{
				dateTime->month = JANUARY;
				dateTime->year++;
				if(dateTime->year > 99) dateTime->year = 0;
			}
		}
	}
	dateTime->hour = (uint8_t)tmpHour;
}
