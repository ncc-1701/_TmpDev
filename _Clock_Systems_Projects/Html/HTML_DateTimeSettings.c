/* Includes ----------------------------------------------------------------- */
#include "settings.h"
#include "web-server.h"
#include "html_txt_funcs.h"
#include "HTML_Header.h"
#include "HTML_DateTimeSettings.h"

/* Application includes */
#include "rtc.h"

/* Private constants -------------------------------------------------------- */
#define HTML_DT_SET_TMP_BUF_LEN 	16

/* Structures definitions --------------------------------------------------- */
struct __attribute__ ((__packed__)) HTML_DateTimeSettings
{
	struct DateTime currDateTime;
	int8_t GMT;
	bool DST;
};

/* Variables ---------------------------------------------------------------- */
/* Struct for settings operations */
static struct HTML_DateTimeSettings settings;

/* Private function prototypes ---------------------------------------------- */
static BaseType_t Send_HTML(HTTPClient_t *pxClient);

/* Public functions --------------------------------------------------------- */
BaseType_t Parse_HTML_DateTimeSettings(HTTPClient_t *pxClient)
{
	const char* buf = pxClient->pcUrlData;
	if(QueryCmp(&buf, "/HTML_DateTimeSettings.html") == pdFALSE) return pdFALSE;

	/* Search for "?" - start of parameters */
	bool apply = pdFALSE;
	if(SearchForStartParameter(&buf))
	{
		// Create temporarily variables
		int32_t tmp32;

		/* Get date and time settings (used as preinit actions),
		   but reset all check-boxes, if they are existing */
		settings.DST = false;
		taskENTER_CRITICAL();
		{
			RTC_GetLocalDateTime(&settings.currDateTime);
			settings.GMT = RTC_GetGMT();
		}
		taskEXIT_CRITICAL();

		while(pdTRUE)
		{
			/* Parse parameters */
			/* Hours */
			if(ParamIsEqu(&buf, "hrs"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if((tmp32 >= 0) && (tmp32 <= 0xFF))
						settings.currDateTime.hour = (uint8_t)tmp32;
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Minutes */
			if(ParamIsEqu(&buf, "mnts"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if((tmp32 >= 0) && (tmp32 <= 0xFF))
						settings.currDateTime.minute = (uint8_t)tmp32;
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Seconds */
			if(ParamIsEqu(&buf, "scnds"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if((tmp32 >= 0) && (tmp32 <= 0xFF))
						settings.currDateTime.second = (uint8_t)tmp32;
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Days */
			if(ParamIsEqu(&buf, "ds"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if((tmp32 >= 0) && (tmp32 <= 0xFF))
						settings.currDateTime.day = (uint8_t)tmp32;
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Months */
			if(ParamIsEqu(&buf, "mnths"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if((tmp32 >= 0) && (tmp32 <= 0xFF))
						settings.currDateTime.month = (uint8_t)tmp32;
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Years */
			if(ParamIsEqu(&buf, "yrs"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if((tmp32 >= 0) && (tmp32 <= 0xFFFF))
						settings.currDateTime.year = (uint16_t)tmp32;
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* GMT */
			if(ParamIsEqu(&buf, "gmt"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if((tmp32 >= (-127)) && (tmp32 <= 128))
						settings.GMT = (int8_t)tmp32;
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* DST flag */
			if(ParamIsEqu(&buf, "dst"))
			{
				if(ValueCmp(buf, "on")) settings.DST = true;

				// Watch for end of parameters
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Apply */
			if(ParamIsEqu(&buf, "b_apl"))
			{
				if(ValueCmp(buf, "apl_st")) apply = pdTRUE;
			}

			/* End of parsing */
			break;
		}
	}

	if(apply)
	{
		taskENTER_CRITICAL();
		{
			/* Story date and time settings */
			VerifyDateTime(&settings.currDateTime);
			RTC_SetLocalDateTime(&settings.currDateTime);
			RTC_SetGMT(settings.GMT);
			RTC_SetDST(settings.DST);
		}
		taskEXIT_CRITICAL();
	}

	/* Send html to client */
	return Send_HTML(pxClient);
}

// Private functions -----------------------------------------------------------
static char* TwoDigitsStr(uint8_t num)
{
	static char twDigs[2];
	twDigs[0] = (num/10) + '0';
	num -= 10*(num/10);
	twDigs[1] = num + '0';
	return twDigs;
}
static BaseType_t Send_HTML(HTTPClient_t *pxClient)
{
	/* Parsed html */
	static const char str_begin[] = "\
    Date and time settings:<br>\r\
  </font>\r\
<pre>\r";
	static const char str_end[] = "\r\r\
<button name=\"b_apl\" type=\"submit\" value=\"apl_st\">\
Apply settings</button>\
</pre>";

	/* Attempt to create the temporary string buffer */
	char* tmpStr = (char*)pvPortMalloc(HTML_DT_SET_TMP_BUF_LEN);
	if(tmpStr == NULL)
	{
		FreeRTOS_printf(("Could not create temporary string buffer\n"));
		return -1;
	}
	
	/* Get date and time settings */
	taskENTER_CRITICAL();
	{
		RTC_GetLocalDateTime(&settings.currDateTime);
		settings.GMT = RTC_GetGMT();
		settings.DST = RTC_GetDST();
	}
	taskEXIT_CRITICAL();

	/* Send html page part by part */
	/* Send header and begin of html*/
	Send_HTML_Header(pxClient, "HTML_DateTimeSettings", NULL, 0);
	SendHTML_Block(pxClient, str_begin, sizeof(str_begin) - 1);

	/* Send current local time and date */
	static const char str_set_time_b[] = "\
Set time (HH:MM:SS):   ";
	SendHTML_Block(pxClient, str_set_time_b,
			sizeof(str_set_time_b) - 1);
	/* Hours input */
	SendInput(pxClient, false, false,
		"hrs", sizeof("hrs") - 1,
		TwoDigitsStr(settings.currDateTime.hour), 2,
		2);
	SendHTML_Block(pxClient, ":", sizeof(":") - 1);
	/* Minutes input */
	SendInput(pxClient, false, false,
		"mnts", sizeof("mnts") - 1,
		TwoDigitsStr(settings.currDateTime.minute), 2,
		2);
	SendHTML_Block(pxClient, ":", sizeof(":") - 1);
	/* Seconds input */
	SendInput(pxClient, false, false,
		"scnds", sizeof("scnds") - 1,
		TwoDigitsStr(settings.currDateTime.second), 2,
		2);

	static const char str_set_date_b[] = "\r\r\
Set date (DD.MM.YYYY): ";
	SendHTML_Block(pxClient, str_set_date_b,
			sizeof(str_set_date_b) - 1);
	/* Days input */
	SendInput(pxClient, false, false,
		"ds", sizeof("ds") - 1,
		TwoDigitsStr(settings.currDateTime.day), 2,
		2);
	SendHTML_Block(pxClient, ".", sizeof(".") - 1);
	/* Months input */
	SendInput(pxClient, false, false,
		"mnths", sizeof("mnths") - 1,
		TwoDigitsStr(settings.currDateTime.month), 2,
		2);
	SendHTML_Block(pxClient, ".", sizeof(".") - 1);
	/* Years input */
	SetNumToStr(settings.currDateTime.year, tmpStr, HTML_DT_SET_TMP_BUF_LEN);
	SendInput(pxClient, false, false,
		"yrs", sizeof("yrs") - 1,
		tmpStr, GetSizeOfStr(tmpStr, HTML_DT_SET_TMP_BUF_LEN),
		4);
	/* Send day of week for checking date */
	SendHTML_Block(pxClient, " ", sizeof(" ") - 1);
	switch(settings.currDateTime.dayOfWeek)
	{
	case MONDAY:
		SendHTML_Block(pxClient, "Monday", sizeof("Monday") - 1);
		break;
	case TUESDAY:
		SendHTML_Block(pxClient, "Tuesday", sizeof("Tuesday") - 1);
		break;
	case WEDNESDAY:
		SendHTML_Block(pxClient, "Wednesday",
				sizeof("Wednesday") - 1);
		break;
	case THURSDAY:
		SendHTML_Block(pxClient, "Thursday", sizeof("Thursday") - 1);
		break;
	case FRIDAY:
		SendHTML_Block(pxClient, "Friday", sizeof("Friday") - 1);
		break;
	case SATURDAY:
		SendHTML_Block(pxClient, "Saturday", sizeof("Saturday") - 1);
		break;
	case SUNDAY:
		SendHTML_Block(pxClient, "Sunday", sizeof("Sunday") - 1);
		break;
	}

	/* Send GMT */
	static const char str_set_gmt_b[] = "\r\r\
Set GMT and DST flag:  ";
	SendHTML_Block(pxClient, str_set_gmt_b,
			sizeof(str_set_gmt_b) - 1);
	/* GMT input */
	SetNumToStr(settings.GMT, tmpStr, HTML_DT_SET_TMP_BUF_LEN);
	SendInput(pxClient, false, false,
		"gmt", sizeof("gmt") - 1,
		tmpStr, GetSizeOfStr(tmpStr, HTML_DT_SET_TMP_BUF_LEN),
		2);

	/* Send DST flag */
	SendCheckBox(pxClient, false, false, "dst",
			sizeof("dst") - 1, settings.DST);

	/* Free temporary string buffer */
	vPortFree(tmpStr);
	
	/* Send end of html page */
	SendHTML_Block(pxClient, str_end, sizeof(str_end) - 1);
	return Send_HTML_End(pxClient);
}
