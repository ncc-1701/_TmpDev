/* This file provides initialization code for FeeRTOS+TCP middleWare. */
 
/* Includes ------------------------------------------------------------------*/
/* Standard includes. */
#include <stdint.h>
#include <assert.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_TCP_server.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_DHCP.h"

#if (configUSE_FAT == 1)
	/* FreeRTOS+FAT includes. */
	#include "ff_stdio.h"
	#include "ff_ramdisk.h"
	#include "ff_sddisk.h"
#endif /*(configUSE_FAT == 1)*/

/* Application includes. */
#include "settings.h"
#include "main_app.h"
#include "html_txt_funcs.h"
#include "httpserver-netconn.h"
#include "web-server.h"

/* Constants -----------------------------------------------------------------*/
#ifndef NEXT_TRY_TO_GET_DHCP_TIMEOUT
#	define NEXT_TRY_TO_GET_DHCP_TIMEOUT 15
#endif /*NEXT_TRY_TO_GET_DHCP_TIMEOUT*/

#ifndef NET_BIOS_PREFIX
#	define NET_BIOS_PREFIX 				"DEV-"
#endif /*NET_BIOS_PREFIX*/

/* Private constants ---------------------------------------------------------*/
#define HTML_DEFAULT_LOGIN 			"admin"
#define HTML_DEFAULT_PASSW 			"1"

/* HTTP servers execute in the TCP server work task. */
#ifndef mainTCP_SERVER_TASK_PRIORITY
#	define mainTCP_SERVER_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#endif /*mainTCP_SERVER_TASK_PRIORITY*/
#define	mainTCP_SERVER_STACK_SIZE	(configMINIMAL_STACK_SIZE * 8)

#if (configUSE_FAT == 1)
	/* The SD card is mounted in the root of the file system. */
	#define mainHAS_SDCARD					1
	#define mainSD_CARD_DISK_NAME			"/"
#endif /*(configUSE_FAT == 1)*/

/* Variables -----------------------------------------------------------------*/
enum ProtocolType currProtocolType;
static bool static_IP_UntilReboot = false;

/* The static IP and MAC address.  The address configuration will be used if
ipconfigUSE_DHCP is 0, or if ipconfigUSE_DHCP is 1 but a DHCP server could
not be contacted. */
uint32_t staticIP_Addr;
uint32_t staticNetMask;
uint32_t staticIP_GW;
uint32_t staticIP_DNS;

/* Default MAC address configuration. */
uint8_t ucMACAddress[ipMAC_ADDRESS_LENGTH_BYTES] = {0};

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
uint8_t ucMAC_AddressOvrd[ipMAC_ADDRESS_LENGTH_BYTES] =
{[0 ... ipMAC_ADDRESS_LENGTH_BYTES - 1] = 0xFF};
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

/* Prefix for host name or netBIOS name */
char hostName[16] = NET_BIOS_PREFIX;

/* Handle of the task that runs the FTP and HTTP servers. */
static TaskHandle_t xServerWorkTaskHandle = NULL;

/* The server that manages the FTP and HTTP connections. */
static TCPServer_t *pxTCPServer = NULL;

#if (ipconfigUSE_DHCP == 1)
/* Set timeout for next DHCP request if the first request will failed */
static xTimerHandle xDHCP_NextTryTimer = NULL;
static BaseType_t gotSettingsByDHCP = pdFALSE;
#endif /*(ipconfigUSE_DHCP == 1)*/

/* Login and password variables */
static char login[HTML_LOGIN_MAX_LEN] = HTML_DEFAULT_LOGIN;
static char password[HTML_PASSW_MAX_LEN] = HTML_DEFAULT_PASSW;

#if (configUSE_FAT == 1)
/* The SD card disk. */
static FF_Disk_t *pxDisk = NULL;

/* SD card state */
static enum FF_DiskState SD_CardState = FF_DISK_REMOVED;
#endif /*(configUSE_FAT == 1)*/

/* Counter for upTime */
static uint32_t upTime = 0; //7654*24*60*60 + 3*60*60 + 2*60 + 1;//

/* Private function prototypes -----------------------------------------------*/
static void prvServerWorkTask(void *pvParameters);

#if (ipconfigUSE_DHCP == 1)
static void RestartDHCP(TimerHandle_t xTimer);
#endif /*(ipconfigUSE_DHCP == 1)*/

#if (configUSE_FAT == 1)
static void prvSDCardDetect(void);
#endif /*(configUSE_FAT == 1)*/

static void StoreMAC_Address();

/* Public functions ----------------------------------------------------------*/
void WebServerInit()
{
	/* Check for errors */
	static_assert(sizeof(NET_BIOS_PREFIX) <= (16 - 8),
			"Too long NetBIOS prefix");

	/* Reset variables */
	//SetDefaults();

	FreeRTOS_printf(("FreeRTOS_IPInit\n"));
	
#if (ipconfigUSE_DHCP == 1)
	/* Init DHCP NextTry timer for two minutes without */
	xDHCP_NextTryTimer =
		xTimerCreate(
				"DHCP_NextTry",
				(NEXT_TRY_TO_GET_DHCP_TIMEOUT * pdMS_TO_TICKS(1000)),
				pdFALSE, NULL, RestartDHCP);
	if(xDHCP_NextTryTimer == NULL)
	{
		FreeRTOS_printf(("Could not create DHCP_NextTryTimer\n"));
		return;
	}
#endif /*(ipconfigUSE_DHCP == 1)*/

	/* Store MAC */
	StoreMAC_Address();

	/* Set network settings */
	uint8_t* ucIPAddress = (uint8_t*)(&staticIP_Addr);
	uint8_t* ucNetMask = (uint8_t*)(&staticNetMask);
	uint8_t* ucGatewayAddress = (uint8_t*)(&staticIP_GW);
	uint8_t* ucDNSServerAddress = (uint8_t*)(&staticIP_DNS);
	
	/* Initialize the network interface. */
	/*if(currProtocolType == DHCP)
	{
		FreeRTOS_IPInit(0, 0, 0, 0, ucMACAddress);
	}
	else*/ FreeRTOS_IPInit(ucIPAddress, ucNetMask, ucGatewayAddress,
			ucDNSServerAddress, ucMACAddress);

	/* Create the task that handles the TCP server.  This will wait for a
	notification from the network event hook before creating the servers.
	The task is created at the idle priority, and sets itself to
	mainTCP_SERVER_TASK_PRIORITY after the file system has initialised. */
	if(xTaskCreate(prvServerWorkTask, "SvrWork", mainTCP_SERVER_STACK_SIZE, 
		NULL, tskIDLE_PRIORITY, &xServerWorkTaskHandle) !=  pdPASS)
	{
		FreeRTOS_printf(("Could not create TCP server task\n"));
		return;
	}
}

void WebServerSetDefaults()
{
#if (ipconfigUSE_DHCP == 1)
	currProtocolType = DHCP;
#else /*(ipconfigUSE_DHCP == 1)*/
	currProtocolType = Static_IP;
#endif /*(ipconfigUSE_DHCP == 1)*/

	staticIP_Addr = FreeRTOS_inet_addr_quick(configIP_ADDR0, configIP_ADDR1,
											 configIP_ADDR2, configIP_ADDR3);
	staticNetMask = FreeRTOS_inet_addr_quick(configNET_MASK0, configNET_MASK1,
											 configNET_MASK2, configNET_MASK3);
	staticIP_GW = FreeRTOS_inet_addr_quick(configGW_ADDR0, configGW_ADDR1,
										   configGW_ADDR2, configGW_ADDR3);
	staticIP_DNS = FreeRTOS_inet_addr_quick(configDNS_ADDR0, configDNS_ADDR1,
										   configDNS_ADDR2, configDNS_ADDR3);

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
	/* Disable override of MAC */
	WebServerResetMAC_AddrOvrd();
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

	/* Set default login and password */
	SetValue(HTML_DEFAULT_LOGIN, login, HTML_LOGIN_MAX_LEN);
	SetValue(HTML_DEFAULT_PASSW, password, HTML_PASSW_MAX_LEN);
	ResetAuthKeys();
}

/* MAC address operations function -------------------------------------------*/
void WebServerSetStaticIP_UntilReboot()
{
	/* Set corresponding flag */
	static_IP_UntilReboot = 1;
}

const uint8_t* WebServerGetMAC_Addr()
{
	return ucMACAddress;
}

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
BaseType_t WebServerGetMAC_AddrIsOvrd()
{
	uint8_t* pMAC_AddressOvrd = ucMAC_AddressOvrd;
	for(uint8_t i = 0; i < ipMAC_ADDRESS_LENGTH_BYTES; i++)
	{
		if(*pMAC_AddressOvrd != 0xFF) return pdTRUE;
		pMAC_AddressOvrd++;
	}
	return pdFALSE;
}

const uint8_t* WebServerGetMAC_AddrOvrd()
{
	return ucMAC_AddressOvrd;
}

void WebServerSetMAC_AddrOvrd(const uint8_t* pMAC_Addr)
{
	/* Copy overrode MAC and watch for changes */
	BaseType_t changed = pdFALSE;
	uint8_t* pMAC_AddressOvrd = ucMAC_AddressOvrd;
	for(uint8_t i = 0; i < ipMAC_ADDRESS_LENGTH_BYTES; i++)
	{
		if(*pMAC_AddressOvrd != *pMAC_Addr)
		{
			changed = pdTRUE;
			*pMAC_AddressOvrd = *pMAC_Addr;
		}
		pMAC_AddressOvrd++;
		pMAC_Addr++;
	}

	if(changed)
	{
		/* Try to apply the overrode MAC */
		StoreMAC_Address();
	}
}

void WebServerResetMAC_AddrOvrd()
{
	const uint8_t resetMAC_AddressOvrd[ipMAC_ADDRESS_LENGTH_BYTES] =
	{[0 ... ipMAC_ADDRESS_LENGTH_BYTES - 1] = 0xFF};
	WebServerSetMAC_AddrOvrd(resetMAC_AddressOvrd);
}
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

void WebServerApplyNetworkSettings()
{
	/* Send NetworkDown event */
	FreeRTOS_NetworkDown();
}

__attribute__((weak)) void StoreSettingsAndReset_MCU() {}

/* Hooks functions -----------------------------------------------------------*/
/* Called by FreeRTOS+TCP when the network connects or disconnects.  Disconnect
events are only received if implemented in the MAC driver. */
void vApplicationIPNetworkEventHook(eIPCallbackEvent_t eNetworkEvent)
{
	uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;

#ifdef RESET_MCU_AFTER_NERWORK_CHANGE
	static uint32_t lastIPAddress = 0;
	static enum ProtocolType lastProtocolType;
#endif /*RESET_MCU_AFTER_NERWORK_CHANGE*/

	char cBuffer[16];
	static BaseType_t xTasksAlreadyCreated = pdFALSE;

	FreeRTOS_printf(("vApplicationIPNetworkEventHook: event %ld\n", 
					 eNetworkEvent));

	/* If the network has just come up... */
	if(eNetworkEvent == eNetworkUp)
	{
		/* Create the tasks that use the IP stack if they have not already been
		created. */
		if(xTasksAlreadyCreated == pdFALSE)
		{
			/* Tasks that use the TCP/IP stack can be created here. */

			/* Start a new task to fetch logging lines and send them out. */
			#if (CREATE_UDP_LOGGING_TASK == 1)
			{
				vUDPLoggingTaskCreate();
			}
			#endif /*(CREATE_UDP_LOGGING_TASK == 1)*/

			/* Let the server work task now it can now create the servers. */
			xTaskNotifyGive(xServerWorkTaskHandle);

			xTasksAlreadyCreated = pdTRUE;
		}
	
		FreeRTOS_GetAddressConfiguration(&ulIPAddress,
										 &ulNetMask,
										 &ulGatewayAddress,
										 &ulDNSServerAddress);

#ifdef RESET_MCU_AFTER_NERWORK_CHANGE
		if(lastIPAddress == 0)
		{
			lastIPAddress = ulIPAddress;
			lastProtocolType = currProtocolType;
		}
#endif /*RESET_MCU_AFTER_NERWORK_CHANGE*/

#if (ipconfigUSE_DHCP == 1)
		/* Check for DHCP state */
		if((currProtocolType == DHCP) && (static_IP_UntilReboot == false))
		{
			if(gotSettingsByDHCP == pdFALSE)
			{
				if(xDHCP_NextTryTimer != NULL)
				{
					if(xTimerIsTimerActive(xDHCP_NextTryTimer) != pdFALSE)
						xTimerReset(xDHCP_NextTryTimer, 0);
					else xTimerStart(xDHCP_NextTryTimer, 0);
				}
			}
		}
#endif /*(ipconfigUSE_DHCP == 1)*/

		if((currProtocolType == Static_IP) || static_IP_UntilReboot)
		{
			/* Check for changes */
			if((ulIPAddress != staticIP_Addr) ||
				(ulNetMask != staticNetMask) ||
				(ulGatewayAddress != staticIP_GW) ||
				(ulDNSServerAddress != staticIP_DNS))
			{
#ifdef RESET_MCU_AFTER_NERWORK_CHANGE
				/* Compare network with own */
				if(static_IP_UntilReboot == false)
				{
					if(	(ulIPAddress & FreeRTOS_GetNetmask()) !=
						(lastIPAddress & FreeRTOS_GetNetmask()))
					{
						/* Trigger settings manager */
						StoreSettingsAndReset_MCU();
					}
				}

				/* Store current IP and protocol type */
				lastIPAddress = ulIPAddress;
#endif /*RESET_MCU_AFTER_NERWORK_CHANGE*/

				/* Store settings */
				ulIPAddress = staticIP_Addr;
				ulNetMask = staticNetMask;
				ulGatewayAddress = staticIP_GW;
				ulDNSServerAddress = staticIP_DNS;

				/* Apply static IP and update netInterface settings */
				FreeRTOS_SetAddressConfiguration(
					&staticIP_Addr, &staticNetMask,
					&staticIP_GW, &staticIP_DNS);
			}
		}

#ifdef RESET_MCU_AFTER_NERWORK_CHANGE
		if(static_IP_UntilReboot == false)
		{
			/* Check changes protocol type */
			if(lastProtocolType != currProtocolType)
			{
				/* Trigger settings manager */
				StoreSettingsAndReset_MCU();
			}
			lastProtocolType = currProtocolType;
		}
#endif /*RESET_MCU_AFTER_NERWORK_CHANGE*/

		/* Reset temporary flag */
		static_IP_UntilReboot = false;

		/* Print out the network configuration, which may have come from a DHCP
		server. */
		FreeRTOS_inet_ntoa(ulIPAddress, cBuffer);
		FreeRTOS_printf(("IP Address: %s\n", cBuffer));

		FreeRTOS_inet_ntoa(ulNetMask, cBuffer);
		FreeRTOS_printf(("Subnet Mask: %s\n", cBuffer));

		FreeRTOS_inet_ntoa(ulGatewayAddress, cBuffer);
		FreeRTOS_printf(("Gateway Address: %s\n", cBuffer));

		FreeRTOS_inet_ntoa(ulDNSServerAddress, cBuffer);
		FreeRTOS_printf(("DNS Server Address: %s\n", cBuffer));
	}
}

const char *pcApplicationHostnameHook(void)
{
	/* Assign the host name.  This function will be called during the DHCP:
	the machine will be registered with an IP address plus this name. */
	return hostName;
}

BaseType_t xApplicationDNSQueryHook(const char *pcName)
{
	if(strcmp(pcName, hostName) == 0) return pdTRUE;
	return pdFALSE;
}

#if (ipconfigUSE_DHCP == 1)
eDHCPCallbackAnswer_t xApplicationDHCPHook(eDHCPCallbackPhase_t eDHCPPhase,
                                            uint32_t ulIPAddress)
{
	/* Prevent warning for unused parameters */
	(void)ulIPAddress;

	/* Check current settings */
	if(currProtocolType != DHCP)
	{
		/* Anyway: stop DHCP timer */
		if(xDHCP_NextTryTimer != NULL) xTimerStop(xDHCP_NextTryTimer, 0);
		return eDHCPUseDefaults;
	}

	eDHCPCallbackAnswer_t eReturn = eDHCPContinue;

	/* This hook is called in a couple of places during the DHCP process, as
	identified by the eDHCPPhase parameter. */
	switch(eDHCPPhase)
	{
	case eDHCPPhasePreDiscover:
		/* A DHCP discovery is about to be sent out.  eDHCPContinue is
		returned to allow the discovery to go out. */
		gotSettingsByDHCP = pdFALSE;
		break;

	case eDHCPPhasePreRequest:
		/* An offer has been received from the DHCP server, and the offered
		IP address is passed in the ulIPAddress parameter. */
		gotSettingsByDHCP = pdTRUE;
		
		/* Anyway: stop DHCP timer here */
		if(xDHCP_NextTryTimer != NULL) xTimerStop(xDHCP_NextTryTimer, 0);
		break;
	}
	return eReturn;
}
#endif /*(ipconfigUSE_DHCP == 1)*/

#if (configUSE_FAT == 1)
void vApplicationCardDetectChangeHookFromISR(
		BaseType_t *pxHigherPriorityTaskWoken)
{
	#if (ipconfigSUPPORT_SIGNALS != 0) && (ipconfigUSE_FTP == 1)
	/* This routine will be called on every change of the Card-detect GPIO pin.
	The TCP server is probably waiting for in event in a select() statement.
	Wake it up. */
	if(pxTCPServer != NULL)
	{
		FreeRTOS_TCPServerSignalFromISR(pxTCPServer, pxHigherPriorityTaskWoken);
	}
	#else /*(ipconfigSUPPORT_SIGNALS != 0) && (ipconfigUSE_FTP == 1)*/
	(void)pxHigherPriorityTaskWoken;
	#endif /*(ipconfigSUPPORT_SIGNALS != 0) && (ipconfigUSE_FTP == 1)*/
}
#endif /*(configUSE_FAT == 1)*/

#if (ipconfigFTP_HAS_USER_PASSWORD_HOOK != 0)
const char *pcApplicationFTPUserHook(const char *pcUserName)
{
	static const char userOk[] = "331 User name okay, need password\r\n";
	static const char userFail[] = "430 Invalid username\r\n";

	/* Compare with login */
	if(ValueCmp(pcUserName, GetLogin()) == false)
	{
		/* Wrong login */
		return userFail;
	}
	return userOk;
}

BaseType_t xApplicationFTPPasswordHook(
		const char *pcUserName, const char *pcPassword)
{
	/* Compare with login */
	if(ValueCmp(pcUserName, GetLogin()) == false)
	{
		/* Wrong login */
		return pdFALSE;
	}

	/* Compare with password */
	if(ValueCmp(pcPassword, GetPassword()) == false)
	{
		/* Wrong password */
		return pdFALSE;
	}
	return pdTRUE;
}
#endif /*(ipconfigFTP_HAS_USER_PASSWORD_HOOK != 0)*/

/* Settings functions --------------------------------------------------------*/
char* GetLogin()
{
	return login;
}

void SetLogin(char* str)
{
	/* Validate string for empty state */
	if(*str == 0) return;

	SetValue(str, login, HTML_LOGIN_MAX_LEN);
}
char* GetPassword()
{
	return password;
}

void SetPassword(char* str)
{
	/* Validate string for empty state */
	if(*str == 0) return;

	SetValue(str, password, HTML_PASSW_MAX_LEN);
}

uint32_t GetUpTime()
{
	return upTime;
}

/* Weak function to reset authorization keys */
__attribute__((weak)) void ResetAuthKeys() {}

/* Status functions */
#if (configUSE_FAT == 1)
/* Disk state */
enum FF_DiskState GetFF_DiskState()
{
	return SD_CardState;
}
#endif /*(configUSE_FAT == 1)*/

/* Private functions ---------------------------------------------------------*/
static void prvServerWorkTask(void *pvParameters)
{
	pxTCPServer = NULL;
	const TickType_t xInitialBlockTime = pdMS_TO_TICKS(5000UL);

	/* A structure that defines the servers to be created.  Which servers are
	included in the structure depends on the mainCREATE_HTTP_SERVER and
	mainCREATE_FTP_SERVER settings at the top of this file. */
	static const struct xSERVER_CONFIG xServerConfiguration[] =
	{
		#if (ipconfigUSE_HTTP == 1)
				/* Server type,	port number,	backlog,	root dir. */
				{ eSERVER_HTTP, 80, 			12, 		ipconfigHTTP_ROOT },
		#endif

		#if (ipconfigUSE_FTP == 1)
				/* Server type,	port number,	backlog, 	root dir. */
				{ eSERVER_FTP, 	21, 			12, 		"" }
		#endif
	};

	/* Remove compiler warning about unused parameter. */
	(void) pvParameters;

#if (configUSE_FAT == 1)
	/* Try to initialize FAT. */
	//pxDisk = FF_SDDiskInit(mainSD_CARD_DISK_NAME);
#endif /*(configUSE_FAT == 1)*/

	/* The priority of this task can be raised now the disk has been
	initialised. */
	vTaskPrioritySet(NULL, mainTCP_SERVER_TASK_PRIORITY);

	/* Wait until the network is up before creating the servers.  The
	notification is given from the network event hook. */
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

	/* Create the servers defined by the xServerConfiguration array above. */
	pxTCPServer = FreeRTOS_CreateTCPServer(xServerConfiguration,
			sizeof(xServerConfiguration)/sizeof(xServerConfiguration[0]));
	configASSERT(pxTCPServer);

	/* UpTime variables */
	static TickType_t xUpTime;
	static TickType_t xCurrTime;
	xUpTime = xTaskGetTickCount();

	for(;;)
	{
#if (configUSE_FAT == 1)
		/* If board does not define a pin for detecting the
		removal of the SD card, so periodically check for the cards
		presence. */
		prvSDCardDetect();
#endif /*(configUSE_FAT == 1)*/

		/* Run TCP server task. */
		FreeRTOS_TCPServerWork(pxTCPServer, xInitialBlockTime);

		/* Count UpTime */
		xCurrTime = xTaskGetTickCount();
		while((xCurrTime - xUpTime) > 1000)
		{
			xUpTime += 1000;
			upTime++;
		}

		/* Additional reset WatchDog */
		ExternResetWD();
	}
}

#if (ipconfigUSE_DHCP == 1)
static void RestartDHCP(TimerHandle_t xTimer)
{
	/* Remove compiler warning about unused parameter. */
	(void)xTimer;

	vDHCPProcess(pdTRUE);
}
#endif /*(ipconfigUSE_DHCP == 1)*/

#if (configUSE_FAT == 1)
static void prvSDCardDetect(void)
{
static BaseType_t xWasPresent = pdTRUE, xIsPresent;
FF_IOManager_t *pxIOManager;
BaseType_t xResult;

	/* The Xplained Pro board does not define a pin for detecting the remove of
	the SD card, so check for the card periodically in software. */
	xIsPresent = FF_SDDiskDetect(pxDisk);
	if(pxDisk == NULL)
	{
		if(xIsPresent != pdFALSE)
		{
			/* Try to initialize FAT. */
			pxDisk = FF_SDDiskInit(mainSD_CARD_DISK_NAME);
		}
	}

	if(xWasPresent != xIsPresent)
	{
		if(pxDisk == NULL)
		{
			/* First initialization has not been done yet */
			SD_CardState = FF_DISK_REMOVED;
		}
		else if(xIsPresent == pdFALSE)
		{
			/* Store state to local variable */
			SD_CardState = FF_DISK_REMOVED;

			FreeRTOS_printf(("SD-card now removed (%ld -> %ld)\n",
					xWasPresent, xIsPresent));

			/* _RB_ Preferably the IO manager would not be exposed to the
			application here, but instead FF_SDDiskUnmount() would, which takes
			the disk as its parameter rather than the IO manager, would itself
			invalidate any open files before unmounting the disk. */
			pxIOManager = sddisk_ioman(pxDisk);

			if(pxIOManager != NULL)
			{
				/* Invalidate all open file handles so they will get closed by
				the application. */
				FF_Invalidate(pxIOManager);
				FF_SDDiskUnmount(pxDisk);
			}
		}
		else
		{
			FreeRTOS_printf(("SD-card now present (%ld -> %ld)\n",
					xWasPresent, xIsPresent));
			configASSERT(pxDisk);

			xResult = FF_SDDiskReinit(pxDisk);
			if(xResult != pdPASS)
			{
				/* Store state to local variable */
				SD_CardState = FF_DISK_NO_INITED;

				FF_PRINTF("FF_SDDiskInit: prvSDMMCInit failed\n");
			}
			else
			{
				xResult = FF_SDDiskMount(pxDisk);

				if(xResult > 0)
				{
					/* Store state to local variable */
					SD_CardState = FF_DISK_MOUNTED;

					FF_PRINTF("FF_SDDiskMount: SD-card OK\n");
					FF_SDDiskShowPartition(pxDisk);
				}
				else
				{
					/* Store state to local variable */
					SD_CardState = FF_DISK_NO_MOUNTED;

					FF_PRINTF("FF_SDDiskMount: SD-card FAILED\n");
				}
			}
		}

		xWasPresent = xIsPresent;
	}
}
#endif /*(configUSE_FAT == 1)*/

/* This function store the MAC, based on MCU ID or overrode MAC */
static void StoreMAC_Address()
{
	uint8_t* pMAC_Addr = ucMACAddress;

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
	/* Check for overrode MAC */
	if(WebServerGetMAC_AddrIsOvrd())
	{
		/* Copy overrode MAC */
		uint8_t* pMAC_AddressOvrd = ucMAC_AddressOvrd;
		for(uint8_t i = 0; i < ipMAC_ADDRESS_LENGTH_BYTES; i++)
		{
			*pMAC_Addr = *pMAC_AddressOvrd;
			pMAC_Addr++;
			pMAC_AddressOvrd++;
		}
	}
	else
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/
	{
		/* Form MAC based by MCU ID */
		*pMAC_Addr = 0x00;
		pMAC_Addr++;
		*pMAC_Addr = 0x80;
		pMAC_Addr++;

		/* Set last 4 bytes by unique ID */
		struct Unique_ID ID;
		uid_read(&ID);

		/* Create summ off all ID */
		uint32_t summ = ID.off0 + (ID.off2 << 16);
		summ += ID.off4;
		summ += ID.off8;

		*pMAC_Addr = (uint8_t)(summ >> 24);
		pMAC_Addr++;
		*pMAC_Addr = (uint8_t)(summ >> 16);
		pMAC_Addr++;
		*pMAC_Addr = (uint8_t)(summ >> 8);
		pMAC_Addr++;
		*pMAC_Addr = (uint8_t)(summ);
	}

	/* Generate the host name */
	for(uint8_t i = sizeof(NET_BIOS_PREFIX); i < sizeof(hostName); i++)
	{
		hostName[i] = 0;
	}

	/* Use last 4 bytes of MAC */
	uint32_t summ = (ucMACAddress[2] << 24) | (ucMACAddress[3] << 16) |
			(ucMACAddress[4] << 8) | ucMACAddress[5];
	SetHexToStr(summ, &hostName[sizeof(NET_BIOS_PREFIX) - 1], 9, false);

	/* Check for task handle state */
	if(xServerWorkTaskHandle != NULL)
	{
		/* Update MAC for IP stack */
		FreeRTOS_UpdateMACAddress(ucMACAddress);

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
		/* Force reinit eth HW */
		extern void xNetworkInterfaceForceInitialise();
		xNetworkInterfaceForceInitialise();
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/
	}
}
