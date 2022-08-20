/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _WEB_SERVER_H_
#define _WEB_SERVER_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"

/* Application includes. */
#include "settings.h"

/* Public constants ----------------------------------------------------------*/
enum ProtocolType
{
	DHCP,
	Static_IP
};

#define HTML_LOGIN_MAX_LEN 			20
#define HTML_PASSW_MAX_LEN 			20

#if (configUSE_FAT == 1)
enum FF_DiskState
{
	FF_DISK_REMOVED,
	FF_DISK_NO_INITED,
	FF_DISK_NO_MOUNTED,
	FF_DISK_MOUNTED
};
#endif /*(configUSE_FAT == 1)*/

/* Public variables ----------------------------------------------------------*/
extern enum ProtocolType currProtocolType;
extern uint32_t staticIP_Addr;
extern uint32_t staticNetMask;
extern uint32_t staticIP_GW;
extern uint32_t staticIP_DNS;

/* Public function prototypes ------------------------------------------------*/
void WebServerInit();
void WebServerSetDefaults();
void WebServerSetStaticIP_UntilReboot();

/* MAC address operations function prototypes --------------------------------*/
const uint8_t* WebServerGetMAC_Addr();

#ifdef ALLOW_MAC_ADDRESS_OVERRIDE
BaseType_t WebServerGetMAC_AddrIsOvrd();
const uint8_t* WebServerGetMAC_AddrOvrd();
void WebServerSetMAC_AddrOvrd(const uint8_t* pMAC_Addr);
void WebServerResetMAC_AddrOvrd();
#endif /*ALLOW_MAC_ADDRESS_OVERRIDE*/

void WebServerApplyNetworkSettings();
void StoreSettingsAndResetMCU();

/* Settings functions */
char* GetLogin();
void SetLogin(char* str);
char* GetPassword();
void SetPassword(char* str);
uint32_t GetUpTime();

/* Weak function to reset authorization keys */
void ResetAuthKeys();

/* Status functions */
#if (configUSE_FAT == 1)
/* Disk state */
enum FF_DiskState GetFF_DiskState();
#endif /*(configUSE_FAT == 1)*/

#endif /*_WEB_SERVER_H_*/
