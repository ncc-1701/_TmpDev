// Includes --------------------------------------------------------------------
#include "html_txt_funcs.h"
#include "robots.h"

// Variables -------------------------------------------------------------------
// Parsed html
static const char str_0[] = "\
User-agent: *\r\
Disallow: /";

// Private function prototypes -------------------------------------------------
static BaseType_t Send_HTML(HTTPClient_t *pxClient);

// Public functions ------------------------------------------------------------
BaseType_t Parse_robots(HTTPClient_t *pxClient)
{
	const char* buf = pxClient->pcUrlData;
	if(QueryCmp(&buf, "/robots.txt") == pdFALSE) return pdFALSE;
	return Send_HTML(pxClient);
}

// Private functions -----------------------------------------------------------
static BaseType_t Send_HTML(HTTPClient_t *pxClient)
{
	// Send html header
	SendHTML_Header_OK(pxClient);

	SendHTML_Block(pxClient, str_0, sizeof(str_0) - 1);
	
	// Send last empty block
	return SendHTML_Block(pxClient, "", 0);
}
