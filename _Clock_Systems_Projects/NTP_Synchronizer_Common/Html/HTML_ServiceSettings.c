/* Includes ----------------------------------------------------------------- */
#include "settings.h"
#include "ui.h"
#include "UDP_logging.h"
#include "html_txt_funcs.h"
#include "HTML_Header.h"
#include "HTML_ServiceSettings.h"

/* Application includes */
#include "rtc_driver.h"

/* Private constants -------------------------------------------------------- */
#define HTML_SRVC_SET_TMP_BUF_LEN 	16

/* Structures definitions --------------------------------------------------- */
struct __attribute__ ((__packed__)) HTML_ServiceSettings
{
	/* RTC correction settings */
	int16_t RTC_CorrectionPPM;

	/* Logging settings */
	bool loggingEnable;
	uint32_t loggingIP_Addr;
	uint16_t loggingPort;
	bool logEvents;
	bool logWarnings;
	bool logErrors;
};

/* Variables ---------------------------------------------------------------- */


/* Structure for settings operations */
static struct HTML_ServiceSettings settings;

/* Private function prototypes ---------------------------------------------- */
static BaseType_t Send_HTML(HTTPClient_t *pxClient);

/* Public functions --------------------------------------------------------- */
BaseType_t Parse_HTML_ServiceSettings(HTTPClient_t *pxClient)
{
	const char* buf = pxClient->pcUrlData;
	if(QueryCmp(&buf, "/HTML_ServiceSettings.html") == pdFALSE) return pdFALSE;

	/* Search for "?" - start of parameters */
	bool apply = false;
	if(SearchForStartParameter(&buf))
	{
		/* Create temporarily variables */
		char tmpValStr[32];
		int32_t tmp32;

		/* Get selector settings (used as preinit actions),
		   but reset all check-boxes, if they are existing */
		/* Logging settings */
		settings.loggingEnable = false;
		settings.logEvents = false;
		settings.logWarnings = false;
		settings.logErrors = false;

		taskENTER_CRITICAL();
		{
			/* RTC correction settings */
			settings.RTC_CorrectionPPM = RTC_DriverGetCorrectionPPM();

			/* Logging settings */
			settings.loggingIP_Addr = GetUDP_LoggingIP_Addr();
			settings.loggingPort = GetUDP_LoggingPort();
		}
		taskEXIT_CRITICAL();

		while(pdTRUE)
		{
			/* Parse parameters */
			/* RTC correction */
			if(ParamIsEqu(&buf, "rtc_cr"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if((tmp32 >= SHRT_MIN) && (tmp32 <= SHRT_MAX))
						settings.RTC_CorrectionPPM = (int16_t)tmp32;
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Logging settings configure --------------------------------------------*/
			/* UDP-logging global enable flag */
			if(ParamIsEqu(&buf, "enLog"))
			{
				if(ValueCmp(buf, "on"))
					settings.loggingEnable = true;

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* UDP-logging host settings */
			/* UDP-logging host IP-address */
			if(ParamIsEqu(&buf, "logIP"))
			{
				/* Get value as string */
				GetValue(buf, tmpValStr, sizeof(tmpValStr));

				/* Try to convert string to number */
				settings.loggingIP_Addr =
					FreeRTOS_inet_addr(tmpValStr);

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}
			/* UDP-logging host port */
			if(ParamIsEqu(&buf, "logPrt"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if((tmp32 >= 0) && (tmp32 <= 0xFFFF))
						settings.loggingPort = FreeRTOS_htons(tmp32);
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}
			/* UDP-logging cfg messages */
			/* Log events flag */
			if(ParamIsEqu(&buf, "enLogEvt"))
			{
				if(ValueCmp(buf, "on"))
					settings.logEvents = true;

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}
			/* Log warnings flag */
			if(ParamIsEqu(&buf, "enLogWrn"))
			{
				if(ValueCmp(buf, "on"))
					settings.logWarnings = true;

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}
			/* Log errors flag */
			if(ParamIsEqu(&buf, "enLogErr"))
			{
				if(ValueCmp(buf, "on"))
					settings.logErrors = true;

				/* Watch for end of parameters */
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
			/* RTC correction settings */
			RTC_DriverSetCorrectionPPM(settings.RTC_CorrectionPPM);

			/* Logging settings */
			SetUDP_LoggingEnable(settings.loggingEnable);
			SetUDP_LoggingIP_Addr(settings.loggingIP_Addr);
			SetUDP_LoggingPort(settings.loggingPort);
			SetUDP_LogEvents(settings.logEvents);
			SetUDP_LogWarnings(settings.logWarnings);
			SetUDP_LogErrors(settings.logErrors);
		}
		taskEXIT_CRITICAL();
	}

	/* Send html to client */
	return Send_HTML(pxClient);
}

/* Private functions -------------------------------------------------------- */
static BaseType_t Send_HTML(HTTPClient_t *pxClient)
{
	/* Parsed html */
	static const char str_begin[] = "\
    Service settings:<br>\r\
  </font>\r\
<pre>\r";
	static const char str_end[] = "\r\r\
<button name=\"b_apl\" type=\"submit\" value=\"apl_st\">\
Apply settings</button>\
</pre>";

	/* Attempt to create the temporary string buffer. */
	char* tmpStr = (char*)pvPortMalloc(HTML_SRVC_SET_TMP_BUF_LEN);
	if(tmpStr == NULL)
	{
		FreeRTOS_printf(("Could not create temporary string buffer\n"));
		return -1;
	}
	
	/* Get app settings */
	taskENTER_CRITICAL();
	{
		/* RTC correction settings */
		settings.RTC_CorrectionPPM = RTC_DriverGetCorrectionPPM();

		/* Logging settings */
		settings.loggingEnable = GetUDP_LoggingEnable();
		settings.loggingIP_Addr = GetUDP_LoggingIP_Addr();
		settings.loggingPort = GetUDP_LoggingPort();
		settings.logEvents = GetUDP_LogEvents();
		settings.logWarnings = GetUDP_LogWarnings();
		settings.logErrors = GetUDP_LogErrors();
	}
	taskEXIT_CRITICAL();

	/* Send html page part by part */
	/* Send header and begin of html*/
	Send_HTML_Header(pxClient, "HTML_ServiceSettings", NULL, 0);
	SendHTML_Block(pxClient, str_begin, sizeof(str_begin) - 1);

	/* Send RTC correction */
	static const char str_rtc_cr_b[] = "</pre>\
RTC settings:<pre>\r\
Set RTC correction in PPM    ";
	SendHTML_Block(pxClient, str_rtc_cr_b,
			sizeof(str_rtc_cr_b) - 1);
	SetNumToStr(settings.RTC_CorrectionPPM,
			tmpStr, HTML_SRVC_SET_TMP_BUF_LEN);
	SendInput(pxClient, false, false,
			"rtc_cr", sizeof("rtc_cr") - 1,
			tmpStr, GetSizeOfStr(tmpStr, HTML_SRVC_SET_TMP_BUF_LEN), 2);

	/* Logging settings configure --------------------------------------------*/
	static const char str_log_cfg_b[] = "\r</pre>\
Logging settings configure:<pre>\r\
Enable logging     ";
	/* UDP-logging global enable flag */
	SendHTML_Block(pxClient, str_log_cfg_b,
			sizeof(str_log_cfg_b) - 1);
	SendCheckBox(pxClient, false, false,
			"enLog", sizeof("enLog") - 1,
			settings.loggingEnable);

	/* UDP-logging host settings */
	static const char str_udp_log_cfg_b[] = "\r\r\
UDP-logging host settings:\r\
logging IP address ";
	/* UDP-logging host IP-address */
	SendHTML_Block(pxClient, str_udp_log_cfg_b,
			sizeof(str_udp_log_cfg_b) - 1);
	memset(tmpStr, 0x00, HTML_SRVC_SET_TMP_BUF_LEN);
	FreeRTOS_inet_ntoa(settings.loggingIP_Addr, tmpStr);
	SendInput(pxClient, false, false,
			"logIP", sizeof("logIP") - 1,
			tmpStr, GetSizeOfStr(tmpStr, HTML_SRVC_SET_TMP_BUF_LEN), 8);
	/* UDP-logging host port */
	static const char str_udp_log_cfg_1[] = "\r\
logging port       ";
	SendHTML_Block(pxClient, str_udp_log_cfg_1,
			sizeof(str_udp_log_cfg_1) - 1);
	SetNumToStr(FreeRTOS_htons(settings.loggingPort),
			tmpStr, HTML_SRVC_SET_TMP_BUF_LEN);
	SendInput(pxClient, false, false,
			"logPrt", sizeof("logPrt") - 1,
			tmpStr, GetSizeOfStr(tmpStr, HTML_SRVC_SET_TMP_BUF_LEN), 4);

	/* UDP-logging cfg messages */
	static const char str_udp_log_msg_cfg_b[] = "\r\r\
UDP-logging messages configure:\r\
Log events         ";
	/* Send log events flag */
	SendHTML_Block(pxClient, str_udp_log_msg_cfg_b,
			sizeof(str_udp_log_msg_cfg_b) - 1);
	SendCheckBox(pxClient, false, false,
			"enLogEvt", sizeof("enLogEvt") - 1,
			settings.logEvents);
	/* Send log warnings flag */
	static const char str_udp_log_msg_cfg_1[] = "\r\
Log warnings       ";
	SendHTML_Block(pxClient, str_udp_log_msg_cfg_1,
			sizeof(str_udp_log_msg_cfg_1) - 1);
	SendCheckBox(pxClient, false, false,
			"enLogWrn", sizeof("enLogWrn") - 1,
			settings.logWarnings);
	/* Send log errors flag */
	static const char str_udp_log_msg_cfg_2[] = "\r\
Log errors         ";
	SendHTML_Block(pxClient, str_udp_log_msg_cfg_2,
			sizeof(str_udp_log_msg_cfg_2) - 1);
	SendCheckBox(pxClient, false, false,
			"enLogErr", sizeof("enLogErr") - 1,
			settings.logErrors);

	/* Free temporary string buffer */
	vPortFree(tmpStr);
	
	/* Send end of html page */
	SendHTML_Block(pxClient, str_end, sizeof(str_end) - 1);
	return Send_HTML_End(pxClient);
}
