/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _RTC_DRIVER_H_
#define _RTC_DRIVER_H_

/* Includes ------------------------------------------------------------------*/
/* Standard includes */
#include <stdint.h>
#include <stdbool.h>

/* Application includes */
#include "rtc.h"

/* Public functions prototypes -----------------------------------------------*/
/* Init and task driver functions */
void RTC_DriverInit();
void RTC_DriverTimeCriticalTask();
bool RTC_DriverGetHW_Ok();

/* RTC functions */
void RTC_DriverGetDateTime(struct DateTime* dateTime, uint16_t* ticks);
void RTC_DriverSetDateTime(struct DateTime* dateTime, uint16_t ticks);

bool RTC_GetBKP_GetSetDataFncPtr(
		uint32_t (**pGetDataFnc)(), void (**pSetDataFnc)(uint32_t));

/* Service settings functions */
/* RTC correction functions for PPM (do not use them for settings manager,
   because of float to integer conversion errors) */
int16_t RTC_DriverGetCorrectionPPM();
void RTC_DriverSetCorrectionPPM(int16_t val);

/* RTC correction functions for settings manager */
void RTC_DriverGetCorrection(uint8_t* addedPulses, uint32_t* pulsesValue);
void RTC_DriverSetCorrection(uint8_t addedPulses, uint32_t pulsesValue);


/* Functions that allow to be overridden */
void RTC_DriverPerSecondEvent();

#endif /* _RTC_DRIVER_H_ */
