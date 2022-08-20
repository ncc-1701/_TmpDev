/* HTTP-server implementation for FreeRTOS + TCP */

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* FreeRTOS Protocol includes. */
#include "FreeRTOS_HTTP_commands.h"
#include "FreeRTOS_TCP_server.h"
#include "FreeRTOS_server_private.h"

/* Remove the whole file if HTTP is not supported. */
#if(ipconfigUSE_HTTP == 1)

#if (configUSE_FAT != 0)
	/* FreeRTOS+FAT includes. */
	#include "ff_stdio.h"
#endif /*(configUSE_FAT != 0)*/

/* Application includes. */
#include "httpserver-netconn.h"

/* Constants -----------------------------------------------------------------*/
/* Time constants */
#ifndef HTTP_SHUTDOWN_DELAY
	#define HTTP_SHUTDOWN_DELAY 		3000
#endif // HTTP_SHUTDOWN_DELAY

#ifndef HTTP_WAIT_FOR_SOCKET_CLOSING_DELAY
	#define HTTP_WAIT_FOR_SOCKET_CLOSING_DELAY 2000
#endif /*HTTP_WAIT_FOR_SOCKET_CLOSING_DELAY*/

#ifndef HTTP_ATTACK_BLOCK_TIMEOUT
	#define HTTP_ATTACK_BLOCK_TIMEOUT 	1000
#endif // HTTP_ATTACK_BLOCK_TIMEOUT

#ifndef HTTP_SERVER_BACKLOG
	#define HTTP_SERVER_BACKLOG			(12)
#endif

#if !defined(ARRAY_SIZE)
	#define ARRAY_SIZE(x) 				(BaseType_t)(sizeof(x)/sizeof(x)[0])
#endif

/* Some defines to make the code more readable */
#define pcCOMMAND_BUFFER				pxClient->pxParent->pcCommandBuffer
#define pcNEW_DIR						pxClient->pxParent->pcNewDir
#define pcFILE_BUFFER					pxClient->pxParent->pcFileBuffer

#ifndef ipconfigHTTP_REQUEST_CHARACTER
	#define ipconfigHTTP_REQUEST_CHARACTER '?'
#endif

/* Debug options -------------------------------------------------------------*/
//#define DEBUG_HTTP_SEND_NEG_RESULT

/* Structures definitions --------------------------------------------------- */
typedef struct xTYPE_COUPLE
{
	const char *pcExtension;
	const char *pcType;
} TypeCouple_t;

#if (configUSE_FAT != 0)
static TypeCouple_t pxTypeCouples[] =
{
	{ "html", "text/html" },
	{ "css",  "text/css" },
	{ "js",   "text/javascript" },
	{ "png",  "image/png" },
	{ "jpg",  "image/jpeg" },
	{ "gif",  "image/gif" },
	{ "txt",  "text/plain" },
	{ "mp3",  "audio/mpeg3" },
	{ "wav",  "audio/wav" },
	{ "flac", "audio/ogg" },
	{ "pdf",  "application/pdf" },
	{ "ttf",  "application/x-font-ttf" },
	{ "ttc",  "application/x-font-ttf" }
};
#endif /*(configUSE_FAT != 0)*/

/* Variables -----------------------------------------------------------------*/
static BaseType_t applyNetWorkSettingsAfterNetConnClose = pdFALSE;
static const char pcEmptyString[1] = {'\0'};

/* Private function prototypes -----------------------------------------------*/
/*_RB_ Need comment block, although fairly self evident. */
#if (configUSE_FAT != 0)
static void prvFileClose(HTTPClient_t *pxClient);
#endif /*(configUSE_FAT != 0)*/

static BaseType_t prvProcessCmd(HTTPClient_t *pxClient, BaseType_t xIndex);

#if (configUSE_FAT != 0)
static const char *pcGetContentsType(const char *apFname);
#endif /*(configUSE_FAT != 0)*/

static BaseType_t prvOpenURL_Internal(HTTPClient_t *pxClient);

#if (configUSE_FAT != 0)
static BaseType_t prvSendFile(HTTPClient_t *pxClient);
#endif /*(configUSE_FAT != 0)*/

static BaseType_t prvSendReply(HTTPClient_t *pxClient, BaseType_t xCode,
		BaseType_t chunked);

static BaseType_t CheckForAllowTCP_Transmission();
static void UpdateTCP_TransmissionTimeout(BaseType_t xRc);
static void ResetTCP_TransmissionTimeout();
static BaseType_t FreeRTOS_SendWithWaiting(Socket_t xSocket,
		const void *pvBuffer, size_t uxDataLength);

/* Public functions ----------------------------------------------------------*/
BaseType_t xHTTPClientWork(TCPClient_t *pxTCPClient)
{
BaseType_t xRc;
HTTPClient_t *pxClient = (HTTPClient_t *) pxTCPClient;

#if (configUSE_FAT != 0)
	if(pxClient->pxFileHandle != NULL)
	{
		prvSendFile(pxClient);
	}
#endif /*(configUSE_FAT != 0)*/

	if(pxClient->xLastRecSuccessfulTime == 0)
	{
		/* It is new HTTP client: set receive successful time */
		pxClient->xLastRecSuccessfulTime = xTaskGetTickCount();

		/* Clear pcCommandBuffer (use it as flag for complete HTTP request) */
		pcCOMMAND_BUFFER[0] = '\0';
	}

	BaseType_t last_xRc = 0;
	if(pcCOMMAND_BUFFER[0] != '\0')
	{
		/* Previous HTTP request was not valid or is not complete:
		try to proceed receive. Search for end of "pcCommandBuffer" */
		for(last_xRc = 1;
			last_xRc < (BaseType_t)(sizeof(pcCOMMAND_BUFFER)); last_xRc++)
		{
			if(pcCOMMAND_BUFFER[last_xRc] == '\0') break;
		}
	}

	if((last_xRc != 0) && (last_xRc < (BaseType_t)(sizeof(pcCOMMAND_BUFFER))))
	{
		xRc = FreeRTOS_recv(pxClient->xSocket,
				(void *)(&pcCOMMAND_BUFFER[last_xRc]),
				sizeof(pcCOMMAND_BUFFER) - last_xRc, 0);

		if(xRc > 0)
		{
			/* Add last received data length */
			xRc += last_xRc;
		}
	}
	else
	{
		xRc = FreeRTOS_recv(pxClient->xSocket, (void *)pcCOMMAND_BUFFER,
				sizeof(pcCOMMAND_BUFFER), 0);
	}

	if(xRc > 0)
	{
	BaseType_t xIndex;
	const char *pcEndOfCmd;
	const struct xWEB_COMMAND *curCmd;
	char *pcBuffer = pcCOMMAND_BUFFER;

		/* Update last receive successful time and reset transmission timeout */
		pxClient->xLastRecSuccessfulTime = xTaskGetTickCount();
		ResetTCP_TransmissionTimeout();

		if(xRc < (BaseType_t) sizeof(pcCOMMAND_BUFFER))
		{
			pcBuffer[xRc] = '\0';
		}
		while(xRc && (pcBuffer[xRc - 1] == 13 || pcBuffer[xRc - 1] == 10))
		{
			pcBuffer[--xRc] = '\0';
		}
		pcEndOfCmd = pcBuffer + xRc;

		curCmd = xWebCommands;

		/* Pointing to "/index.html HTTP/1.1". */
		pxClient->pcUrlData = pcBuffer;

		/* Pointing to "HTTP/1.1". */
		pxClient->pcRestData = pcEmptyString;

		/* Last entry is "ECMD_UNK". */
		for(xIndex = 0; xIndex < WEB_CMD_COUNT - 1; xIndex++, curCmd++)
		{
		BaseType_t xLength;

			xLength = curCmd->xCommandLength;
			if(	(xRc >= xLength) &&
				(memcmp(curCmd->pcCommandName, pcBuffer, xLength) == 0))
			{
			char *pcLastPtr;

				pxClient->pcUrlData += xLength + 1;
				for(pcLastPtr = (char*)pxClient->pcUrlData;
					pcLastPtr < pcEndOfCmd; pcLastPtr++)
				{
					char ch = *pcLastPtr;
					if((ch == '\0') || (strchr("\n\r \t", ch) != NULL))
					{
						*pcLastPtr = '\0';
						pxClient->pcRestData = pcLastPtr + 1;
						break;
					}
				}
				break;
			}
		}

		if(xIndex < (WEB_CMD_COUNT - 1))
		{
			/* Check for valid HTTP request */
			if(pxClient->pcRestData == pcEmptyString)
			{
				/* HTTP request is not valid or is not complete:
				try to proceed receive */
				return xRc;
			}

			xRc = prvProcessCmd(pxClient, xIndex);
		}
	}
	else if(xRc < 0)
	{
		/* The connection will be closed and the client will be deleted. */
		FreeRTOS_printf(("xHTTPClientWork: rc = %ld\n", xRc));
	}

	/* Clear pcCommandBuffer (use it as flag for complete HTTP request) */
	pcCOMMAND_BUFFER[0] = '\0';

	/* Service zero FreeRTOS_recv return */
	if(xRc == 0)
	{
		/* Check for last receive successful time */
		if(xTaskGetTickCount() - pxClient->xLastRecSuccessfulTime >
					HTTP_SHUTDOWN_DELAY) xRc = (-1);

		/* Check for allowed transmission before the sending of data */
		if(CheckForAllowTCP_Transmission() == pdFALSE) xRc = (-1);

		if(xRc < 0)
		{
			/* Reset receive successful time */
			pxClient->xLastRecSuccessfulTime = 0;

			/* Send signal to close the socket */
			return (-1);
		}
	}

	return xRc;
}
/*-----------------------------------------------------------*/

void vHTTPClientDelete(TCPClient_t *pxTCPClient)
{
HTTPClient_t *pxClient = (HTTPClient_t *) pxTCPClient;

	/* This HTTP client stops, close / release all resources. */
	if(pxClient->xSocket != FREERTOS_NO_SOCKET)
	{
		FreeRTOS_FD_CLR(pxClient->xSocket, pxClient->pxParent->xSocketSet,
				eSELECT_ALL);
		FreeRTOS_closesocket(pxClient->xSocket);
		pxClient->xSocket = FREERTOS_NO_SOCKET;
	}

#if (configUSE_FAT != 0)
	prvFileClose(pxClient);
#endif /*(configUSE_FAT != 0)*/

	if(applyNetWorkSettingsAfterNetConnClose)
	{
		/* Reinit net interface */
		void WebServerApplyNetworkSettings();
		WebServerApplyNetworkSettings();

		applyNetWorkSettingsAfterNetConnClose = pdFALSE;
	}
}
/*-----------------------------------------------------------*/

void HTTP_ServerApplyNetworkSettingsAfterNetConnClose()
{
	applyNetWorkSettingsAfterNetConnClose = pdTRUE;
}

BaseType_t SendHTML_Header_OK(HTTPClient_t *pxClient)
{
	return prvSendReply(pxClient, WEB_REPLY_OK, pdTRUE);
}

BaseType_t SendHTML_Header_RedirectToRoot(HTTPClient_t *pxClient,
		char* key, uint16_t lenght)
{
	char* pTmpStrPrc;
	char* pLastTmpStrPrc;
	uint16_t tmpStrFreeLng;

	/* Init pointers and length */
	pTmpStrPrc = pxClient->pxParent->pcExtraContents;
	pLastTmpStrPrc = pTmpStrPrc;
	tmpStrFreeLng = sizeof(pxClient->pxParent->pcExtraContents);
	if(key != NULL)
	{
		pTmpStrPrc = SetValue("Location: /;key=", pTmpStrPrc, tmpStrFreeLng);
		/* Correct pointers and length */
		pLastTmpStrPrc = pTmpStrPrc;
		tmpStrFreeLng -= (pLastTmpStrPrc - pTmpStrPrc);

		if(tmpStrFreeLng < lenght)
		{
			/* It is not enough string length */
			return -1;
		}

		pTmpStrPrc = SetValue(key, pTmpStrPrc, lenght);
		/* Correct pointers and length */
		pLastTmpStrPrc = pTmpStrPrc;
		tmpStrFreeLng -= (pLastTmpStrPrc - pTmpStrPrc);

		SetValue("/\r\n", pTmpStrPrc, tmpStrFreeLng);
	}
	else
	{
		/* Redirect to root directory without a key */
		SetValue("Location: /", pTmpStrPrc, tmpStrFreeLng);
	}

	return prvSendReply(pxClient, WEB_REDIRECT, pdTRUE);
}

BaseType_t SendHTML_Header_404(HTTPClient_t *pxClient)
{
	return prvSendReply(pxClient, WEB_NOT_FOUND, pdTRUE);
}

BaseType_t SendHTML_Block(HTTPClient_t *pxClient,
		const void *pvBuffer, size_t uxDataLength)
{
	/* Check for allowed transmission before the sending of data */
	if(CheckForAllowTCP_Transmission() == pdFALSE) return (-1);

	BaseType_t xRc;
	char tmpStr[5];

	if(uxDataLength == 0)
	{
		// Send last empty block
		xRc = FreeRTOS_SendWithWaiting(pxClient->xSocket,
				"0\r\n\r\n", sizeof("0\r\n\r\n") - 1);
		UpdateTCP_TransmissionTimeout(xRc);

		return xRc;
	}

	SetHexToStr(uxDataLength, tmpStr, sizeof(tmpStr), true);
	xRc = FreeRTOS_SendWithWaiting(pxClient->xSocket,
			tmpStr, GetSizeOfStr(tmpStr, sizeof(tmpStr)));
	if(xRc >= 0)
		xRc = FreeRTOS_SendWithWaiting(pxClient->xSocket,
				"\r\n", 	sizeof("\r\n") - 1);

	/* Send chunk data */
	if(xRc >= 0)
		xRc = FreeRTOS_SendWithWaiting(pxClient->xSocket,
				pvBuffer, uxDataLength);

	/* Send new line as end of chunk */
	if(xRc >= 0)
		xRc = FreeRTOS_SendWithWaiting(pxClient->xSocket,
				"\r\n", 	sizeof("\r\n") - 1);

	UpdateTCP_TransmissionTimeout(xRc);
	return xRc;
}

#if (configUSE_FAT != 0)
static void prvFileClose(HTTPClient_t *pxClient)
{
	if(pxClient->pxFileHandle != NULL)
	{
		FreeRTOS_printf(("Closing file: %s\n", pxClient->pcCurrentFilename));
		ff_fclose(pxClient->pxFileHandle);
		pxClient->pxFileHandle = NULL;
	}
}
#endif /*(configUSE_FAT != 0)*/
/*-----------------------------------------------------------*/

__attribute__((weak)) void WebServerApplyNetworkSettings() {}

/* Private functions ---------------------------------------------------------*/
static BaseType_t prvSendReply(HTTPClient_t *pxClient, BaseType_t xCode,
		BaseType_t chunked)
{
struct xTCP_SERVER *pxParent = pxClient->pxParent;
BaseType_t xRc;

	/* A normal command reply on the main socket (port 21). */
	char *pcBuffer = pxParent->pcFileBuffer;

	if(chunked == pdFALSE)
	{
		xRc = snprintf(pcBuffer, sizeof(pxParent->pcFileBuffer),
			"HTTP/1.1 %d %s\r\n"
			"Content-Type: %s\r\n"
			"Connection: Keep-Alive\r\n"
			"Keep-Alive: timeout=10, max=20\r\n"
			"Allow: GET\r\n"
			"Cache-Control: no-cache\r\n"
			"Expires: 0\r\n"
			"%s\r\n",
			(int) xCode,
			webCodename (xCode),
			pxParent->pcContentsType[0] ?
					pxParent->pcContentsType : "text/html",
			pxParent->pcExtraContents);
	}
	else
	{
		xRc = snprintf(pcBuffer, sizeof(pxParent->pcFileBuffer),
			"HTTP/1.1 %d %s\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Content-Type: %s\r\n"
			"Connection: Keep-Alive\r\n"
			"Keep-Alive: timeout=10, max=20\r\n"
			"Allow: GET\r\n"
			"Cache-Control: no-cache\r\n"
			"Expires: 0\r\n"
			"%s\r\n",
			(int) xCode,
			webCodename (xCode),
			pxParent->pcContentsType[0] ?
					pxParent->pcContentsType : "text/html",
			pxParent->pcExtraContents);
	}

	pxParent->pcContentsType[0] = '\0';
	pxParent->pcExtraContents[0] = '\0';

	xRc = FreeRTOS_SendWithWaiting(pxClient->xSocket,
			(const void *) pcBuffer, xRc);
	pxClient->bits.bReplySent = pdTRUE_UNSIGNED;

	UpdateTCP_TransmissionTimeout(xRc);

	return xRc;
}
/*-----------------------------------------------------------*/

#if (configUSE_FAT != 0)
static BaseType_t prvSendFile(HTTPClient_t *pxClient)
{
/*size_t uxSpace;*/
size_t uxCount;
BaseType_t xRc = 0;

	if(pxClient->bits.bReplySent == pdFALSE_UNSIGNED)
	{
		pxClient->bits.bReplySent = pdTRUE_UNSIGNED;

		strcpy(pxClient->pxParent->pcContentsType,
				pcGetContentsType(pxClient->pcCurrentFilename));
		snprintf(pxClient->pxParent->pcExtraContents,
				sizeof(pxClient->pxParent->pcExtraContents),
				"Content-Length: %d\r\n", (int) pxClient->uxBytesLeft);

		/* "Requested file action OK". */
		xRc = prvSendReply(pxClient, WEB_REPLY_OK, pdFALSE);
	}

	if(xRc >= 0) do
	{
		uxCount = pxClient->uxBytesLeft;
		if(uxCount == 0) break;

		if(uxCount > sizeof(pxClient->pxParent->pcFileBuffer))
		{
			uxCount = sizeof(pxClient->pxParent->pcFileBuffer);
		}
		ff_fread(pxClient->pxParent->pcFileBuffer, 1, uxCount,
				pxClient->pxFileHandle);
		pxClient->uxBytesLeft -= uxCount;

		xRc = FreeRTOS_SendWithWaiting(pxClient->xSocket,
				pxClient->pxParent->pcFileBuffer, uxCount);
		if(xRc < 0) break;
	} while(uxCount > 0u);

	if(pxClient->uxBytesLeft == 0u)
	{
		/* Writing is ready, no need for further 'eSELECT_WRITE' events. */
		FreeRTOS_FD_CLR(pxClient->xSocket,
				pxClient->pxParent->xSocketSet, eSELECT_WRITE);
		prvFileClose(pxClient);
	}
	else
	{
		/* Wake up the TCP task as soon as this socket may be written to. */
		FreeRTOS_FD_SET(pxClient->xSocket,
				pxClient->pxParent->xSocketSet, eSELECT_WRITE);
	}

	return xRc;
}
#endif /*(configUSE_FAT != 0)*/
/*-----------------------------------------------------------*/

static BaseType_t prvOpenURL_Internal(HTTPClient_t *pxClient)
{
BaseType_t xRc;

#if (configUSE_FAT != 0)
char pcSlash[2];
#endif /*(configUSE_FAT != 0)*/

	pxClient->bits.ulFlags = 0;

	#if(ipconfigHTTP_HAS_HANDLE_REQUEST_HOOK != 0)
	{
		if(strchr(pxClient->pcUrlData, ipconfigHTTP_REQUEST_CHARACTER) != NULL)
		{
		size_t xResult;

			xResult = uxApplicationHTTPHandleRequestHook(pxClient->pcUrlData,
					pxClient->pcCurrentFilename,
					sizeof(pxClient->pcCurrentFilename));
			if(xResult > 0)
			{
				strcpy(pxClient->pxParent->pcContentsType, "text/html");
				snprintf(pxClient->pxParent->pcExtraContents,
						sizeof(pxClient->pxParent->pcExtraContents),
						"Content-Length: %d\r\n", (int) xResult);

				/* "Requested file action OK" */
				xRc = prvSendReply(pxClient, WEB_REPLY_OK, pdFALSE);
				if(xRc > 0)
				{
					xRc = FreeRTOS_send(pxClient->xSocket,
							pxClient->pcCurrentFilename, xResult, 0);
				}
				/* Although against the coding standard of FreeRTOS, a return is
				done here  to simplify this conditional code. */
				return xRc;
			}
		}
	}
	#endif /* ipconfigHTTP_HAS_HANDLE_REQUEST_HOOK */

#if (configUSE_FAT != 0)
	if(pxClient->pcUrlData[0] != '/')
	{
		/* Insert a slash before the file name. */
		pcSlash[0] = '/';
		pcSlash[1] = '\0';
	}
	else
	{
		/* The browser provided a starting '/' already. */
		pcSlash[0] = '\0';
	}
	snprintf(
		pxClient->pcCurrentFilename, sizeof(pxClient->pcCurrentFilename),
		"%s%s%s",
		pxClient->pcRootDir,
		pcSlash,
		pxClient->pcUrlData);

	pxClient->pxFileHandle = ff_fopen(pxClient->pcCurrentFilename, "rb");

	FreeRTOS_printf(("Open file '%s': %s\n", pxClient->pcCurrentFilename,
		pxClient->pxFileHandle != NULL ? "Ok" : strerror(stdioGET_ERRNO())));

	if(pxClient->pxFileHandle == NULL)
#endif /*(configUSE_FAT != 0)*/
	{
		/* It is not file: try to open as HTML-page */
		xRc = prvOpenURL(pxClient);
	}
#if (configUSE_FAT != 0)
	else
	{
		pxClient->uxBytesLeft = (size_t) pxClient->pxFileHandle->ulFileSize;
		xRc = prvSendFile(pxClient);
	}
#endif /*(configUSE_FAT != 0)*/

	return xRc;
}
/*-----------------------------------------------------------*/

static BaseType_t prvProcessCmd(HTTPClient_t *pxClient, BaseType_t xIndex)
{
BaseType_t xResult = 0;

	/* A new command has been received. Process it. */
	switch(xIndex)
	{
	case ECMD_GET:
		xResult = prvOpenURL_Internal(pxClient);
		break;

	case ECMD_HEAD:
	case ECMD_POST:
	case ECMD_PUT:
	case ECMD_DELETE:
	case ECMD_TRACE:
	case ECMD_OPTIONS:
	case ECMD_CONNECT:
	case ECMD_PATCH:
	case ECMD_UNK:
		{
			FreeRTOS_printf(("prvProcessCmd: Not implemented: %s\n",
				xWebCommands[xIndex].pcCommandName));
		}
		break;
	}

	return xResult;
}
/*-----------------------------------------------------------*/

#if (configUSE_FAT != 0)
static const char *pcGetContentsType (const char *apFname)
{
	const char *slash = NULL;
	const char *dot = NULL;
	const char *ptr;
	const char *pcResult = "text/html";
	BaseType_t x;

	for(ptr = apFname; *ptr; ptr++)
	{
		if (*ptr == '.') dot = ptr;
		if (*ptr == '/') slash = ptr;
	}
	if(dot > slash)
	{
		dot++;
		for(x = 0; x < ARRAY_SIZE(pxTypeCouples); x++)
		{
			if(strcasecmp(dot, pxTypeCouples[x].pcExtension) == 0)
			{
				pcResult = pxTypeCouples[x].pcType;
				break;
			}
		}
	}
	return pcResult;
}
#endif /*(configUSE_FAT != 0)*/

static TickType_t xFreeRTOS_SendTimeOut = 0;
static BaseType_t CheckForAllowTCP_Transmission()
{
	TickType_t deltaTime = xTaskGetTickCount();
	deltaTime = xFreeRTOS_SendTimeOut - deltaTime;
	if(deltaTime == 0) return pdTRUE;
	if(deltaTime > HTTP_ATTACK_BLOCK_TIMEOUT) return pdTRUE;
	return pdFALSE;
}

static void UpdateTCP_TransmissionTimeout(BaseType_t xRc)
{
#ifdef DEBUG_HTTP_SEND_NEG_RESULT
	volatile uint16_t junks = 0;
#endif // DEBUG_HTTP_SEND_NEG_RESULT

	/* Catch some errors */
	if(xRc == (-pdFREERTOS_ERRNO_ENOTCONN))
	{
		/* Socket is not connected: update timeout anyway */
		xFreeRTOS_SendTimeOut = xTaskGetTickCount();
		return;
	}

	if(xRc < 0)
	{
#ifdef DEBUG_HTTP_SEND_NEG_RESULT
		junks++;
#endif // DEBUG_HTTP_SEND_NEG_RESULT

		/* Something wrong with transmission: set timeout for waiting */
		xFreeRTOS_SendTimeOut = xTaskGetTickCount() + HTTP_ATTACK_BLOCK_TIMEOUT;
		return;
	}

	/* No errors: update timeout */
	xFreeRTOS_SendTimeOut = xTaskGetTickCount();
}

static void ResetTCP_TransmissionTimeout()
{
	/* Update timeout */
	xFreeRTOS_SendTimeOut = xTaskGetTickCount();
}

static BaseType_t FreeRTOS_SendWithWaiting(Socket_t xSocket,
		const void *pvBuffer, size_t uxDataLength)
{
	size_t uxSpace;
	size_t uxCount;
	BaseType_t xRc = 0;
	TickType_t xTimeOut = xTaskGetTickCount();

	while(uxDataLength)
	{
		uxSpace = FreeRTOS_tx_space(xSocket);

		if(uxDataLength < uxSpace) uxCount = uxDataLength;
		else uxCount = uxSpace;

		/* Send "uxCount" bytes, even if "uxCount" is zero */
		xRc = FreeRTOS_send(xSocket, pvBuffer, uxCount, 0);

		if(xRc < 0) break;

		if(xRc == 0)
		{
			/* Perhaps socket is closing, check for timeout */
			if((xTaskGetTickCount() - xTimeOut) >=
					HTTP_WAIT_FOR_SOCKET_CLOSING_DELAY)
			{
				/* Time is out: do not try to send again */
				return (-1);
			}
			/* Make short delay */
			vTaskDelay(1);
		}
		else
		{
			/* Reset timeout for socket closing */
			xTimeOut = xTaskGetTickCount();
		}

		/* Correct pointer with using returned positive result
		(actually sent data) */
		uxDataLength -= xRc;
		pvBuffer += xRc;
	}

	return xRc;
}

#endif /* ipconfigUSE_HTTP */
