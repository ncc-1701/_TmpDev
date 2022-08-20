/* This is main application file */

/* Includes ------------------------------------------------------------------*/
#include "settings.h"

/* Drivers and hardware headers include */
#include "rtc.h"

/* Applications headers include */
#include "sntp.h"
#include "TRS_sync_proto.h"
#include "ui.h"

/* Critical applications headers include */
#include "web-server.h"
#include "settings_NV_manager.h"
#include "main_app.h"

/* Public functions ----------------------------------------------------------*/
void MainAppInit()
{
	/* Init drivers and hardware */
	RTC_Init();

	/* Init applications variables before restoring settings */
	/* Init only web-server settings */
	WebServerSetDefaults();
	SNTP_Init();
	TRS_SyncProtoInit();
	UI_Init();
	
	/* Init settings manager
	 * and restore NV settings before RAM settings but after app init */
	SettingsNV_ManagerInit();

	/* Init and start web-server after restoring settings */
	WebServerInit();
}

void MainAppSetDefaults()
{
	/* Set default settings for all applications */
	WebServerSetDefaults();
	SNTP_SetDefaults();
	TRS_SyncProtoSetDefaults();
	UI_SetDefaults();
}
