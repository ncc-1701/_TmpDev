/* Includes ------------------------------------------------------------------*/
#include "settings.h"
#include "web-server.h"
#include "html_txt_funcs.h"
#include "HTML_Header.h"
#include "404.h"
#include "HTML_Login.h"

/* Private constants ---------------------------------------------------------*/
#define HTML_LOGIN_TMP_BUF_LEN 16

#ifndef MAX_LOGINS_HTTP_CLIENTS
#	define MAX_LOGINS_HTTP_CLIENTS 		16
#endif /*MAX_LOGINS_HTTP_CLIENTS*/

/* Structures definitions ----------------------------------------------------*/
/* Structure member for keep client connection */
struct __attribute__ ((__packed__)) HTTP_ClientConnection
{
	uint32_t IP_Addr;
	//uint16_t port;
	uint16_t ID;
};

/* Variables -----------------------------------------------------------------*/
/* Parsed HTML for entry login and password */
static const char str_lgn_psswrd_h[] = "\
<!DOCTYPE html PUBLIC \"HTML 4.01 Transitional/EN\">\n\
<html>\n\
<head>\n\
  <meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\">\n\
  <title>HTML_Login</title>\n\
  <meta http-equiv=\"cache-control\" content=\"max-age=0\">\n\
  <meta http-equiv=\"cache-control\" content=\"no-cache\">\n\
  <meta http-equiv=\"expires\" content=\"0\">\n\
  <meta http-equiv=\"expires\" content=\"Tue, 01 Jan 1980 1:00:00 GMT\">\n\
  <meta http-equiv=\"pragma\" content=\"no-cache\">\n\
  <script type=\"text/javascript\">\n\
  function stopRKey(evt)\n\
  {\n\
    var evt = (evt) ? evt : ((event) ? event : null);\n\
    var node = (evt.target) ? evt.target : \
((evt.srcElement) ? evt.srcElement : null);\n\
    if ((evt.keyCode == 13) && (node.type==\"text\"))  {return false;}\n\
  }\n\
  document.onkeypress = stopRKey;\n\
  </script>\n\
</head>\n\
<body>\n\
<form action=\"/login\">\n";
static const char str_lgn_psswrd_b[] = "\
  <font size=\"+2\">\n\
<pre>\n\
<p><strong>\
Login:    </strong>\
<input maxlength=\"16\" size=\"20\" name=\"login\"></p>\
<p><strong>\
Password: </strong>\
<input type=\"password\" maxlength=\"16\" size=\"20\" name=\"password\"></p>\
          <input name=\"Login\" value=\"Login\" type=\"submit\">\
</pre>\n\
  </font>\n";

/* Redirect HTML */
static const char str_redirect_to_main_b[] = "\
<html>\n\
<head>\n\
<title>Redirect to main page after 2 seconds</title>\n\
<meta http-equiv=\"refresh\" content=\"2; URL=;key=";
static const char str_redirect_to_main_1[] = "/\">\n\
<meta name=\"keywords\" content=\"automatic redirection\">\n\
</head>\n\
<body>\n\
Login is successful!<br>\
If your browser doesn't support automatically redirect,<br>\n\
please, visit the home page manually:<br><br>\n\
  <font size=\"+2\">\n\
  <a href=\";key=";
static const char str_redirect_to_main_e[] = "/\">Home</a>\n\
</font>\n\
</body>\n\
</html>";

/* Variables for keeping clients' connection */
static struct HTTP_ClientConnection clientConnections[MAX_LOGINS_HTTP_CLIENTS];
static uint16_t pAppendingClientConnections = 0;

/* Other variables */
static volatile uint16_t package_ID = 0;

/* Private function prototypes -----------------------------------------------*/
static BaseType_t HTML_LoginRequest(HTTPClient_t *pxClient, const char** pBuf);
static BaseType_t Send_HTML(HTTPClient_t *pxClient);
static BaseType_t Send_HTML_Redirect(HTTPClient_t *pxClient, uint16_t key);

/* Public functions ----------------------------------------------------------*/
BaseType_t HTML_Login(HTTPClient_t *pxClient)
{
	BaseType_t xResult = 0;
	const char** pBuf = &pxClient->pcUrlData;

	/* At first, check for "login" request */
	if(QueryCmp(pBuf, "/login"))
	{
		/* Proceed login with password */
		xResult = HTML_LoginRequest(pxClient, pBuf);
		if(xResult != 0) return xResult;
		return Send_HTML(pxClient);
	}

	/* Redirect to login form from root directory */
	if(QueryCmp(pBuf, "/") || QueryCmp(pBuf, "/HTML_Main.html"))
	{
		/* Redirect to login form */
		return Send_HTML(pxClient);
	}

	/* Search for "key" symbol (";") */
	bool foundKey = false;
	for(uint16_t i = 0; i < 1000; i++)
	{
		if((**pBuf == 0) || (**pBuf == ' '))
		{
			/* End or brake of string */
			break;
		}
		if(**pBuf == ';')
		{
			(*pBuf)++;

			/* Found "key" symbol */
			foundKey = true;
			break;
		}
		(*pBuf)++;
	}

	if(foundKey == false)
	{
		/* Did not find "key" symbol, send 404 form */
		return Send_404(pxClient);
	}

	/* Create temporarily variables */
	int32_t tmp32;
	struct freertos_sockaddr pxAddress;
	pxAddress.sin_addr = 0;
	pxAddress.sin_port = 0;

	/* Try to get the key and compare it with stored one */
	if(ParamIsEqu(pBuf, "key") == false)
	{
		/* There is no "key" parameter */
		return Send_HTML(pxClient);
	}

	/* Try to convert string to number */
	if(GetNumFromStr(pBuf, &tmp32, pdTRUE))
	{
		if((tmp32 >= 0) && (tmp32 <= 0xFFFF))
		{
			/* Validate key with existing clients */
			FreeRTOS_GetRemoteAddress(pxClient->xSocket, &pxAddress);
			if(pxAddress.sin_addr/* && (pxAddress.sin_port)*/)
			{
				for(uint8_t i = 0; i < MAX_LOGINS_HTTP_CLIENTS; i++)
				{
					/* Check IP address */
					if(clientConnections[i].IP_Addr == pxAddress.sin_addr)
					{
						/* Check port */
						//if(clientConnections[i].port == pxAddress.sin_port)
						{
							/* Check key (IP) */
							if(clientConnections[i].ID == tmp32)
							{
								/* Key is valid,
								search for "address" symbol ("/") */
								for(uint16_t i = 0; i < 1000; i++)
								{
									if((**pBuf == 0) ||(**pBuf == ' '))
									{
										/* End or brake of string, it impossible
										to parse.
										   Redirect to login form */
										return Send_HTML(pxClient);
									}
									if(**pBuf == '/')
									{
										/* Found "address" symbol.
										   Let another parser try to get
										cropped the GET request */
										return pdFALSE;
									}
									(*pBuf)++;
								}
							}
						}
					}
				}

				/* If any, try to login with password */
				xResult = HTML_LoginRequest(pxClient, pBuf);
				if(xResult != 0) return xResult;
			}
		}
	}

	/* Wrong or empty key, redirect to login form */
	return Send_HTML(pxClient);
}

/* Function to reset authorization keys */
void ResetAuthKeys()
{
	for(uint8_t i = 0; i < MAX_LOGINS_HTTP_CLIENTS; i++)
	{
		clientConnections[i].IP_Addr = 0;
		//clientConnections[i].port = 0;
		clientConnections[i].ID = 0;
	}
}

/* Private functions ---------------------------------------------------------*/
static BaseType_t HTML_LoginRequest(HTTPClient_t *pxClient, const char** pBuf)
{
	static bool firstKey = true;

	/* Create temporarily variables */
	char tmpValStr[32];

	/* Check login and password */
	BaseType_t login = pdFALSE;
	if(SearchForStartParameter(pBuf))
	{
		while(pdTRUE)
		{
			/* Parse parameters */
			/* Search login field */
			if(ParamIsEqu(pBuf, "login"))
			{
				/* Get value as string */
				GetValue(*pBuf, tmpValStr, sizeof(tmpValStr));

				/* Compare with login */
				if(ValueCmp(*pBuf, GetLogin()) == false)
				{
					/* Wrong login, send html to client */
					return Send_HTML(pxClient);
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(pBuf) == false) break;
			}

			/* Search password field */
			if(ParamIsEqu(pBuf, "password"))
			{
				/* Get value as string */
				GetValue(*pBuf, tmpValStr, sizeof(tmpValStr));

				/* Compare with password */
				if(ValueCmp(*pBuf, GetPassword()) == false)
				{
					/* Wrong password, send html to client */
					return Send_HTML(pxClient);
				}

				/* Watch for end of parameters */
				if(SearchForNextParameter(pBuf) == false) break;
			}

			/* Login button */
			if(ParamIsEqu(pBuf, "Login"))
			{
				if(ValueCmp(*pBuf, "Login")) login = pdTRUE;
			}

			/* End of parsing */
			break;
		}
	}

	if(login == pdFALSE)
	{
		/* Unexpected end, return pdFALSE for redirecting to logging form */
		return pdFALSE;
		/* Unexpected end, send 404 form */
		//return Send_404(pxClient);
	}

	/* Store new key for new client */
	struct freertos_sockaddr pxAddress;
	pxAddress.sin_addr = 0;
	pxAddress.sin_port = 0;

	FreeRTOS_GetRemoteAddress(pxClient->xSocket, &pxAddress);
	if((pxAddress.sin_addr == 0) || (pxAddress.sin_port == 0))
	{
		/* Something wrong with sockets,
		   let client will try to login once again */
		return Send_HTML(pxClient);
	}

	/* Before adding new connection, try to search it from existing */
	bool found = false;
	for(uint8_t i = 0; i < MAX_LOGINS_HTTP_CLIENTS; i++)
	{
		/* Check IP address */
		if(clientConnections[i].IP_Addr == pxAddress.sin_addr)
		{
			/* Check port */
			//if(clientConnections[i].port == pxAddress.sin_port)
			{
				/* Connection already stored, just change key */
				clientConnections[i].ID = package_ID;
				found = true;
			}
		}
	}

	if(found == false)
	{
		/* First key must be random */
		if(firstKey)
		{
			package_ID = (uint16_t)(xTaskGetTickCount() & 0xFFFF);
			firstKey = false;
		}
		else
		{
			/* Make new ID */
			package_ID++;
		}

		/* Create and store new connection */
		clientConnections[pAppendingClientConnections].IP_Addr =
				pxAddress.sin_addr;
		/*clientConnections[pAppendingClientConnections].port =
				pxAddress.sin_port;*/
		clientConnections[pAppendingClientConnections].ID = package_ID;
		pAppendingClientConnections++;
		if(pAppendingClientConnections >= MAX_LOGINS_HTTP_CLIENTS)
			pAppendingClientConnections = 0;
	}

	/* Redirect to main page */
	return Send_HTML_Redirect(pxClient, package_ID);
}

static BaseType_t Send_HTML(HTTPClient_t *pxClient)
{
	/* Attempt to create the temporary string buffer. */
	char* tmpStr = (char*)pvPortMalloc(HTML_LOGIN_TMP_BUF_LEN);
	if(tmpStr == NULL)
	{
		FreeRTOS_printf(("Could not create temporary string buffer\n"));
		return -1;
	}

	/* Send html header */
	SendHTML_Header_OK(pxClient);

	SendHTML_Block(pxClient, str_lgn_psswrd_h, sizeof(str_lgn_psswrd_h) - 1);

#ifdef DEVICE_NAME
	/* Set font size and enable "pre" tag */
	static const char device_name_b[] = "\
  <font size=\"+1\">\r\
<pre>";
	SendHTML_Block(pxClient, device_name_b, sizeof(device_name_b) - 1);
	
	/* Send device name */
	SendHTML_Block(pxClient, DEVICE_NAME, sizeof(DEVICE_NAME) - 1);

	/* Add device MAC address */
	static const char str_curr_MAC_b[] = " (MAC address: ";
	SendHTML_Block(pxClient, str_curr_MAC_b,
			sizeof(str_curr_MAC_b) - 1);
	const uint8_t* pMAC_Addr = WebServerGetMAC_Addr();
	for(uint8_t i = 0;;)
	{
		/* Send MAC octet */
		SetHexToStr(*pMAC_Addr, tmpStr, 3, false);
		SendHTML_Block(pxClient, tmpStr,
				GetSizeOfStr(tmpStr, HTML_LOGIN_TMP_BUF_LEN));

		i++;
		if(i >= ipMAC_ADDRESS_LENGTH_BYTES) break;
		pMAC_Addr++;

		/* Send colon */
		SendHTML_Block(pxClient, ":", sizeof(":") - 1);
	}
	SendHTML_Block(pxClient, ")", sizeof(")") - 1);

	/* Add new string and disable "pre" tag */
	static const char device_name_e[] = "<br>\r</pre>\n\
  </font>\r";
	SendHTML_Block(pxClient, device_name_e, sizeof(device_name_e) - 1);
#endif /*DEVICE_NAME*/

	SendHTML_Block(pxClient, str_lgn_psswrd_b, sizeof(str_lgn_psswrd_b) - 1);

	/* Free temporary string buffer */
	vPortFree(tmpStr);

	return Send_HTML_End(pxClient);
}

static BaseType_t Send_HTML_Redirect(HTTPClient_t *pxClient, uint16_t key)
{
	/* Attempt to create the temporary string buffer. */
	char* tmpStr = (char*)pvPortMalloc(HTML_LOGIN_TMP_BUF_LEN);
	if(tmpStr == NULL)
	{
		FreeRTOS_printf(("Could not create temporary string buffer\n"));
		return -1;
	}

	/* Form key string */
	SetNumToStr(key, tmpStr, HTML_LOGIN_TMP_BUF_LEN);

	/* Send html header */
	//SendHTML_Header_OK(pxClient);
	SendHTML_Header_RedirectToRoot(pxClient,
			tmpStr, GetSizeOfStr(tmpStr, HTML_LOGIN_TMP_BUF_LEN));

	SendHTML_Block(pxClient, str_redirect_to_main_b,
			sizeof(str_redirect_to_main_b) - 1);
	SendHTML_Block(pxClient, tmpStr,
					GetSizeOfStr(tmpStr, HTML_LOGIN_TMP_BUF_LEN));
	SendHTML_Block(pxClient, str_redirect_to_main_1,
			sizeof(str_redirect_to_main_1) - 1);
	SendHTML_Block(pxClient, tmpStr,
					GetSizeOfStr(tmpStr, HTML_LOGIN_TMP_BUF_LEN));
	SendHTML_Block(pxClient, str_redirect_to_main_e,
			sizeof(str_redirect_to_main_e) - 1);

	/* Free temporary string buffer */
	vPortFree(tmpStr);

	/* Send last empty block */
	return SendHTML_Block(pxClient, "", 0);
}
