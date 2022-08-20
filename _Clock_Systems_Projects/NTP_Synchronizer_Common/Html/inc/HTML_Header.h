#ifndef _HTML_HEADER_H_
#define _HTML_HEADER_H_

// Includes --------------------------------------------------------------------
#include "httpserver-netconn.h"

// Public function prototypes --------------------------------------------------
BaseType_t Send_HTML_Header(HTTPClient_t *pxClient,
							const char* formName,
							const char* onLoadScrpt,
							uint16_t autoupdatePeriod);
BaseType_t Send_HTML_End(HTTPClient_t *pxClient);
BaseType_t Send_HTML_EndWithJS(HTTPClient_t *pxClient, const char* js);
BaseType_t Send_HTML_EndWithServiceSettingsHint(HTTPClient_t *pxClient);

#endif // _HTML_HEADER_H_
