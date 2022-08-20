/* HTTP server HTML-pages aggregator */

/* Preincludes */
#include "settings.h"

/* Includes ------------------------------------------------------------------*/
/* Include html headers */
/* Commons HTMLs */
#include "robots.h"
#include "404.h"
#include "HTML_Login.h"
#include "HTML_NetworkSettings.h"

/* Home and applications HTMLs */
#include "HTML_Main.h"
#include "HTML_SyncSettings.h"
#include "HTML_DateTimeSettings.h"
#include "HTML_ServiceSettings.h"

/* Application includes */
#include "httpserver-netconn.h"

/* Public functions ----------------------------------------------------------*/
BaseType_t prvOpenURL(HTTPClient_t *pxClient)
{
	/* Try to parse HTTP GET command */
	BaseType_t xResult = 0;

	/* At first, service all requests without logging */
	/* Analyze for robot requests */
	xResult = Parse_robots(pxClient);
	if(xResult != 0) return xResult;

#ifndef DISABLE_WEB_UI_LOGIN
	/* At second, try to get login state */
	xResult = HTML_Login(pxClient);
	if(xResult != 0) return xResult;
#endif /*DISABLE_WEB_UI_LOGIN*/

	xResult = Parse_HTML_Main(pxClient);
	if(xResult != 0) return xResult;

	xResult = Parse_HTML_DateTimeSettings(pxClient);
	if(xResult != 0) return xResult;

	xResult = Parse_HTML_SyncSettings(pxClient);
	if(xResult != 0) return xResult;

	xResult = Parse_HTML_NetworkSettings(pxClient);
	if(xResult != 0) return xResult;

	xResult = Parse_HTML_ServiceSettings(pxClient);
	if(xResult != 0) return xResult;

	/* Wrong or unknown HTTP GET command */
	return Send_404(pxClient);
}
