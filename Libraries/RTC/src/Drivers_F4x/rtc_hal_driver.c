/* It is driver  for RTC calendar and calibration for STM32F4X families MCU */

/* Includes ------------------------------------------------------------------*/
/* Standard includes */
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* Hardware includes */
#include "stm32f4xx_hal.h"

/* Drivers includes */
#include "printf_uart_driver.h"
#include "rtc_driver.h"

/* Application includes */
#include "rtc.h"

/* Constants -----------------------------------------------------------------*/
#ifndef RTC_CLOCK_DELAY_PER_SEC_IT
	#define RTC_CLOCK_DELAY_PER_SEC_IT 	0
#endif /* RTC_CLOCK_DELAY_PER_SEC_IT */

/* Settings check */
#if (RTC_CLOCK_DELAY_PER_SEC_IT > 999)
	#error "Max value of RTC_CLOCK_DELAY_PER_SEC_IT is 999"
#endif /* (RTC_CLOCK_DELAY_PER_SEC_IT > 999) */

/* Private constants */
/* Start year for UNIX systems */
#ifndef FIRSTYEAR
	#define FIRSTYEAR   					1970
#endif /* FIRSTYEAR */

/* Hardware constants */
#define RTC_ASYNCH_PREDIV  			0x0F//0x7F
#define RTC_SYNCH_PREDIV   			0x07FF//0x00FF

/* BKP registers functions constants -----------------------------------------*/
#define MAX_BKP_GET_SET_DATA_FNC_NUM 10
#define FIRST_BKP_GET_SET_DATA_FNC_NUM 10

/* Constants for managing back-up access */
enum RTC_BkUpAccess
{
	RTC_BK_UP_ACCESS_GET_SET_DATA = 0,
	RTC_BK_UP_ACCESS_RTC_CONF = MAX_BKP_GET_SET_DATA_FNC_NUM,
	RTC_BK_UP_ACCESS_ALARM_CONF,
	RTC_BK_UP_ACCESS_RTC_CORRECT,
	RTC_BK_UP_ACCESS_RTC_SET_DATE_TIME,
	RTC_BK_UP_ACCESS_ALARM_IT,
};

/* Debug options -------------------------------------------------------------*/
//#define DEBUG_RTC_ALWAYS_REINIT_RTC
//#define DEBUG_RTC_SET_TIME
//#define DEBUG_RTC_GET_TIME
//#define DEBUG_RTC_GET_PER_SEC_TIME
//#define DEBUG_RTC_NEW_TIME
//#define DEBUG_RTC_SUBSECONDS
//#define DEBUG_RTC_IT_SUBSECONDS
//#define DEBUG_RTC_MSEC

/* Private variables ---------------------------------------------------------*/
/* RTC handler declaration */
static RTC_HandleTypeDef hRTC;

/* Variables for RTC HW state */
static volatile HAL_StatusTypeDef RTC_HW_State;

/* Variables for managing back-up access */
static volatile uint16_t bkUpAccessFlags;

/* Structure member for last time after NewTimeEvent */
static struct DateTime lastDateTime;

#if defined(DEBUG_RTC_SUBSECONDS) || defined(DEBUG_RTC_IT_SUBSECONDS)
static uint32_t volatile subSeconds = 0;
#endif /*defined(DEBUG_RTC_SUBSECONDS) || defined(DEBUG_RTC_IT_SUBSECONDS)*/

/* External function prototypes --------------------------------------------- */
#if defined(DEBUG_RTC_SET_TIME) || defined(DEBUG_RTC_GET_TIME) ||\
	defined(DEBUG_RTC_GET_PER_SEC_TIME) || defined(DEBUG_RTC_NEW_TIME) ||\
	defined(DEBUG_RTC_SUBSECONDS) || defined(DEBUG_RTC_IT_SUBSECONDS) ||\
	defined(DEBUG_RTC_MSEC)
extern void PrintfUART_DriverSendChar(char sendingChar);
#endif

/* Private function prototypes -----------------------------------------------*/
static int16_t GetCorrectionPPM(uint8_t addedPulses, uint32_t pulsesValue);
static void RTC_Configuration();
static void Error_Handler();
static bool DateTimeIsEqualToLast(struct DateTime* dateTime);
static bool CheckForNewDateTimeEvent(struct DateTime* dateTime);
static bool PerSecUpdateTimeEvent();

/* BKP registers functions prototypes ----------------------------------------*/
static uint32_t BKP10_Get_U32();
static void BKP10_Set_U32(uint32_t val);
static uint32_t BKP11_Get_U32();
static void BKP11_Set_U32(uint32_t val);
static uint32_t BKP12_Get_U32();
static void BKP12_Set_U32(uint32_t val);
static uint32_t BKP13_Get_U32();
static void BKP13_Set_U32(uint32_t val);
static uint32_t BKP14_Get_U32();
static void BKP14_Set_U32(uint32_t val);
static uint32_t BKP15_Get_U32();
static void BKP15_Set_U32(uint32_t val);
static uint32_t BKP16_Get_U32();
static void BKP16_Set_U32(uint32_t val);
static uint32_t BKP17_Get_U32();
static void BKP17_Set_U32(uint32_t val);
static uint32_t BKP18_Get_U32();
static void BKP18_Set_U32(uint32_t val);
static uint32_t BKP19_Get_U32();
static void BKP19_Set_U32(uint32_t val);

/* Public functions ----------------------------------------------------------*/


/* BKP registers functions prototypes ----------------------------------------*/
static uint32_t BKP_Get_U32(uint8_t numBKP)
{
	/* Validate the number of BKP register */
	if(numBKP < FIRST_BKP_GET_SET_DATA_FNC_NUM) return 0;
	if(numBKP >= FIRST_BKP_GET_SET_DATA_FNC_NUM + MAX_BKP_GET_SET_DATA_FNC_NUM)
		return 0;

	/* Correct for usability the number of BKP register */
	numBKP -= FIRST_BKP_GET_SET_DATA_FNC_NUM;
	switch(numBKP)
	{
	case 0:
		return HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR10);
	case 1:
		return HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR11);
	case 2:
		return HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR12);
	case 3:
		return HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR13);
	case 4:
		return HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR14);
	case 5:
		return HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR15);
	case 6:
		return HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR16);
	case 7:
		return HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR17);
	case 8:
		return HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR18);
	case 9:
		return HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR19);
	}
	return 0;
}
static void BKP_Set_U32(uint8_t numBKP, uint32_t val)
{
	/* Validate the number of BKP register */
	if(numBKP < FIRST_BKP_GET_SET_DATA_FNC_NUM) return;
	if(numBKP >= FIRST_BKP_GET_SET_DATA_FNC_NUM + MAX_BKP_GET_SET_DATA_FNC_NUM)
		return;

	/* Correct for usability the number of BKP register */
	numBKP -= FIRST_BKP_GET_SET_DATA_FNC_NUM;

	/* Enable BKP write access, but before that
	 * set corresponding back-up access flag */
	bkUpAccessFlags |= (1 << numBKP);
	HAL_PWR_EnableBkUpAccess();

	switch(numBKP)
	{
	case 0:
		HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR10, val);
		break;
	case 1:
		HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR11, val);
		break;
	case 2:
		HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR12, val);
		break;
	case 3:
		HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR13, val);
		break;
	case 4:
		HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR14, val);
		break;
	case 5:
		HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR15, val);
		break;
	case 6:
		HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR16, val);
		break;
	case 7:
		HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR17, val);
		break;
	case 8:
		HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR18, val);
		break;
	case 9:
		HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR19, val);
		break;
	}

	/* Disable BKP write access, but before that
	 * reset corresponding back-up access flag */
	bkUpAccessFlags &= ~(1 << numBKP);
	/* Check back-up access flags */
	if(bkUpAccessFlags == 0) HAL_PWR_DisableBkUpAccess();
}
static uint32_t BKP10_Get_U32()
{
	return BKP_Get_U32(10);
}
static void BKP10_Set_U32(uint32_t val)
{
	BKP_Set_U32(10, val);
}
static uint32_t BKP11_Get_U32()
{
	return BKP_Get_U32(11);
}
static void BKP11_Set_U32(uint32_t val)
{
	BKP_Set_U32(11, val);
}
static uint32_t BKP12_Get_U32()
{
	return BKP_Get_U32(12);
}
static void BKP12_Set_U32(uint32_t val)
{
	BKP_Set_U32(12, val);
}
static uint32_t BKP13_Get_U32()
{
	return BKP_Get_U32(13);
}
static void BKP13_Set_U32(uint32_t val)
{
	BKP_Set_U32(13, val);
}
static uint32_t BKP14_Get_U32()
{
	return BKP_Get_U32(14);
}
static void BKP14_Set_U32(uint32_t val)
{
	BKP_Set_U32(14, val);
}
static uint32_t BKP15_Get_U32()
{
	return BKP_Get_U32(15);
}
static void BKP15_Set_U32(uint32_t val)
{
	BKP_Set_U32(15, val);
}
static uint32_t BKP16_Get_U32()
{
	return BKP_Get_U32(16);
}
static void BKP16_Set_U32(uint32_t val)
{
	BKP_Set_U32(16, val);
}
static uint32_t BKP17_Get_U32()
{
	return BKP_Get_U32(17);
}
static void BKP17_Set_U32(uint32_t val)
{
	BKP_Set_U32(17, val);
}
static uint32_t BKP18_Get_U32()
{
	return BKP_Get_U32(18);
}
static void BKP18_Set_U32(uint32_t val)
{
	BKP_Set_U32(18, val);
}
static uint32_t BKP19_Get_U32()
{
	return BKP_Get_U32(19);
}
static void BKP19_Set_U32(uint32_t val)
{
	BKP_Set_U32(19, val);
}

bool RTC_GetBKP_GetSetDataFncPtr(
		uint32_t (**pGetDataFnc)(), void (**pSetDataFnc)(uint32_t))
{
	static uint8_t BKP_GetSetDataFncPtrQuantity = 0;

	/* Check for BKP_GetSetDataFncPtrQuantity */
	if(BKP_GetSetDataFncPtrQuantity >= MAX_BKP_GET_SET_DATA_FNC_NUM)
		return false;
	switch(BKP_GetSetDataFncPtrQuantity)
	{
	case 0:
		*pGetDataFnc = BKP10_Get_U32;
		*pSetDataFnc = BKP10_Set_U32;
		break;

	case 1:
		*pGetDataFnc = BKP11_Get_U32;
		*pSetDataFnc = BKP11_Set_U32;
		break;

	case 2:
		*pGetDataFnc = BKP12_Get_U32;
		*pSetDataFnc = BKP12_Set_U32;
		break;

	case 3:
		*pGetDataFnc = BKP13_Get_U32;
		*pSetDataFnc = BKP13_Set_U32;
		break;

	case 4:
		*pGetDataFnc = BKP14_Get_U32;
		*pSetDataFnc = BKP14_Set_U32;
		break;

	case 5:
		*pGetDataFnc = BKP15_Get_U32;
		*pSetDataFnc = BKP15_Set_U32;
		break;

	case 6:
		*pGetDataFnc = BKP16_Get_U32;
		*pSetDataFnc = BKP16_Set_U32;
		break;

	case 7:
		*pGetDataFnc = BKP17_Get_U32;
		*pSetDataFnc = BKP17_Set_U32;
		break;

	case 8:
		*pGetDataFnc = BKP18_Get_U32;
		*pSetDataFnc = BKP18_Set_U32;
		break;

	case 9:
		*pGetDataFnc = BKP19_Get_U32;
		*pSetDataFnc = BKP19_Set_U32;
		break;
	}

	BKP_GetSetDataFncPtrQuantity++;
	return true;
}

/* Init and other task functions */
void RTC_DriverInit()
{
	RTC_AlarmTypeDef salarmstructure;

	/* Init internal variables */
	RTC_HW_State = HAL_OK;
	bkUpAccessFlags = 0;

	/* Enable PWR clock */
	__HAL_RCC_PWR_CLK_ENABLE();

	/* Init RTC handle */
	hRTC.Instance = RTC;
	hRTC.Init.HourFormat = RTC_HOURFORMAT_24;
	hRTC.Init.AsynchPrediv = RTC_ASYNCH_PREDIV;
	hRTC.Init.SynchPrediv = RTC_SYNCH_PREDIV;
	hRTC.Init.OutPut = RTC_OUTPUT_DISABLE;
	hRTC.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
	hRTC.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
	__HAL_RTC_RESET_HANDLE_STATE(&hRTC);

	/* Check if Data stored in BackUp register1: no need to reconfigure RTC */
	/* Read the Back Up Register 1 Data */
#ifndef DEBUG_RTC_ALWAYS_REINIT_RTC
	if(HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR1) != 0x32F2)
#endif /*DEBUG_RTC_ALWAYS_REINIT_RTC*/
	{
		RTC_Configuration();
	}

	/* Enable BKP write access, but before that
	 * set corresponding back-up access flag */
	bkUpAccessFlags |= (1 << RTC_BK_UP_ACCESS_ALARM_CONF);
	HAL_PWR_EnableBkUpAccess();

	/* Configure the RTC Alarm peripheral for generation per-second event */
	salarmstructure.Alarm = RTC_ALARM_A;
	salarmstructure.AlarmMask = RTC_ALARMMASK_ALL;
	salarmstructure.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_NONE;
	salarmstructure.AlarmTime.SubSeconds = RTC_SYNCH_PREDIV -
			(((RTC_SYNCH_PREDIV + 1)*RTC_CLOCK_DELAY_PER_SEC_IT)/1000);

	if(HAL_RTC_SetAlarm_IT(&hRTC,&salarmstructure,RTC_FORMAT_BIN) != HAL_OK)
	{
		/* Initialization Error */
		Error_Handler();
	}

	/* Configure the NVIC for RTC Alarm (used for per-second events) */
	HAL_NVIC_SetPriority(RTC_Alarm_IRQn, RTC_TIMI_PRIOR, 0);
	HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);

#ifdef RTC_CLOCK_OUTPUT_ENABLE
	/* Enable calibration output port */
	if(HAL_RTCEx_SetCalibrationOutPut(&hRTC, RTC_CALIBOUTPUT_1HZ) != HAL_OK)
	{
		/* Initialization Error */
		Error_Handler();
	}
#endif /* RTC_CLOCK_OUTPUT_ENABLE */

	/* Disable BKP write access, but before that
	 * reset corresponding back-up access flag */
	bkUpAccessFlags &= ~(1 << RTC_BK_UP_ACCESS_ALARM_CONF);
	/* Check back-up access flags */
	if(bkUpAccessFlags == 0) HAL_PWR_DisableBkUpAccess();
}

void RTC_DriverTimeCriticalTask()
{
#if defined(DEBUG_RTC_SUBSECONDS) || defined(DEBUG_RTC_MSEC)
	static uint16_t startUpDelay = 2000;
	struct DateTime dateTime;
	if(startUpDelay)
	{
		startUpDelay--;
		return;
	}
#endif /*defined(DEBUG_RTC_SUBSECONDS) || defined(DEBUG_RTC_MSEC)*/

#ifdef DEBUG_RTC_SUBSECONDS
	RTC_DriverGetDateTime(&dateTime, NULL);
	PrintfUART_DriverSendChar((uint8_t)(subSeconds >> 8));
	PrintfUART_DriverSendChar((uint8_t)subSeconds);
#endif /*DEBUG_RTC_SUBSECONDS*/

#ifdef DEBUG_RTC_MSEC
	uint16_t ticks;
	RTC_DriverGetDateTime(&dateTime, &ticks);
	PrintfUART_DriverSendChar((uint8_t)(ticks >> 8));
	PrintfUART_DriverSendChar((uint8_t)ticks);
#endif /*DEBUG_RTC_MSEC*/
}

bool RTC_DriverGetHW_Ok()
{
	if(RTC_HW_State == HAL_OK) return true;
	return false;
}

/* Service settings functions */
int16_t RTC_DriverGetCorrectionPPM()
{
	uint8_t addedPulses = 0;
	uint32_t pulsesValue = 0;
	RTC_DriverGetCorrection(&addedPulses, &pulsesValue);
	return GetCorrectionPPM(addedPulses, pulsesValue);
}

void RTC_DriverSetCorrectionPPM(int16_t val)
{
	uint8_t addedPulses = 0;
	uint32_t pulsesValue = 0;

	/* Validate input parameters */
	if(val < (-488)) val = -488;
	else if(val > 487) val = 487;

	/* Get flag of added pulses */
	if(val > 0)
	{
		/* Pulses will be added: frequency will fast */
		addedPulses = 1;
		float dev = (float)val;
		dev = dev/1000000.0;

		pulsesValue = (512 - dev*(1048576 - 512))/(1 + dev);
	}
	else if(val < 0)
	{
		/* Pulses will be masked: frequency will slow */
		float dev = (float)(-val);
		dev = dev/1000000.0;

		pulsesValue = (dev*1048576)/(1 - dev);
	}

	/* Check for already inited RTC */
	if(HAL_RTCEx_BKUPRead(&hRTC, RTC_BKP_DR1) != 0x32F2)
	{
		/* Backup data register value is not correct or not yet programmed
		 * (when the first time the program is executed) */
		RTC_Configuration();
	}

	RTC_DriverSetCorrection(addedPulses, pulsesValue);
}

void RTC_DriverGetCorrection(uint8_t* addedPulses, uint32_t* pulsesValue)
{
	*addedPulses = 0;

	/* Get flag of added pulses */
	if(hRTC.Instance->CALR & RTC_SMOOTHCALIB_PLUSPULSES_SET) *addedPulses = 1;

	/* Get number of pulses */
	*pulsesValue = hRTC.Instance->CALR & 0x000001FFU;
}

void RTC_DriverSetCorrection(uint8_t addedPulses, uint32_t pulsesValue)
{
	/* Compare correction value with current state */
	uint8_t currAddedPulses = 0;
	uint32_t currPulsesValue = 0;
	RTC_DriverGetCorrection(&currAddedPulses, &currPulsesValue);
	if((addedPulses == currAddedPulses) && (pulsesValue == currPulsesValue))
		return;

	/* Enable BKP write access, but before that
	 * set corresponding back-up access flag */
	bkUpAccessFlags |= (1 << RTC_BK_UP_ACCESS_RTC_CORRECT);
	HAL_PWR_EnableBkUpAccess();

	/* Set correction */
	uint32_t plusPulses = RTC_SMOOTHCALIB_PLUSPULSES_RESET;
	if(addedPulses) plusPulses = RTC_SMOOTHCALIB_PLUSPULSES_SET;

	/* Use 32 sec period */
	if(HAL_RTCEx_SetSmoothCalib(
			&hRTC, RTC_SMOOTHCALIB_PERIOD_32SEC, plusPulses, pulsesValue) !=
					HAL_OK)
	{
		Error_Handler();
	}

	/* Disable BKP write access, but before that
	 * reset corresponding back-up access flag */
	bkUpAccessFlags &= ~(1 << RTC_BK_UP_ACCESS_RTC_CORRECT);
	/* Check back-up access flags */
	if(bkUpAccessFlags == 0) HAL_PWR_DisableBkUpAccess();
}


void RTC_DriverGetDateTime(struct DateTime* dateTime, uint16_t* ticks)
{
	dateTime->hour = lastDateTime.hour;
	dateTime->minute = lastDateTime.minute;
	dateTime->second = lastDateTime.second;
	dateTime->year = lastDateTime.year;
	dateTime->month = lastDateTime.month;
	dateTime->day = lastDateTime.day;
	dateTime->dayOfWeek = lastDateTime.dayOfWeek;

	/* Store ticks */
	if(ticks != NULL)
	{
		uint32_t subSeconds = (uint32_t)(hRTC.Instance->SSR);
		*ticks = (uint16_t)(((RTC_SYNCH_PREDIV - subSeconds)*1000)/
				(RTC_SYNCH_PREDIV + 1));
	}

#if defined(DEBUG_RTC_SUBSECONDS) || defined(DEBUG_RTC_IT_SUBSECONDS)
	subSeconds = time.SubSeconds;
#endif /*defined(DEBUG_RTC_SUBSECONDS) || defined(DEBUG_RTC_IT_SUBSECONDS)*/

#ifdef DEBUG_RTC_GET_TIME
	PrintfUART_DriverSendChar('g');
#endif /* DEBUG_RTC_GET_TIME */
}

void RTC_DriverSetDateTime(struct DateTime* dateTime, uint16_t ticks)
{
	static RTC_DateTypeDef date;
	static RTC_TimeTypeDef time;

	/* Enable BKP write access, but before that
	 * set corresponding back-up access flag */
	bkUpAccessFlags |= (1 << RTC_BK_UP_ACCESS_RTC_SET_DATE_TIME);
	HAL_PWR_EnableBkUpAccess();

	/* Set date and time */
	/* Set the time */
	time.Hours = dateTime->hour;
	time.Minutes = dateTime->minute;
	time.Seconds = dateTime->second;
	time.TimeFormat = RTC_HOURFORMAT12_AM;
	time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	time.StoreOperation = RTC_STOREOPERATION_RESET;

	/* Set the date */
	date.Year = dateTime->year%FIRSTYEAR;
	date.Month = dateTime->month;
	date.Date = dateTime->day;

	/* Convert day of week */
	date.WeekDay = dateTime->dayOfWeek + 1;

	/* Set the RTC current Date */
	if(HAL_RTC_SetDate(&hRTC, &date, RTC_FORMAT_BIN) != HAL_OK)
	{
		/* HW Error */
		Error_Handler();
	}

	/* Set the RTC current Time */
	if(HAL_RTC_SetTime(&hRTC, &time, RTC_FORMAT_BIN) != HAL_OK)
	{
		/* HW Error */
		Error_Handler();
	}
	
	/* Validate ticks */
	if(ticks > 999) ticks = 999;
	if(ticks)
	{
		/* RTC is delayed */
		uint32_t shiftSubFS =
				RTC_SYNCH_PREDIV - ((RTC_SYNCH_PREDIV + 1)*ticks)/1000;
		if(HAL_RTCEx_SetSynchroShift(&hRTC, RTC_SHIFTADD1S_SET, shiftSubFS) !=
				HAL_OK)
		{
			/* HW Error */
			Error_Handler();
		}
	}

	/* Disable BKP write access, but before that
	 * reset corresponding back-up access flag */
	bkUpAccessFlags &= ~(1 << RTC_BK_UP_ACCESS_RTC_SET_DATE_TIME);
	/* Check back-up access flags */
	if(bkUpAccessFlags == 0) HAL_PWR_DisableBkUpAccess();

#ifdef DEBUG_RTC_SET_TIME
	PrintfUART_DriverSendChar('s');
#endif /* DEBUG_RTC_SET_TIME */

	/* Check for "NewDateTimeEvent" only if ticks is zero */
	if(ticks == 0) CheckForNewDateTimeEvent(dateTime);
	else
	{
		/* Store current time */
		lastDateTime.second = dateTime->second;
		lastDateTime.minute = dateTime->minute;
		lastDateTime.hour = dateTime->hour;
		lastDateTime.day = dateTime->day;
		lastDateTime.dayOfWeek = dateTime->dayOfWeek;
		lastDateTime.month = dateTime->month;
		lastDateTime.year = dateTime->year;
	}
}

__attribute__((weak)) void RTC_DriverPerSecondEvent() {}

/* Private functions ---------------------------------------------------------*/
static int16_t GetCorrectionPPM(uint8_t addedPulses, uint32_t pulsesValue)
{
	/* Calculate dev = f_cal/f_RTC */
	float dev = ((float)(addedPulses*512) - (float)pulsesValue)/
				(1048576 + pulsesValue - addedPulses*512);

	int16_t val = (int16_t)(roundf(1000000.0*dev));

	/* Validate result */
	if(val < (-488)) val = -488;
	else if(val > 487) val = 487;

	return val;
}

static void RTC_Configuration()
{
	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

	/* Enable BKP write access, but before that
	 * set corresponding back-up access flag */
	bkUpAccessFlags |= (1 << RTC_BK_UP_ACCESS_RTC_CONF);
	HAL_PWR_EnableBkUpAccess();

	/* Configure the RTC peripheral */
	/* Configure LSE as RTC clock source */
	RCC_OscInitStruct.OscillatorType =  RCC_OSCILLATORTYPE_LSE;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}
	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
	PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
	if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/* Enable RTC peripheral Clocks */
	__HAL_RCC_RTC_ENABLE();

	if(HAL_RTC_Init(&hRTC) != HAL_OK)
	{
		/* Initialization Error */
		Error_Handler();
	}

	/* Writes a data in a RTC Backup data Register1 */
	HAL_RTCEx_BKUPWrite(&hRTC, RTC_BKP_DR1, 0x32F2);

	/* Reset another BKP registers */
	for(uint8_t i = 0; i < MAX_BKP_GET_SET_DATA_FNC_NUM; i++)
	{
		BKP_Set_U32(i + FIRST_BKP_GET_SET_DATA_FNC_NUM, 0);
	}

	/* Disable BKP write access, but before that
	 * reset corresponding back-up access flag */
	bkUpAccessFlags &= ~(1 << RTC_BK_UP_ACCESS_RTC_CONF);
	/* Check back-up access flags */
	if(bkUpAccessFlags == 0) HAL_PWR_DisableBkUpAccess();

	/* Reset date and time */
	struct DateTime dateTime;
	dateTime.year = 0;
	dateTime.month = 1;
	dateTime.day = 1;
	dateTime.dayOfWeek = RTC_WEEKDAY_SATURDAY;

	dateTime.hour = 0;
	dateTime.minute = 0;
	dateTime.second = 0;

	RTC_DriverSetDateTime(&dateTime, 0);
}

static void Error_Handler()
{
	/* Set RTC HW state */
	RTC_HW_State = HAL_ERROR;
}

static bool DateTimeIsEqualToLast(struct DateTime* dateTime)
{
	/* Compare entry dateTime with lastDateTime
	   (field by field, except day of week) */
	if(lastDateTime.second != dateTime->second) return false;
	if(lastDateTime.minute != dateTime->minute) return false;
	if(lastDateTime.hour != dateTime->hour) return false;
	if(lastDateTime.day != dateTime->day) return false;
	if(lastDateTime.month != dateTime->month) return false;
	if(lastDateTime.year != dateTime->year) return false;
	return true;
}

static bool CheckForNewDateTimeEvent(struct DateTime* dateTime)
{
	static volatile bool entrLock = false;
	//static struct DateTime lastDateTime;
	if(entrLock) return false;
	entrLock = true;

	/* Compare entry dateTime with lastDateTime */
	if(DateTimeIsEqualToLast(dateTime) == false)
	{
		/* Store changed time */
		lastDateTime.second = dateTime->second;
		lastDateTime.minute = dateTime->minute;
		lastDateTime.hour = dateTime->hour;
		lastDateTime.day = dateTime->day;
		lastDateTime.dayOfWeek = dateTime->dayOfWeek;
		lastDateTime.month = dateTime->month;
		lastDateTime.year = dateTime->year;

		RTC_DriverPerSecondEvent();

#ifdef DEBUG_RTC_NEW_TIME
		PrintfUART_DriverSendChar('n');
#endif /* DEBUG_RTC_NEW_TIME */

		entrLock = false;
		return true;
	}

#ifdef DEBUG_RTC_NEW_TIME
	PrintfUART_DriverSendChar('e');
#endif /* DEBUG_RTC_NEW_TIME */

	entrLock = false;
	return false;
}

static bool PerSecUpdateTimeEvent()
{
	/* Get new time */
	struct DateTime dateTime;
	//RTC_DriverGetDateTime(&dateTime, NULL);

	static RTC_TimeTypeDef time;
	static RTC_DateTypeDef date;

	/* Get the RTC current Time */
	HAL_RTC_GetTime(&hRTC, &time, RTC_FORMAT_BIN);
	/* Get the RTC current Date */
	HAL_RTC_GetDate(&hRTC, &date, RTC_FORMAT_BIN);

	/* Store date and time */
	/* Store time */
	dateTime.hour = time.Hours;
	dateTime.minute = time.Minutes;
	dateTime.second = time.Seconds;

	/* Store date */
	dateTime.year = date.Year + FIRSTYEAR;
	dateTime.month = date.Month;
	dateTime.day = date.Date;

	/* Convert day of week */
	dateTime.dayOfWeek = date.WeekDay - 1;

	/* Check for "NewDateTimeEvent" */
	return CheckForNewDateTimeEvent(&dateTime);
}

/* Low level functions -------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif
void RTC_Alarm_IRQHandler(void)
{
	/* Enable BKP write access, but before that
	 * set corresponding back-up access flag */
	bkUpAccessFlags |= (1 << RTC_BK_UP_ACCESS_ALARM_IT);
	HAL_PWR_EnableBkUpAccess();

	HAL_RTC_AlarmIRQHandler(&hRTC);

	/* Disable BKP write access, but before that
	 * reset corresponding back-up access flag */
	bkUpAccessFlags &= ~(1 << RTC_BK_UP_ACCESS_ALARM_IT);
	/* Check back-up access flags */
	if(bkUpAccessFlags == 0) HAL_PWR_DisableBkUpAccess();

	/* Generate PerSec event */
#ifdef DEBUG_RTC_GET_PER_SEC_TIME
	PrintfUART_DriverSendChar('G');
#endif /* DEBUG_RTC_GET_PER_SEC_TIME */

#ifndef DEBUG_RTC_IT_SUBSECONDS
	PerSecUpdateTimeEvent();
#else /*DEBUG_RTC_IT_SUBSECONDS*/
	if(PerSecUpdateTimeEvent()) PrintfUART_DriverSendChar('I');
	else PrintfUART_DriverSendChar('i');
	PrintfUART_DriverSendChar((uint8_t)(subSeconds >> 8));
	PrintfUART_DriverSendChar((uint8_t)subSeconds);
#endif /*DEBUG_RTC_IT_SUBSECONDS*/
}
#ifdef __cplusplus
}
#endif
