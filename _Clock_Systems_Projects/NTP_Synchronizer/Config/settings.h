/* Define to prevent recursive inclusion ------------------------------------ */
#ifndef _SETTINGS_H_
#define _SETTINGS_H_

/* Hardware supporting:
 * NTP_Synchronizer_V1.0 */

/* OUR CONFIGE -------------------------------------------------------------- */
/* General settings ----------------------------------------------------------*/
/* Device (or device family) name */
#define DEVICE_NAME 				"SNTP_SNC"
/* Project version */
#define PROJECT_VER 				"1.0a"

/* Network configure ---------------------------------------------------------*/
/* Prefix for NetBIOS name */
#define NET_BIOS_PREFIX 			"SNTP-"
#define RESET_MCU_AFTER_NERWORK_CHANGE

/* Default IP address configuration */
#define configIP_ADDR0				192
#define configIP_ADDR1				168
#define configIP_ADDR2				1
#define configIP_ADDR3				100

/* Default netmask configuration */
#define configNET_MASK0				255
#define configNET_MASK1				255
#define configNET_MASK2				255
#define configNET_MASK3				0

/* Default gateway IP address configuration */
#define configGW_ADDR0				configIP_ADDR0
#define configGW_ADDR1				configIP_ADDR1
#define configGW_ADDR2				configIP_ADDR2
#define configGW_ADDR3				1

#define configDNS_ADDR0				configGW_ADDR0
#define configDNS_ADDR1				configGW_ADDR1
#define configDNS_ADDR2				configGW_ADDR2
#define configDNS_ADDR3				configGW_ADDR3

/* HTTP settings -------------------------------------------------------------*/
#define MAX_HTTP_CLIENTS_CONN 		3// 2// 4//

/* Logging settings ----------------------------------------------------------*/
#define MAX_UDP_LOG_MSG_SIZE 		64

/* RTC configuration ---------------------------------------------------------*/
#define DST_PRESENT
#define LOCAL_GMT		  			2

/* SNTP configuration --------------------------------------------------------*/
#define DEFAULT_NTP_SYNC_PERIOD 	3600// 30//
#define DEFAULT_NTP_STARTUP_DELAY 	0
#define SNTP_RECV_TIMEOUT 			5000
#define SNTP_SET_ACCURATE_TIME

/* Set actual time after last synchronization, sec */
#define SNTP_ACTUAL_TIME_TIMEOUT 	(30*60)//10//

/* Hardware configuration --------------------------------------------------- */
#define MCU_MAX_PERFORMANCE//MCU_MIDDLE_PERFORMANCE//MCU_HIGH_PERFORMANCE//
#define WATCH_DOG_RELOAD_PERIOD 	16000

/* DEBUGGING ---------------------------------------------------------------- */
//#define DEBUG_MODULES
//#define DISABLE_WEB_UI_LOGIN

#endif /*_SETTINGS_H_*/
