#ifndef _HTML_TXT_FUNCS_H_
#define _HTML_TXT_FUNCS_H_

/* Includes ----------------------------------------------------------------- */
/* Standard includes */
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"

/* FreeRTOS+TCP includes */
#include "FreeRTOS_Sockets.h"

/* FreeRTOS Protocol includes */
#include "FreeRTOS_TCP_server.h"
#include "FreeRTOS_server_private.h"

/* Public constants --------------------------------------------------------- */
#ifndef YEAR0
#	define YEAR0 				1900 // the first year
#endif /* YEAR0 */

/* Public function prototypes ----------------------------------------------- */
void SendCheckBox(HTTPClient_t *pxClient, bool disabled, bool readonly,
		char* name, uint16_t nameLength, bool checked);
void SendInput(HTTPClient_t *pxClient, bool disabled, bool readonly,
		char* name, uint16_t nameLength,
		char* value, uint16_t valueLength,
		uint16_t size);

uint16_t GetSizeOfStr(const char* str, uint16_t size);

bool ValueCmp(const char* str, const char* cmp_str);
void GetValue(const char* srcStr, char* dstStr, uint16_t size);
/* Return end of destination string */
char* SetValue(const char* srcStr, char* dstStr, uint16_t size);

bool GetNumFromStr(const char** str, int32_t* num, bool trim);
/* Return end of destination string */
char* SetNumToStr(int32_t num, char* str, uint16_t size);
bool GetFloatFromStr(const char** str, float* fNum, bool trim);
/* Return end of destination string */
char* SetFloatToStr(float num,
		uint16_t numDigitsBeforeP, uint16_t numDigitsAfterP,
		char* str, uint16_t size);

bool GetHexFromStr(const char** str, int32_t* num, bool trim);
/* Return end of destination string */
char* SetHexToStr(uint32_t hex, char* str, uint16_t size, bool alignToLeft);

/* Return end of destination string */
char* SetDateTimeToStr(struct tm* dateTime, char* str, uint16_t size);
/* Return end of destination string */
char* SetTimeToStr(struct tm* dateTime, char* str, uint16_t size);
/* Return end of destination string */
char* SetDateToStr(struct tm* dateTime, char* str, uint16_t size);
/* Return end of destination string */
char* SetTimeIntervalToStr(uint32_t counter, char* str, uint16_t size);

bool GetValueFromTxtField(const char** str,
		char* dstStr, uint16_t size, bool trim);
bool GetNumFromTxtField(const char** str, int32_t* num, bool trim);
void RemoveForbiddenSymbols(char* str, char* forbStr);

bool QueryCmp(const char** str, const char* cmp_str);
bool ParamIsEqu(const char** str, const char* cmp_str);
bool SearchForStartParameter(const char** str);
bool SearchForNextParameter(const char** str);
uint32_t PowBase10(uint32_t power);

#endif /* _HTML_TXT_FUNCS_H_ */
