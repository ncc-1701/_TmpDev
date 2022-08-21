/* Includes ----------------------------------------------------------------- */
#include "web-server.h"
#include "html_txt_funcs.h"
#include "HTML_Header.h"
#include "HTML_NetworkSettings.h"

/* Private constants -------------------------------------------------------- */
#define HTML_NETWORK_SETTINGS_TMP_BUF_LEN 16

/* Structures definitions --------------------------------------------------- */
struct __attribute__ ((__packed__)) HTML_NetworkSettings
{
	/* Network settings */
	enum ProtocolType protocolType;
	uint32_t currIP_Addr;
	uint32_t currNetmask;
	uint32_t currIP_Addr_GW;
	uint32_t currIP_Addr_DNS;
	uint32_t staticIP_Addr;
	uint32_t staticNetmask;
	uint32_t staticIP_Addr_GW;
	uint32_t staticIP_Addr_DNS;

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
	/* MAC settings */
	BaseType_t MAC_AddrIsOvrd;
	uint8_t MAC_Addr[ipMAC_ADDRESS_LENGTH_BYTES];
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

	/* Login and password for UI */
	char login[HTML_LOGIN_MAX_LEN];
	char password[HTML_PASSW_MAX_LEN];
};

/* Variables ---------------------------------------------------------------- */
/* Structure for settings operations */
static struct HTML_NetworkSettings settings;

/* Private function prototypes ---------------------------------------------- */
static BaseType_t Send_HTML(HTTPClient_t *pxClient);

/* Public functions --------------------------------------------------------- */
BaseType_t Parse_HTML_NetworkSettings(HTTPClient_t *pxClient)
{
	const char* buf = pxClient->pcUrlData;

	if(QueryCmp(&buf, "/HTML_NetworkSettings.html") == pdFALSE)
		return pdFALSE;

	/* Get settings (used as preinit actions) */
	/* Network settings */
	settings.currIP_Addr = FreeRTOS_GetIPAddress();
	settings.currNetmask = FreeRTOS_GetNetmask();
	settings.currIP_Addr_GW = FreeRTOS_GetGatewayAddress();
	settings.currIP_Addr_DNS = FreeRTOS_GetDNSServerAddress();

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
	/* MAC settings */
	settings.MAC_AddrIsOvrd = pdFALSE;
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

	taskENTER_CRITICAL();
	{
		settings.protocolType = currProtocolType;
		settings.staticIP_Addr = staticIP_Addr;
		settings.staticNetmask = staticNetMask;
		settings.staticIP_Addr_GW = staticIP_GW;
		settings.staticIP_Addr_DNS = staticIP_DNS;

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
		/* MAC settings */
		/* Copy current MAC */
		const uint8_t* pCurrMAC_Addr = WebServerGetMAC_Addr();
		uint8_t* pSetMAC_Addr = settings.MAC_Addr;
		for(uint8_t i = 0; i < ipMAC_ADDRESS_LENGTH_BYTES; i++)
		{
			*pSetMAC_Addr = *pCurrMAC_Addr;
			pSetMAC_Addr++;
			pCurrMAC_Addr++;
		}
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

		/* Login and password for UI */
		SetValue(GetLogin(), settings.login, HTML_LOGIN_MAX_LEN);
		SetValue(GetPassword(), settings.password, HTML_PASSW_MAX_LEN);
	}
	taskEXIT_CRITICAL();

	/* Search for "?" - start of parameters */
	bool apply = pdFALSE;
	if(SearchForStartParameter(&buf))
	{
		/* Create temporarily variables */
		char tmpValStr[32];
		while(pdTRUE)
		{
			/* Parse parameters */
			/* Protocol type -------------------------------------------------*/
			if(ParamIsEqu(&buf, "t_nw_st"))
			{
				if(ValueCmp(buf, "dn")) settings.protocolType = DHCP;
				else if(ValueCmp(buf, "st")) settings.protocolType = Static_IP;

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* IP */
			if(ParamIsEqu(&buf, "IP"))
			{
				/* Get value as string */
				GetValue(buf, tmpValStr, sizeof(tmpValStr));

				/* Try to convert string to IP address */
				settings.staticIP_Addr = FreeRTOS_inet_addr(tmpValStr);

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Netmask */
			if(ParamIsEqu(&buf, "msk"))
			{
				/* Get value as string */
				GetValue(buf, tmpValStr, sizeof(tmpValStr));

				/* Try to convert string to number */
				settings.staticNetmask = FreeRTOS_inet_addr(tmpValStr);

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* GW */
			if(ParamIsEqu(&buf, "GW"))
			{
				/* Get value as string */
				GetValue(buf, tmpValStr, sizeof(tmpValStr));

				/* Try to convert string to number */
				settings.staticIP_Addr_GW = FreeRTOS_inet_addr(tmpValStr);

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* DNS */
			if(ParamIsEqu(&buf, "DNS"))
			{
				/* Get value as string */
				GetValue(buf, tmpValStr, sizeof(tmpValStr));

				/* Try to convert string to number */
				settings.staticIP_Addr_DNS = FreeRTOS_inet_addr(tmpValStr);

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
			/* Parse MAC settings --------------------------------------------*/
			if(ParamIsEqu(&buf, "macRB"))
			{
				if(ValueCmp(buf, "ovrd")) settings.MAC_AddrIsOvrd = pdTRUE;

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Parse overrode MAC */
			if(ParamIsEqu(&buf, "mac_ovrd"))
			{
				/* Get value as special string */
				GetValueFromTxtField(&buf, tmpValStr, sizeof(tmpValStr), true);

				/* Try to convert string to MAC array */
				BaseType_t compl = pdFALSE;
				int32_t tmp32;
				const char* tmpStr = tmpValStr;
				uint8_t* pMAC_Addr = settings.MAC_Addr;
				for(uint8_t i = 0;;)
				{
					/* Get MAC octet */
					/* Get number and validate it */
					if(GetHexFromStr(&tmpStr, &tmp32, pdTRUE) == false)
						break;
					if(tmp32 <= 0xFF)
					{
						/* Append number to MAC array */
						*pMAC_Addr = (uint8_t)(tmp32);
					}

					i++;
					if(i >= ipMAC_ADDRESS_LENGTH_BYTES)
					{
						compl = pdTRUE;
						break;
					}
					pMAC_Addr++;

					/* Check for colon */
					if(*tmpStr != ':') break;
					tmpStr++;
				}

				/* Validate parsed MAC */
				if(compl == pdFALSE)
				{
					/* Restore the current MAC */
					taskENTER_CRITICAL();
					{
						const uint8_t* pCurrMAC_Addr = WebServerGetMAC_Addr();
						uint8_t* pSetMAC_Addr = settings.MAC_Addr;
						for(uint8_t i = 0; i < ipMAC_ADDRESS_LENGTH_BYTES; i++)
						{
							*pSetMAC_Addr = *pCurrMAC_Addr;
							pSetMAC_Addr++;
							pCurrMAC_Addr++;
						}
					}
					taskEXIT_CRITICAL();
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

			/* Login and password for UI -------------------------------------*/
			/* Login */
			if(ParamIsEqu(&buf, "login"))
			{
				/* Get value as string */
				GetValue(buf, tmpValStr, sizeof(tmpValStr));

				/* Story it to settings variable */
				SetValue(tmpValStr, settings.login, HTML_LOGIN_MAX_LEN);

				/* Watch for end of parameters */
				if(SearchForNextParameter(&buf) == false) break;
			}

			/* Password */
			if(ParamIsEqu(&buf, "passw"))
			{
				/* Get value as string */
				GetValue(buf, tmpValStr, sizeof(tmpValStr));

				/* Story it to settings variable */
				SetValue(tmpValStr, settings.password, HTML_PASSW_MAX_LEN);

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
		/* Just story network settings (but do not apply) */
		taskENTER_CRITICAL();
		{
			/* Network settings */
			currProtocolType = settings.protocolType;
			staticIP_Addr = settings.staticIP_Addr;
			staticNetMask = settings.staticNetmask;
			staticIP_GW = settings.staticIP_Addr_GW;
			staticIP_DNS = settings.staticIP_Addr_DNS;

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
			/* MAC settings */
			if(settings.MAC_AddrIsOvrd == pdFALSE) WebServerResetMAC_AddrOvrd();
			else
			{
				/* Override MAC */
				WebServerSetMAC_AddrOvrd(settings.MAC_Addr);
			}
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

			/* Login and password for UI */
			SetLogin(settings.login);
			SetPassword(settings.password);
		}
		taskEXIT_CRITICAL();

		/* Let HttpServer apply network settings */
		HTTP_ServerApplyNetworkSettingsAfterNetConnClose();
	}

	/* Send html to client */
	return Send_HTML(pxClient);
}

/* Private functions -------------------------------------------------------- */
static BaseType_t Send_HTML(HTTPClient_t *pxClient)
{
	/* Parsed html */
	static const char str_begin[] = "\
    Current network settings:<br>\r\
  </font>\r\
  <script>\r\
  function ch_DHCP() {\r\
    var radioBox = document.getElementById(\"DHCP\");\r\
    var IP = document.getElementById(\"IP\");\r\
    var msk = document.getElementById(\"msk\");\r\
    var GW = document.getElementById(\"GW\");\r\
    var DNS = document.getElementById(\"DNS\");\r\
    if (radioBox.checked == true){\r\
      IP.disabled = true;\r\
      msk.disabled = true;\r\
      GW.disabled = true;\r\
      DNS.disabled = true;\r\
    } else {\r\
      IP.disabled = false;\r\
      msk.disabled = false;\r\
      GW.disabled = false;\r\
      DNS.disabled = false;\r\
    }\r\
  }\r\
  function ch_MAC_RB() {\r\
    var radioBox = document.getElementById(\"MAC_ORIG\");\r\
    var MAC_ovrd = document.getElementById(\"MAC_ovrd\");\r\
    if (radioBox.checked == true){\r\
      MAC_ovrd.disabled = true;\r\
    } else {\r\
      MAC_ovrd.disabled = false;\r\
    }\r\
  }\r\
  function onLoadEvHndlr() {\r\
    ch_DHCP();\r\
    ch_MAC_RB();\r\
  }\r\
  </script>\r\
<pre>";
	static const char str_end[] = "\r\r\
<button name=\"b_apl\" type=\"submit\" value=\"apl_st\">\
Apply settings</button>\
</pre>";

	/* Attempt to create the temporary string buffer. */
	char* tmpStr = (char*)pvPortMalloc(HTML_NETWORK_SETTINGS_TMP_BUF_LEN);
	if(tmpStr == NULL)
	{
		FreeRTOS_printf(("Could not create temporary string buffer\n"));
		return -1;
	}
	
	/* Get app settings */
	taskENTER_CRITICAL();
	{
		/* Network settings */
		settings.protocolType = currProtocolType;
		settings.staticIP_Addr = staticIP_Addr;
		settings.staticNetmask = staticNetMask;
		settings.staticIP_Addr_GW = staticIP_GW;
		settings.staticIP_Addr_DNS = staticIP_DNS;

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
		/* MAC settings */
		settings.MAC_AddrIsOvrd = WebServerGetMAC_AddrIsOvrd();

		/* Copy current MAC */
		const uint8_t* pCurrMAC_Addr = WebServerGetMAC_Addr();
		uint8_t* pSetMAC_Addr = settings.MAC_Addr;
		for(uint8_t i = 0; i < ipMAC_ADDRESS_LENGTH_BYTES; i++)
		{
			*pSetMAC_Addr = *pCurrMAC_Addr;
			pSetMAC_Addr++;
			pCurrMAC_Addr++;
		}
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

		/* Login and password for UI */
		SetValue(GetLogin(), settings.login, HTML_LOGIN_MAX_LEN);
		SetValue(GetPassword(), settings.password, HTML_PASSW_MAX_LEN);
	}
	taskEXIT_CRITICAL();

	/* Send html page part by part */
	/* Send header and begin of html*/
	Send_HTML_Header(pxClient, "HTML_NetworkSettings", "onLoadEvHndlr()", 0);
	SendHTML_Block(pxClient, str_begin, sizeof(str_begin) - 1);

	/* Network settings ------------------------------------------------------*/
	/* Send own IP address */
	static const char str_curr_IP_b[] = "\r\
IP:            ";
	SendHTML_Block(pxClient, str_curr_IP_b,
			sizeof(str_curr_IP_b) - 1);
	memset(tmpStr, 0x00, HTML_NETWORK_SETTINGS_TMP_BUF_LEN);
	FreeRTOS_inet_ntoa(settings.currIP_Addr, tmpStr);
	SendHTML_Block(pxClient, tmpStr,
			GetSizeOfStr(tmpStr, HTML_NETWORK_SETTINGS_TMP_BUF_LEN));
	
	/* Send network mask */
static const char str_curr_msk_b[] = "\r\
Netmask:       ";
	SendHTML_Block(pxClient, str_curr_msk_b,
			sizeof(str_curr_msk_b) - 1);
	memset(tmpStr, 0x00, HTML_NETWORK_SETTINGS_TMP_BUF_LEN);
	FreeRTOS_inet_ntoa(settings.currNetmask, tmpStr);
	SendHTML_Block(pxClient, tmpStr,
			GetSizeOfStr(tmpStr, HTML_NETWORK_SETTINGS_TMP_BUF_LEN));
	
	/* Send GW address */
	static const char str_curr_GW_b[] = "\r\
IP of Gateway: ";
	SendHTML_Block(pxClient, str_curr_GW_b,
			sizeof(str_curr_GW_b) - 1);
	if(settings.currIP_Addr_GW)
	{
		memset(tmpStr, 0x00, HTML_NETWORK_SETTINGS_TMP_BUF_LEN);
		FreeRTOS_inet_ntoa(settings.currIP_Addr_GW, tmpStr);
		SendHTML_Block(pxClient, tmpStr,
				GetSizeOfStr(tmpStr, HTML_NETWORK_SETTINGS_TMP_BUF_LEN));
	}
	
	/* Send DNS server address */
	static const char str_curr_DNS_b[] = "\r\
IP of DNS:     ";
	SendHTML_Block(pxClient, str_curr_DNS_b,
			sizeof(str_curr_DNS_b) - 1);
	if(settings.currIP_Addr_DNS)
	{
		memset(tmpStr, 0x00, HTML_NETWORK_SETTINGS_TMP_BUF_LEN);
		FreeRTOS_inet_ntoa(settings.currIP_Addr_DNS, tmpStr);
		SendHTML_Block(pxClient, tmpStr,
				GetSizeOfStr(tmpStr, HTML_NETWORK_SETTINGS_TMP_BUF_LEN));
	}
	
	/* Send MAC address */
	static const char str_curr_MAC_b[] = "\r\
MAC address:   ";
	SendHTML_Block(pxClient, str_curr_MAC_b,
			sizeof(str_curr_MAC_b) - 1);
#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
	const uint8_t* pMAC_Addr = settings.MAC_Addr;
#else /*undefined ALLOW_MAC_ADDRESS_OVERRIDE*/
	const uint8_t* pMAC_Addr = WebServerGetMAC_Addr();
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

	for(uint8_t i = 0;;)
	{
		/* Send MAC octet */
		SetHexToStr(*pMAC_Addr, tmpStr, 3, false);
		SendHTML_Block(pxClient, tmpStr,
				GetSizeOfStr(tmpStr, HTML_NETWORK_SETTINGS_TMP_BUF_LEN));
		i++;
		if(i >= ipMAC_ADDRESS_LENGTH_BYTES) break;
		pMAC_Addr++;

		/* Send colon */
		SendHTML_Block(pxClient, ":", sizeof(":") - 1);
	}

	/* Send radio-buttons to select DHCP or static network settings */
	static const char str_dhcp_b[] = "\r\r\
<input name=\"t_nw_st\" id=\"DHCP\" type=\"radio\" value=\"dn\"";
	static const char str_dhcp_1[] = "\
 onclick=\"ch_DHCP()\"> Obtain an network settings\
 automatically from DHCP server";
	SendHTML_Block(pxClient, str_dhcp_b, sizeof(str_dhcp_b) - 1);
	if(settings.protocolType == DHCP)
		SendHTML_Block(pxClient, " checked", sizeof(" checked") - 1);
	SendHTML_Block(pxClient, str_dhcp_1, sizeof(str_dhcp_1) - 1);

	static const char str_static_b[] = "\r\
<input name=\"t_nw_st\" id=\"STAT\" type=\"radio\" value=\"st\"";
	static const char str_static_1[] = "\
 onclick=\"ch_DHCP()\"> Use the following network settings:";
	SendHTML_Block(pxClient, str_static_b, sizeof(str_static_b) - 1);
	if(settings.protocolType != DHCP)
		SendHTML_Block(pxClient, " checked", sizeof(" checked") - 1);
	SendHTML_Block(pxClient, str_static_1, sizeof(str_static_1) - 1);

	/* Send the static network settings */
	/* Send the static IP */
	static const char str_static_IP_b[] = "\r\
IP-address:    <input name=\"IP\" id=\"IP\" value=\"";
	static const char str_static_IP_1[] = "\">";
	SendHTML_Block(pxClient, str_static_IP_b,
			sizeof(str_static_IP_b) - 1);
	memset(tmpStr, 0x00, HTML_NETWORK_SETTINGS_TMP_BUF_LEN);
	FreeRTOS_inet_ntoa(settings.staticIP_Addr, tmpStr);
	SendHTML_Block(pxClient, tmpStr,
			GetSizeOfStr(tmpStr, HTML_NETWORK_SETTINGS_TMP_BUF_LEN));
	SendHTML_Block(pxClient, str_static_IP_1,
			sizeof(str_static_IP_1) - 1);
	
	/* Send the static mask */
	static const char str_static_msk_b[] = "\r\
Netmask:       <input name=\"msk\" id=\"msk\" value=\"";
	static const char str_static_msk_1[] = "\">";
	SendHTML_Block(pxClient, str_static_msk_b,
			sizeof(str_static_msk_b) - 1);
	memset(tmpStr, 0x00, HTML_NETWORK_SETTINGS_TMP_BUF_LEN);
	FreeRTOS_inet_ntoa(settings.staticNetmask, tmpStr);
	SendHTML_Block(pxClient, tmpStr,
			GetSizeOfStr(tmpStr, HTML_NETWORK_SETTINGS_TMP_BUF_LEN));
	SendHTML_Block(pxClient, str_static_msk_1,
			sizeof(str_static_msk_1) - 1);
	
	/* Send the static GW */
	static const char str_static_GW_b[] = "\r\
Gateway:       <input name=\"GW\" id=\"GW\" value=\"";
	static const char str_static_GW_1[] = "\">";
	SendHTML_Block(pxClient, str_static_GW_b,
			sizeof(str_static_GW_b) - 1);
	if(settings.staticIP_Addr_GW)
	{
		memset(tmpStr, 0x00, HTML_NETWORK_SETTINGS_TMP_BUF_LEN);
		FreeRTOS_inet_ntoa(settings.staticIP_Addr_GW, tmpStr);
		SendHTML_Block(pxClient, tmpStr,
				GetSizeOfStr(tmpStr, HTML_NETWORK_SETTINGS_TMP_BUF_LEN));
	}
	SendHTML_Block(pxClient, str_static_GW_1,
			sizeof(str_static_GW_1) - 1);

	/* Send the static DNS */
	static const char str_static_DNS_b[] = "\r\
DNS:           <input name=\"DNS\" id=\"DNS\" value=\"";
	static const char str_static_DNS_1[] = "\">";
	SendHTML_Block(pxClient, str_static_DNS_b,
			sizeof(str_static_DNS_b) - 1);
	if(settings.staticIP_Addr_DNS)
	{
		memset(tmpStr, 0x00, HTML_NETWORK_SETTINGS_TMP_BUF_LEN);
		FreeRTOS_inet_ntoa(settings.staticIP_Addr_DNS, tmpStr);
		SendHTML_Block(pxClient, tmpStr,
				GetSizeOfStr(tmpStr, HTML_NETWORK_SETTINGS_TMP_BUF_LEN));
	}
	SendHTML_Block(pxClient, str_static_DNS_1,
			sizeof(str_static_DNS_1) - 1);
	
#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
	/* Send the radio-buttons to override or not the MAC ---------------------*/
	static const char str_mac_rb_b[] = "\r\r\
<input name=\"macRB\" id=\"MAC_ORIG\" type=\"radio\" value=\"mcu_id\"";
	static const char str_mac_rb_1[] = "\
 onclick=\"ch_MAC_RB()\"> Use the original MAC";
	SendHTML_Block(pxClient, str_mac_rb_b, sizeof(str_mac_rb_b) - 1);
	if(settings.MAC_AddrIsOvrd == 0)
		SendHTML_Block(pxClient, " checked", sizeof(" checked") - 1);
	SendHTML_Block(pxClient, str_mac_rb_1, sizeof(str_mac_rb_1) - 1);

	static const char str_mac_ovrd_b[] = "\r\
<input name=\"macRB\" id=\"MAC_OVRD\" type=\"radio\" value=\"ovrd\"";
	static const char str_mac_ovrd_1[] = "\
 onclick=\"ch_MAC_RB()\"> Override MAC with the following: ";
	SendHTML_Block(pxClient, str_mac_ovrd_b, sizeof(str_mac_ovrd_b) - 1);
	if(settings.MAC_AddrIsOvrd != 0)
		SendHTML_Block(pxClient, " checked", sizeof(" checked") - 1);
	SendHTML_Block(pxClient, str_mac_ovrd_1, sizeof(str_mac_ovrd_1) - 1);

	/* Send the current active MAC */
	static const char str_mac_ovrd_oct_1[] = "\
<input name=\"mac_ovrd\" id=\"MAC_ovrd\" size=\"17\" value=\"";
	static const char str_mac_ovrd_oct_2[] = "\">";
	SendHTML_Block(pxClient, str_mac_ovrd_oct_1,
			sizeof(str_mac_ovrd_oct_1) - 1);
	/* Send value */
	pMAC_Addr = settings.MAC_Addr;
	for(uint8_t i = 0;;)
	{
		/* Send MAC octet */
		SetHexToStr(*pMAC_Addr, tmpStr, 3, false);
		SendHTML_Block(pxClient, tmpStr,
				GetSizeOfStr(tmpStr, HTML_NETWORK_SETTINGS_TMP_BUF_LEN));

		i++;
		if(i >= ipMAC_ADDRESS_LENGTH_BYTES) break;
		pMAC_Addr++;

		/* Send colon */
		SendHTML_Block(pxClient, ":", sizeof(":") - 1);
	}
	SendHTML_Block(pxClient, str_mac_ovrd_oct_2,
			sizeof(str_mac_ovrd_oct_2) - 1);
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

	/* Login and password for UI ---------------------------------------------*/
	/* Send login */
	static const char str_login_b[] = "\r\r\
Login and password for accessing to web-UI (max 16 characters).\r\
Empty login or password are not allowed.\r\
Login:         ";
	SendHTML_Block(pxClient, str_login_b, sizeof(str_login_b) - 1);
	SendInput(pxClient, false, false,
			"login", sizeof("login") - 1,
			settings.login,
			GetSizeOfStr(settings.login,
					HTML_NETWORK_SETTINGS_TMP_BUF_LEN), 16);

	/* Send password */
	static const char str_passw_b[] = "\r\
Password:      ";
	SendHTML_Block(pxClient, str_passw_b, sizeof(str_passw_b) - 1);
	SendInput(pxClient, false, false,
			"passw", sizeof("passw") - 1,
			settings.password,
			GetSizeOfStr(settings.password,
					HTML_NETWORK_SETTINGS_TMP_BUF_LEN), 16);

	/* Free temporary string buffer */
	vPortFree(tmpStr);
	
	/* Send end of html page */
	SendHTML_Block(pxClient, str_end, sizeof(str_end) - 1);
	return Send_HTML_End(pxClient);
}
