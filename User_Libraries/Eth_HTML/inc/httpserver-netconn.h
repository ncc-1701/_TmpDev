#ifndef __HTTPSERVER_NETCONN_H__
#define __HTTPSERVER_NETCONN_H__

/* Includes ----------------------------------------------------------------- */
/* Standard includes */
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+TCP includes */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* FreeRTOS Protocol includes */
#include "FreeRTOS_TCP_server.h"
#include "FreeRTOS_server_private.h"

/* Application includes */
#include "html_txt_funcs.h"

/* Public function prototypes ----------------------------------------------- */
void HTTP_ServerInit();
void HTTP_ServerApplyNetworkSettingsAfterNetConnClose();
BaseType_t prvOpenURL(HTTPClient_t *pxClient);
BaseType_t SendHTML_Block(HTTPClient_t *pxClient,
		const void *pvBuffer, size_t uxDataLength);
BaseType_t SendHTML_Header_OK(HTTPClient_t *pxClient);
BaseType_t SendHTML_Header_RedirectToRoot(HTTPClient_t *pxClient,
		char* key, uint16_t lenght);
BaseType_t SendHTML_Header_404(HTTPClient_t *pxClient);

#endif /*__HTTPSERVER_NETCONN_H__*/
