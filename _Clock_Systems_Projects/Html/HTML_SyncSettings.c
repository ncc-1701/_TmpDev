/* Includes ----------------------------------------------------------------- */
#include "web-server.h"
#include "html_txt_funcs.h"
#include "HTML_Header.h"
#include "HTML_SyncSettings.h"

/* Application includes */
#include "sntp.h"

/* Private constants -------------------------------------------------------- */
#define HTML_SNC_SET_TMP_BUF_LEN 	0xFF

/* Structures definitions --------------------------------------------------- */
struct __attribute__ ((__packed__)) HTML_SyncSettings
{
	/* NTP sync settings */
	bool NTP_SncEn;
	uint32_t NTP_SncPer;
	uint32_t NTP_StrtUpDel;
	struct NTP_ServerSettings NTP_Settings[QUANT_NTP_SERVERS];
};

/* Variables ---------------------------------------------------------------- */
/* Structure for settings operations */
static struct HTML_SyncSettings settings;

/* Private function prototypes ---------------------------------------------- */
static BaseType_t Send_HTML(HTTPClient_t *pxClient);

/* Public functions --------------------------------------------------------- */
BaseType_t Parse_HTML_SyncSettings(HTTPClient_t *pxClient)
{
	const char* buf = pxClient->pcUrlData;
	if(QueryCmp(&buf, "/HTML_SyncSettings.html") == pdFALSE) return pdFALSE;

	/* Search for "?" - start of parameters */
	bool setDefSet = pdFALSE;
	bool apply = pdFALSE;
	if(SearchForStartParameter(&buf))
	{
		/* Create temporarily variables */
		char tmpValStr[32];
		int32_t tmp32;

		/* Get time sync settings (used as preinit actions),
		   but reset all check-boxes, if they are existing */
		/* NTP sync settings */
		settings.NTP_SncEn = false;
		for(uint8_t i = 0; i < QUANT_NTP_SERVERS; i++)
		{
			settings.NTP_Settings[i].enabled = false;
		}

		taskENTER_CRITICAL();
		{
			/* NTP sync settings */
			settings.NTP_SncPer = SNTP_GetSyncPeriod();
			settings.NTP_StrtUpDel = SNTP_GetStartupDelay();

			/* Get NTP servers */
			for(uint8_t i = 0; i < QUANT_NTP_SERVERS; i++)
			{
				/* Get value as string */
				SetValue(NTP_Servers[i].NTP, settings.NTP_Settings[i].NTP,
						sizeof(settings.NTP_Settings[i].NTP));
			}
		}
		taskEXIT_CRITICAL();

		while(pdTRUE)
		{
			/* Parse parameters */
			/* Set default settings ------------------------------------------*/
			if(ParamIsEqu(&buf, "b_def"))
			{
				if(ValueCmp(buf, "def_st")) setDefSet = true;

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Flag SNTP synchronization */
			if(ParamIsEqu(&buf, "NTP_en"))
			{
				if(ValueCmp(buf, "on"))
					settings.NTP_SncEn = true;

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* NTP sync settings ---------------------------------------------*/
			/* Period of synchronization */
			if(ParamIsEqu(&buf, "T_NTP_Snc"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if(tmp32 >= 0) settings.NTP_SncPer = (uint32_t)tmp32;
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Startup delay for NTP synchronization */
			if(ParamIsEqu(&buf, "SUD_NTP_Snc"))
			{
				/* Try to convert string to number */
				if(GetNumFromStr(&buf, &tmp32, pdTRUE))
				{
					if(tmp32 >= 0) settings.NTP_StrtUpDel = (uint32_t)tmp32;
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Button "SyncNow" */
			if(ParamIsEqu(&buf, "b_SncNow"))
			{
				if(ValueCmp(buf, "SncNow"))
				{
					/* Just execute command */
					SNTP_SyncNow();
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* NTP servers table parsing */
			char* pTmpStrPrc;
			char* pLastTmpStrPrc;
			uint16_t tmpStrFreeLng;
			for(uint8_t i = 0; i < QUANT_NTP_SERVERS; i++)
			{
				/* Parse parameters */
				/* Create parameter of NTP server enabled flag */
				/* Init pointers and length */
				pTmpStrPrc = tmpValStr;
				pLastTmpStrPrc = pTmpStrPrc;
				tmpStrFreeLng = HTML_SNC_SET_TMP_BUF_LEN;
				pTmpStrPrc = SetValue("NTP_en", pTmpStrPrc, tmpStrFreeLng);
				/* Correct pointers and length */
				pLastTmpStrPrc = pTmpStrPrc;
				tmpStrFreeLng -= (pLastTmpStrPrc - pTmpStrPrc);
				pTmpStrPrc = SetNumToStr(i, pTmpStrPrc, tmpStrFreeLng);
				if(ParamIsEqu(&buf, tmpValStr))
				{
					// Get value as string
					if(ValueCmp(buf, "on"))
						settings.NTP_Settings[i].enabled = true;

					// Watch for end of parameters
					if(SearchForNextParameter(&buf) == false) break;
				}

				/* Create parameter of NTP server name */
				/* Init pointers and length */
				pTmpStrPrc = tmpValStr;
				pLastTmpStrPrc = pTmpStrPrc;
				tmpStrFreeLng = HTML_SNC_SET_TMP_BUF_LEN;
				pTmpStrPrc = SetValue("NTP_nm", pTmpStrPrc, tmpStrFreeLng);
				/* Correct pointers and length */
				pLastTmpStrPrc = pTmpStrPrc;
				tmpStrFreeLng -= (pLastTmpStrPrc - pTmpStrPrc);
				pTmpStrPrc = SetNumToStr(i, pTmpStrPrc, tmpStrFreeLng);
				if(ParamIsEqu(&buf, tmpValStr))
				{
					GetValueFromTxtField(&buf,
							settings.NTP_Settings[i].NTP,
							sizeof(settings.NTP_Settings[i].NTP), true);

					// Watch for end of parameters
					if(SearchForNextParameter(&buf) == false) break;
				}
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

	if(setDefSet)
	{
		apply = pdFALSE;
		taskENTER_CRITICAL();
		{
			SNTP_SetDefaults();
		}
		taskEXIT_CRITICAL();
	}
	if(apply)
	{
		/* Story time sync settings */
		taskENTER_CRITICAL();
		{
			/* NTP sync settings */
			SNTP_SetSyncEnabled(settings.NTP_SncEn);
			SNTP_SetSyncPeriod(settings.NTP_SncPer);
			SNTP_SetStartupDelay(settings.NTP_StrtUpDel);
			for(uint8_t i = 0; i < QUANT_NTP_SERVERS; i++)
			{
				NTP_Servers[i].enabled = settings.NTP_Settings[i].enabled;
				/* Get value as string */
				SetValue(settings.NTP_Settings[i].NTP, NTP_Servers[i].NTP,
						sizeof(settings.NTP_Settings[i].NTP));
			}
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
    Synchronization settings:<br>\r\
  </font>\r\
<pre>\r\
<button name=\"b_def\" type=\"submit\" value=\"def_st\">\
Set default settings</button>";
	static const char str_end[] = "\r\r\
<button name=\"b_apl\" type=\"submit\" value=\"apl_st\">\
Apply settings</button>\
</pre>";

	/* Attempt to create the temporary string buffer */
	char* tmpStr = (char*)pvPortMalloc(HTML_SNC_SET_TMP_BUF_LEN);
	if(tmpStr == NULL)
	{
		FreeRTOS_printf(("Could not create temporary string buffer\n"));
		return -1;
	}

	/* Get time sync settings */
	taskENTER_CRITICAL();
	{
		/* NTP sync settings */
		settings.NTP_SncEn = SNTP_GetSyncEnabled();
		settings.NTP_SncPer = SNTP_GetSyncPeriod();
		settings.NTP_StrtUpDel = SNTP_GetStartupDelay();
		for(uint8_t i = 0; i < QUANT_NTP_SERVERS; i++)
		{
			settings.NTP_Settings[i].enabled = NTP_Servers[i].enabled;
			/* Get value as string */
			SetValue(NTP_Servers[i].NTP, settings.NTP_Settings[i].NTP,
						sizeof(settings.NTP_Settings[i].NTP));
		}
	}
	taskEXIT_CRITICAL();

	/* Send html page part by part */
	/* Send header and begin of html*/
	Send_HTML_Header(pxClient, "HTML_SyncSettings", NULL, 0);
	SendHTML_Block(pxClient, str_begin, sizeof(str_begin) - 1);

	/* Send SNTP synchronization enable flag */
	static const char str_NTP_en_b[] = "\r\
Enable NTP synchronization                ";
	SendHTML_Block(pxClient, str_NTP_en_b,
			sizeof(str_NTP_en_b) - 1);
	SendCheckBox(pxClient, false, false, "NTP_en", sizeof("NTP_en") - 1,
			settings.NTP_SncEn);

	/* NTP sync settings */
	/* Send period of NTP synchronization */
	static const char str_set_T_NTP_Snc_b[] = "\r\r</pre>\
NTP synchronization settings:<pre>\r\
Period of synchronization\r\
(allowed values from 20 to 86400 seconds) ";
	SendHTML_Block(pxClient, str_set_T_NTP_Snc_b,
			sizeof(str_set_T_NTP_Snc_b) - 1);
	SetNumToStr(settings.NTP_SncPer, tmpStr, HTML_SNC_SET_TMP_BUF_LEN);
	SendInput(pxClient, false, false,
			"T_NTP_Snc", sizeof("T_NTP_Snc") - 1,
			tmpStr, GetSizeOfStr(tmpStr, HTML_SNC_SET_TMP_BUF_LEN), 4);

	/* Send startup delay for NTP synchronization */
	static const char str_set_SUD_NTP_Snc_b[] = "\r\
Startup delay for NTP synchronization\r\
(max value is 86400 seconds)              ";
	SendHTML_Block(pxClient, str_set_SUD_NTP_Snc_b,
			sizeof(str_set_SUD_NTP_Snc_b) - 1);
	SetNumToStr(settings.NTP_StrtUpDel, tmpStr, HTML_SNC_SET_TMP_BUF_LEN);
	SendInput(pxClient, false, false,
			"SUD_NTP_Snc", sizeof("SUD_NTP_Snc") - 1,
			tmpStr, GetSizeOfStr(tmpStr, HTML_SNC_SET_TMP_BUF_LEN), 4);

	/* Button "SyncNow" */
	static const char str_btnSncNow[] = "\r\r\
<button name=\"b_SncNow\" type=\"submit\" value=\"SncNow\">\
Sync date and time with NTP now</button>";
	SendHTML_Block(pxClient, str_btnSncNow,
			sizeof(str_btnSncNow) - 1);

	/* Send NTP servers table */
	/* Table name */
	static const char str_tbl_name_NTP_svr[] = "\r\r\
NTP servers list:\r";

	/* Table caption and ending */
static const char str_tbl_NTP_svr_b[] = "\r\
<table border=\"0\" width=\"950\" cellspacing=\"0\" cellpadding=\"0\">\r\
  <tr>\r\
    <td>\r\
      <table border=\"1\" cellspacing=\"0\" cellpadding=\"5\">\r\
        <tr align=\"left\">\r\
          <th width=40>N</th>\r\
          <th width=70>Enable</th>\r\
          <th width=750>NTP server (DNS-name or IP-address)</th>\r\
        </tr>\r\
      </table>\r\
    </td>\r\
  </tr>\r";
static const char str_tbl_e[] = "\
</table>\r";
	/* Table row */
static const char str_rw_b[] = "\
  <tr>\r\
    <td>\r\
      <table border=\"1\" cellspacing=\"0\" cellpadding=\"5\">\r\
        <tr>\r";
static const char cl_N_b[] = "\
          <td width=40>";
static const char cl_NTP_en_b[] = "\
          <td width=70>";
static const char cl_NTP_str_b[] = "\
          <td width=750>";
static const char cl_e[] ="</td>\r";
static const char str_rw_e[] = "\
        </tr>\r\
      </table>\r\
    </td>\r\
  </tr>\r";

	/* Disable "pre" tag */
	SendHTML_Block(pxClient, "</pre>", sizeof("</pre>") - 1);
	/* Send table name */
	SendHTML_Block(pxClient, str_tbl_name_NTP_svr,
			sizeof(str_tbl_name_NTP_svr) - 1);
	/* Send table caption */
	SendHTML_Block(pxClient, str_tbl_NTP_svr_b,
			sizeof(str_tbl_NTP_svr_b) - 1);

	/* Send table rows */
	char* pTmpStrPrc;
	char* pLastTmpStrPrc;
	uint16_t tmpStrFreeLng;
	for(uint8_t i = 0; i < QUANT_NTP_SERVERS; i++)
	{
		/* Send start of row */
		SendHTML_Block(pxClient, str_rw_b, sizeof(str_rw_b) - 1);

		/* Send number of screen mode */
		SendHTML_Block(pxClient, cl_N_b, sizeof(cl_N_b) - 1);
		SetNumToStr(i + 1, tmpStr, HTML_SNC_SET_TMP_BUF_LEN);
		SendHTML_Block(pxClient, tmpStr,
				  GetSizeOfStr(tmpStr, HTML_SNC_SET_TMP_BUF_LEN));
		SendHTML_Block(pxClient, cl_e, sizeof(cl_e) - 1);

		/* Send enabled flag of screen mode */
		SendHTML_Block(pxClient,
				cl_NTP_en_b, sizeof(cl_NTP_en_b) - 1);
		/* Init pointers and length */
		pTmpStrPrc = tmpStr;
		pLastTmpStrPrc = pTmpStrPrc;
		tmpStrFreeLng = HTML_SNC_SET_TMP_BUF_LEN;
		pTmpStrPrc = SetValue("NTP_en", pTmpStrPrc, tmpStrFreeLng);
		/* Correct pointers and length */
		pLastTmpStrPrc = pTmpStrPrc;
		tmpStrFreeLng -= (pLastTmpStrPrc - pTmpStrPrc);
		pTmpStrPrc = SetNumToStr(i, pTmpStrPrc, tmpStrFreeLng);
		SendCheckBox(pxClient, false, false, tmpStr,
				GetSizeOfStr(tmpStr, HTML_SNC_SET_TMP_BUF_LEN),
		settings.NTP_Settings[i].enabled);
		SendHTML_Block(pxClient, cl_e, sizeof(cl_e) - 1);

		/* Send NTP server name as string */
		SendHTML_Block(pxClient,
				cl_NTP_str_b, sizeof(cl_NTP_str_b) - 1);
		/* Init pointers and length */
		pTmpStrPrc = tmpStr;
		pLastTmpStrPrc = pTmpStrPrc;
		tmpStrFreeLng = HTML_SNC_SET_TMP_BUF_LEN;
		pTmpStrPrc = SetValue("NTP_nm", pTmpStrPrc, tmpStrFreeLng);
		/* Correct pointers and length */
		pLastTmpStrPrc = pTmpStrPrc;
		tmpStrFreeLng -= (pLastTmpStrPrc - pTmpStrPrc);
		pTmpStrPrc = SetNumToStr(i, pTmpStrPrc, tmpStrFreeLng);
		SendInput(pxClient, false, false, tmpStr,
				GetSizeOfStr(tmpStr, HTML_SNC_SET_TMP_BUF_LEN),
				settings.NTP_Settings[i].NTP,
				GetSizeOfStr(settings.NTP_Settings[i].NTP,
						HTML_SNC_SET_TMP_BUF_LEN), 100);
		SendHTML_Block(pxClient, cl_e, sizeof(cl_e) - 1);

		/* Send end of row */
		SendHTML_Block(pxClient, str_rw_e, sizeof(str_rw_e) - 1);
	}

	/* Send end of table */
	SendHTML_Block(pxClient, str_tbl_e, sizeof(str_tbl_e) - 1);
	/* Enable "pre" tag */
	SendHTML_Block(pxClient, "<pre>", sizeof("<pre>") - 1);

	/* Free temporary string buffer */
	vPortFree(tmpStr);
	
	/* Send end of html page */
	SendHTML_Block(pxClient, str_end, sizeof(str_end) - 1);
	return Send_HTML_End(pxClient);
}
