/* Includes ----------------------------------------------------------------- */
#include "settings.h"
#include "web-server.h"
#include "html_txt_funcs.h"
#include "HTML_Header.h"
#include "HTML_Main.h"

/* Application includes */
#include "rtc.h"
#include "sntp.h"
#include "TRS_sync_proto.h"

/* Private constants -------------------------------------------------------- */
#define HTML_MAIN_TMP_BUF_LEN 		32

/* Private function prototypes ---------------------------------------------- */
static BaseType_t Send_HTML(HTTPClient_t *pxClient);

/* Public functions --------------------------------------------------------- */
BaseType_t Parse_HTML_Main(HTTPClient_t *pxClient)
{
	const char* buf = pxClient->pcUrlData;
	bool send = pdFALSE;
	if(QueryCmp(&buf, "/")) send = pdTRUE;
	if(QueryCmp(&buf, "/HTML_Main.html")) send = pdTRUE;
	if(send == pdFALSE) return pdFALSE;

	/* Send html to client */
	return Send_HTML(pxClient);
}

/* Private functions -------------------------------------------------------- */
static BaseType_t Send_HTML(HTTPClient_t *pxClient)
{
	/* Parsed html */
	static const char str_begin[] = "\
    Common status:\r\
  </font>\r\
<pre>";
	static const char str_end[] = "\r\r\
<input name=\"Refresh\" value=\"Refresh status\" type=\"submit\">\r\
</pre>";

	/* Attempt to create the temporary string buffer */
	char* tmpStr = (char*)pvPortMalloc(HTML_MAIN_TMP_BUF_LEN);
	if(tmpStr == NULL)
	{
		FreeRTOS_printf(("Could not create temporary string buffer\n"));
		return -1;
	}

	struct DateTime tmpDateTime;
	struct tm dt;

	/* Send html page part by part */
	/* Send header and begin of html*/
	Send_HTML_Header(pxClient, "HTML_Main", NULL, 15);
	SendHTML_Block(pxClient, str_begin, sizeof(str_begin) - 1);

	/* Send current local time and date --------------------------------------*/
	static const char str_curr_date_b[] = "\r</pre>\
RTC status:<pre>\r\
current local date (DD.MM.YYYY): ";
	static const char str_curr_time_b[] = "\r\
current local time (HH.MM.SS):   ";

	if(RTC_GetTimeIsValide() == false)
	{
		/* Show invalid RTC state */
		SendHTML_Block(pxClient, str_curr_date_b,
				sizeof(str_curr_date_b) - 1);
		SendHTML_Block(pxClient, "n/a\r", sizeof("n/a\r") - 1);
		SendHTML_Block(pxClient, str_curr_time_b,
				sizeof(str_curr_time_b) - 1);
		SendHTML_Block(pxClient, "n/a", sizeof("n/a") - 1);
	}
	else
	{
		/* Get current time */
		RTC_GetLocalDateTime(&tmpDateTime);
		/* Structures conversion */
		dt.tm_year = tmpDateTime.year - YEAR0;
		dt.tm_mon = tmpDateTime.month - 1;
		dt.tm_mday = tmpDateTime.day;
		dt.tm_hour = tmpDateTime.hour;
		dt.tm_min = tmpDateTime.minute;
		dt.tm_sec = tmpDateTime.second;

		SendHTML_Block(pxClient, str_curr_date_b,
				sizeof(str_curr_date_b) - 1);
		SetDateToStr(&dt, tmpStr, HTML_MAIN_TMP_BUF_LEN);
		SendHTML_Block(pxClient, tmpStr,
			GetSizeOfStr(tmpStr, HTML_MAIN_TMP_BUF_LEN));

		/* Send day of week */
		SendHTML_Block(pxClient, " ", sizeof(" ") - 1);
		switch(tmpDateTime.dayOfWeek)
		{
		case MONDAY:
			SendHTML_Block(pxClient, "Monday",
					sizeof("Monday") - 1);
			break;
		case TUESDAY:
			SendHTML_Block(pxClient, "Tuesday",
					sizeof("Tuesday") - 1);
			break;
		case WEDNESDAY:
			SendHTML_Block(pxClient, "Wednesday",
					sizeof("Wednesday") - 1);
			break;
		case THURSDAY:
			SendHTML_Block(pxClient, "Thursday",
					sizeof("Thursday") - 1);
			break;
		case FRIDAY:
			SendHTML_Block(pxClient, "Friday",
					sizeof("Friday") - 1);
			break;
		case SATURDAY:
			SendHTML_Block(pxClient, "Saturday",
					sizeof("Saturday") - 1);
			break;
		case SUNDAY:
			SendHTML_Block(pxClient, "Sunday",
					sizeof("Sunday") - 1);
			break;
		}

		SendHTML_Block(pxClient, str_curr_time_b,
				sizeof(str_curr_time_b) - 1);
		SetTimeToStr(&dt, tmpStr, HTML_MAIN_TMP_BUF_LEN);
		SendHTML_Block(pxClient, tmpStr,
			GetSizeOfStr(tmpStr, HTML_MAIN_TMP_BUF_LEN));
	}
	
	/* Show synchronization status -------------------------------------------*/
	static const char str_last_snc_dt_b[] = "\r\r</pre>\
Synchronization status:<pre>\r\
last successful synchronization at: ";

	SendHTML_Block(pxClient, str_last_snc_dt_b,
			sizeof(str_last_snc_dt_b) - 1);

	enum NTP_TimeStatus lastSncSrc = GetNTP_TimeStatus();
	if(lastSncSrc == NTP_TimeInvalid)
		SendHTML_Block(pxClient, "n/a", sizeof("n/a") - 1);
	else
	{
		/* Get last successful synchronization time and convert it to local */
		SNTP_GetLastSyncTime(&tmpDateTime);
		UTC_To_Local_DateTime(&tmpDateTime);

		/* Structures conversion */
		dt.tm_year = tmpDateTime.year - YEAR0;
		dt.tm_mon = tmpDateTime.month - 1;
		dt.tm_mday = tmpDateTime.day;
		dt.tm_hour = tmpDateTime.hour;
		dt.tm_min = tmpDateTime.minute;
		dt.tm_sec = tmpDateTime.second;

		SetDateToStr(&dt, tmpStr, HTML_MAIN_TMP_BUF_LEN);
		SendHTML_Block(pxClient, tmpStr,
			GetSizeOfStr(tmpStr, HTML_MAIN_TMP_BUF_LEN));
		SendHTML_Block(pxClient, " ", sizeof(" ") - 1);

		SetTimeToStr(&dt, tmpStr, HTML_MAIN_TMP_BUF_LEN);
		SendHTML_Block(pxClient, tmpStr,
			GetSizeOfStr(tmpStr, HTML_MAIN_TMP_BUF_LEN));

		/* Show synchronization status */
		static const char str_last_snc_src_b[] = "\r\
sync source:                        ";
		SendHTML_Block(pxClient, str_last_snc_src_b,
				sizeof(str_last_snc_src_b) - 1);

		SendHTML_Block(pxClient, "NTP server ",
				sizeof("NTP server ") - 1);

		/* Send last used NTP server name */
		uint16_t srtSize = GetSizeOfStr(
				NTP_Servers[sntpRequestedServer].NTP,
				HTML_MAIN_TMP_BUF_LEN);

		if(srtSize)
		{
			SendHTML_Block(pxClient,
					NTP_Servers[sntpRequestedServer].NTP, srtSize);
		}
	}

	/* Show detail NTP synchronization status */
	static const char str_NTP_status_b[] = "\r\r</pre>\
NTP detailed synchronization status:<pre>\r\
synchronization status:                      ";

	SendHTML_Block(pxClient, str_NTP_status_b,
				sizeof(str_NTP_status_b) - 1);
	if(SNTP_GetSyncEnabled() == false)
	{
		SendHTML_Block(pxClient, "NTP synchronization disabled",
				sizeof("NTP synchronization disabled") - 1);
	}
	else
	{
	switch(GetNTP_TimeStatus())
	{
		case NTP_TimeValid:
			SetValue("NTP time is valid", tmpStr, HTML_MAIN_TMP_BUF_LEN);
			break;

		case NTP_TimeNoActual:
			SetValue("NTP time is not actual", tmpStr, HTML_MAIN_TMP_BUF_LEN);
			break;

		case NTP_TimeInvalid:
		default:
			SetValue("NTP time is invalid", tmpStr, HTML_MAIN_TMP_BUF_LEN);
		}
		SendHTML_Block(pxClient, tmpStr,
					  GetSizeOfStr(tmpStr, HTML_MAIN_TMP_BUF_LEN));

		/* Send last time of synchronization */
		static const char str_NTP_status_1[] = "\r\
last successful synchronization with NTP at: ";

		SendHTML_Block(pxClient, str_NTP_status_1,
				sizeof(str_NTP_status_1) - 1);
		if(SNTP_GetLastSyncTime(&tmpDateTime))
		{
			UTC_To_Local_DateTime(&tmpDateTime);

			/* Structures conversion */
			dt.tm_year = tmpDateTime.year - YEAR0;
			dt.tm_mon = tmpDateTime.month - 1;
			dt.tm_mday = tmpDateTime.day;
			dt.tm_hour = tmpDateTime.hour;
			dt.tm_min = tmpDateTime.minute;
			dt.tm_sec = tmpDateTime.second;

			SetDateToStr(&dt, tmpStr, HTML_MAIN_TMP_BUF_LEN);
			SendHTML_Block(pxClient, tmpStr,
				GetSizeOfStr(tmpStr, HTML_MAIN_TMP_BUF_LEN));
			SendHTML_Block(pxClient, " ", sizeof(" ") - 1);

			SetTimeToStr(&dt, tmpStr, HTML_MAIN_TMP_BUF_LEN);
			SendHTML_Block(pxClient, tmpStr,
				GetSizeOfStr(tmpStr, HTML_MAIN_TMP_BUF_LEN));
		}
		else SendHTML_Block(pxClient, "n/a", sizeof("n/a") - 1);

		/* Send info about last used NTP server */
		static const char str_NTP_status_2[] = "\r\
last used NTP server exchange status:        ";
		SendHTML_Block(pxClient, str_NTP_status_2,
				sizeof(str_NTP_status_2) - 1);
		if(sntpRequestedServer < QUANT_NTP_SERVERS)
		{
			// Show last status
			switch(lastNTP_RequestStatus)
			{
			case NTP_RequestTimeOut:
				SendHTML_Block(pxClient, "time out with ",
						sizeof("time out with ") - 1);
				break;

			case NTP_RequestComplete:
				SendHTML_Block(pxClient, "successfully with ",
						sizeof("successfully with ") - 1);
				break;

			case NTP_RequestFailed:
			default:
				SendHTML_Block(pxClient, "failed with ",
						sizeof("failed with ") - 1);
			}

			/* Send last used NTP server name */
			uint16_t srtSize = GetSizeOfStr(
						NTP_Servers[sntpRequestedServer].NTP,
						HTML_MAIN_TMP_BUF_LEN);

			if(srtSize)
			{
				SendHTML_Block(pxClient,
						NTP_Servers[sntpRequestedServer].NTP, srtSize);
			}
		}
		else SendHTML_Block(pxClient, "n/a", sizeof("n/a") - 1);
	}

	SendHTML_Block(pxClient, "\r", sizeof("\r") - 1);

	/* Send some service info */
	static const char str_svc_info_b[] = "\r\
-------------------------------------------------------------------------------\
-\r";
	SendHTML_Block(pxClient, str_svc_info_b, sizeof(str_svc_info_b) - 1);

#if (configUSE_FAT == 1)
	/* Show disk state */
	enum FF_DiskState state = GetFF_DiskState();

	static const char str_dsk_state_b[] = "SD-card state: ";
	SendHTML_Block(pxClient, str_dsk_state_b, sizeof(str_dsk_state_b) - 1);
	if(state == FF_DISK_REMOVED)
		SendHTML_Block(pxClient, "removed\r", sizeof("removed\r") - 1);
	else if(state == FF_DISK_NO_INITED)
		SendHTML_Block(pxClient, "is not inited\r",
				sizeof("is not inited\r") - 1);
	else if(state == FF_DISK_NO_MOUNTED)
		SendHTML_Block(pxClient, "is not mounted\r",
				sizeof("is not mounted\r") - 1);
	else SendHTML_Block(pxClient, "is mounted\r",
				sizeof("is mounted\r") - 1);
#endif /*(configUSE_FAT == 1)*/

	/* Show upTime */
	static const char str_up_time_b[] = "UpTime: ";
	SendHTML_Block(pxClient, str_up_time_b, sizeof(str_up_time_b) - 1);
	SetTimeIntervalToStr(GetUpTime(), tmpStr, HTML_MAIN_TMP_BUF_LEN);
	SendHTML_Block(pxClient, tmpStr,
		GetSizeOfStr(tmpStr, HTML_MAIN_TMP_BUF_LEN));

	/* Free temporary string buffer */
	vPortFree(tmpStr);
	
	/* Send end of html page */
	SendHTML_Block(pxClient, str_end, sizeof(str_end) - 1);
	return Send_HTML_EndWithServiceSettingsHint(pxClient);
}
