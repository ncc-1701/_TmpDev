/* Includes ----------------------------------------------------------------- */
#include "settings.h"
#include "html_txt_funcs.h"
#include "HTML_Header.h"

/* Private constants -------------------------------------------------------- */
/* Structures definitions --------------------------------------------------- */
/* Variables ---------------------------------------------------------------- */
/* Parsed html */
static const char str_hdr_b[] = "\
<!DOCTYPE html PUBLIC \"HTML 4.01 Transitional/EN\">\r\
<html>\r\
<head>\r\
  <meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\">\r";
static const char str_refresh_b[] = "\
  <meta http-equiv=\"refresh\" content=\""; //15
static const char str_refresh_e[] = "\">\r";
static const char str_hdr_1[] = "\
  <title>"; //HTML Title
static const char str_hdr_2[] = "</title>\r\
  <meta http-equiv=\"cache-control\" content=\"max-age=0\">\r\
  <meta http-equiv=\"cache-control\" content=\"no-cache\">\r\
  <meta http-equiv=\"expires\" content=\"0\">\r\
  <meta http-equiv=\"expires\" content=\"Tue, 01 Jan 1980 1:00:00 GMT\">\r\
  <meta http-equiv=\"pragma\" content=\"no-cache\">\r\
  <script type=\"text/javascript\">\r\
  function stopRKey(evt)\r\
  {\r\
    var evt = (evt) ? evt : ((event) ? event : null);\r\
    var node = (evt.target) ? evt.target : \
((evt.srcElement) ? evt.srcElement : null);\r\
    if ((evt.keyCode == 13) && (node.type==\"text\"))  {return false;}\r\
  }\r\
  document.onkeypress = stopRKey;\r\
  </script>\r\
</head>\r\
<body";
static const char str_hdr_3[] = ">\r\
<form action=\"\">\r\
  <font size=\"+2\">\r";

static const char str_html_enum_b[] = "\
    <a href=\"HTML_Main.html\">Home</a><br>\r\
    <a href=\"HTML_DateTimeSettings.html\">Date and time settings</a><br>\r\
    <a href=\"HTML_DisplaySettings.html\">Display settings</a><br>\r\
    <a href=\"HTML_ScreenModesSettings.html\">Screen modes settings</a><br>\r\
    <a href=\"HTML_WeatherSensorsSettings.html\">\
Weather sensors settings</a><br>\r";

#ifdef ALARM_INPUT
	static const char str_html_alarm_inp_set[] = "\
	<a href=\"HTML_AlarmInpSettings.html\">Alarm input settings</a><br>\r";
#endif /*ALARM_INPUT*/

#ifdef SCHEDULER
	static const char str_html_schdlr_set[] = "\
	<a href=\"HTML_SchedulerSettings.html\">Scheduler settings</a><br>\r";
	static const char str_html_schdlr_tbl[] = "\
	<a href=\"HTML_SchedulerTables.html\">Scheduler tables</a><br>\r";
#endif /*SCHEDULER*/

static const char str_html_enum_1[] = "\
    <a href=\"HTML_NetworkSettings.html\">Network settings</a><br>\r\
    <a href=\"HTML_SyncSettings.html\">\
Synchronization settings</a><br><br>\r";

/* Public functions --------------------------------------------------------- */
BaseType_t Send_HTML_Header(HTTPClient_t *pxClient,
							const char* formName,
							const char* onLoadScrpt,
							uint16_t autoupdatePeriod)
{
	/* Send html header */
	SendHTML_Header_OK(pxClient);

	/* Send html page part by part */
	SendHTML_Block(pxClient, str_hdr_b, sizeof(str_hdr_b) - 1);

	/* If need - send refresh meta content */
	if(autoupdatePeriod)
	{
		char tmpStr[4];
		SendHTML_Block(pxClient, str_refresh_b,
				sizeof(str_refresh_b) - 1);

		memset(tmpStr, 0x00, sizeof(tmpStr));
		SetNumToStr(autoupdatePeriod, tmpStr, sizeof(tmpStr));
		SendHTML_Block(pxClient, tmpStr,
			GetSizeOfStr(tmpStr, sizeof(tmpStr)));

		SendHTML_Block(pxClient, str_refresh_e,
						sizeof(str_refresh_e) - 1);
	}

	SendHTML_Block(pxClient, str_hdr_1, sizeof(str_hdr_1) - 1);

	/* Insert HTML Title (max 64 chars) */
	SendHTML_Block(pxClient, formName,
			GetSizeOfStr((char*)formName, 64));
	SendHTML_Block(pxClient, str_hdr_2, sizeof(str_hdr_2) - 1);

	/* If need, insert <body> onLoad java-script function (max 64 chars) */
	if(onLoadScrpt != NULL)
	{
		SendHTML_Block(pxClient,  " onload=\"",
				sizeof(" onload=\"") - 1);
		SendHTML_Block(pxClient, onLoadScrpt,
				GetSizeOfStr((char*)onLoadScrpt, 64));
		SendHTML_Block(pxClient,  "\"", sizeof("\"") - 1);
	}

	SendHTML_Block(pxClient, str_hdr_3, sizeof(str_hdr_3) - 1);

#ifdef DEVICE_NAME
	/* Send device name */
	SendHTML_Block(pxClient, DEVICE_NAME, sizeof(DEVICE_NAME) - 1);

	/* Add new string */
	SendHTML_Block(pxClient, "<br>\r", sizeof("<br>\r") - 1);
#endif /*DEVICE_NAME*/

	/* Change font */
	SendHTML_Block(pxClient, "</font><font size=\"+1\">",
			sizeof("</font><font size=\"+1\">") - 1);

	SendHTML_Block(pxClient, str_html_enum_b,
			sizeof(str_html_enum_b) - 1);

#ifdef ALARM_INPUT
	SendHTML_Block(pxClient, str_html_alarm_inp_set,
			sizeof(str_html_alarm_inp_set) - 1);
#endif /*ALARM_INPUT*/

#ifdef SCHEDULER
	SendHTML_Block(pxClient, str_html_schdlr_set,
			sizeof(str_html_schdlr_set) - 1);
	SendHTML_Block(pxClient, str_html_schdlr_tbl,
			sizeof(str_html_schdlr_tbl) - 1);
#endif /*SCHEDULER*/

	SendHTML_Block(pxClient, str_html_enum_1,
			sizeof(str_html_enum_1) - 1);

	/* Change font */
	return SendHTML_Block(pxClient, "</font><font size=\"+2\">",
			sizeof("</font><font size=\"+2\">") - 1);
}

static BaseType_t InternalSendHTML_End(HTTPClient_t *pxClient,
		const char* js, bool serviceDot)
{
	static const char str_service_settings_href[] = "\
<a href=\"HTML_ServiceSettings.html\">.</a>";
	static const char str_end_b[] = "\
<br>\r\
  For more info, visit: \
  <a href=\"http://trc.com.ua\">http://trc.com.ua</a><br>\r\
  Build date: ";
	static const char str_end_1[] = "\r\
</form>\r";
	static const char str_end_e[] = "\
</body>\r\
</html>";

	/* Send end of html page */
	SendHTML_Block(pxClient, str_end_b, sizeof(str_end_b) - 1);
	SendHTML_Block(pxClient, __DATE__, sizeof(__DATE__) - 1);
	SendHTML_Block(pxClient, " ", sizeof(" ") - 1);
	SendHTML_Block(pxClient, __TIME__, sizeof(__TIME__) - 1);

#ifdef PROJECT_VER
	SendHTML_Block(pxClient, " V", sizeof(" V") - 1);
	SendHTML_Block(pxClient, PROJECT_VER, sizeof(PROJECT_VER) - 1);
#endif /*PROJECT_VER*/

	/* Add service settings hint reference */
	if(serviceDot) SendHTML_Block(pxClient, str_service_settings_href,
			sizeof(str_service_settings_href) - 1);

	SendHTML_Block(pxClient, str_end_1, sizeof(str_end_1) - 1);
	if(js != NULL) SendHTML_Block(pxClient, js, GetSizeOfStr((char*)js, 1000));
	SendHTML_Block(pxClient, str_end_e, sizeof(str_end_e) - 1);

	/* Send last empty block */
	return SendHTML_Block(pxClient, "", 0);
}

BaseType_t Send_HTML_End(HTTPClient_t *pxClient)
{
	return InternalSendHTML_End(pxClient, NULL, false);
}

BaseType_t Send_HTML_EndWithJS(HTTPClient_t *pxClient, const char* js)
{
	return InternalSendHTML_End(pxClient, js, false);
}

BaseType_t Send_HTML_EndWithServiceSettingsHint(HTTPClient_t *pxClient)
{
	return InternalSendHTML_End(pxClient, NULL, true);
}
