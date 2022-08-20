/* It is user interface application
*/

/* Includes ------------------------------------------------------------------*/
/* Standard includes */
#include <stdint.h>
#include <stdbool.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

/* Drivers includes */
#include "ui_btns_leds_driver.h"

/* Application includes */
#include "settings.h"
#include "main_app.h"
#include "button.h"
#include "rtc.h"
#include "web-server.h"
#include "sntp.h"
#include "ui.h"

/* Private constants -------------------------------------------------------- */
/* Buttons sample period, ms */
#ifndef BUTTONS_SAMPLE_PERIOD
#	define BUTTONS_SAMPLE_PERIOD 		15
#endif /*BUTTONS_SAMPLE_PERIOD*/

#ifndef HOLD_RESET_BUTTON_TIMEOUT
#	define HOLD_RESET_BUTTON_TIMEOUT 	5000
#endif /*HOLD_RESET_BUTTON_TIMEOUT*/

/* LED indication constants */
enum LED_State
{
	LED_StateOff,
	LED_StateOn,
	LED_StateBlink,
	LED_StateFastBlink
};

#define FLASHING_PERIOD				500

/* FreeRTOS constants */
#ifndef UI_TASK_PRIORITY
#	define UI_TASK_PRIORITY				(tskIDLE_PRIORITY + 2)
#endif /*UI_TASK_PRIORITY*/
#define UI_TASK_STACK_SIZE			(configMINIMAL_STACK_SIZE)

/* Other constants -----------------------------------------------------------*/
/* DEBUGGING -----------------------------------------------------------------*/
//#define DEBUG_UI_BUTTONS

/* Variables -----------------------------------------------------------------*/
/* UI buttons and LEDs */
struct HoldButton m_ResetButton;

/* Handle of the UI task that service buttons and indicators */
static TaskHandle_t xUI_TaskHandle = NULL;

/* Other variables */
static bool flashing;
static bool fastFlashing;
static uint16_t flashCount = 0;

/* Private function prototypes -----------------------------------------------*/
static void UI_Task();
static void SetStatusLED_ExtState(enum LED_State state);
static void SetAndApplyDefaults();

/* Public functions ----------------------------------------------------------*/
void UI_Init()
{
	/* Init drivers */
	UI_BtnsLedsDriverInit();
	
	/* Init applications */
	/* Init variables */
	/* Set start condition for indication */
	SetStatusLED_ExtState(LED_StateOff);

	/* Create UI task */
	if(xTaskCreate(UI_Task, "UI_Task", UI_TASK_STACK_SIZE,
		NULL, UI_TASK_PRIORITY, &xUI_TaskHandle) !=  pdPASS)
	{
		FreeRTOS_printf(("Could not create UI task\n"));
		return;
	}
}

void UI_SetDefaults()
{
	/* Set defaults for applications */

}

/* Private functions ---------------------------------------------------------*/
static void UI_Task()
{
	for(;;)
	{
		/* Drivers loop */
		UI_BtnsLedsDriverTask();

		static bool firstCycle = false;
		static uint16_t holdTime = 0;
		static uint16_t indicateResetNetWorkSettings = 0;
		static TickType_t IO_TimeOut = 0;

		/* Check for IO exchange timeout */
		TickType_t time = xTaskGetTickCount();
		if(firstCycle == false)
		{
			firstCycle = true;

			/* Set first time */
			IO_TimeOut = time;
			return;
		}
		if((time - IO_TimeOut) < BUTTONS_SAMPLE_PERIOD) return;

		/* Change timeout */
		IO_TimeOut += BUTTONS_SAMPLE_PERIOD;

		/* Buttons service ---------------------------------------------------*/
		/* Call driver functions for buttons */
		SetHoldButtonState(&m_ResetButton,  GetResetButtonState());

		/* Buttons service ---------------------------------------------------*/
		/* Reset button service */
		if(HoldButtonIsPressed(&m_ResetButton))
		{
	#ifdef DEBUG_UI_BUTTONS
			PrintfUART_DriverSendChar('R');
	#endif /*DEBUG_UI_BUTTONS*/

			/* Set up a timer to set and apply defaults */
			if(holdTime < HOLD_RESET_BUTTON_TIMEOUT)
				holdTime += BUTTONS_SAMPLE_PERIOD;
		}
		else
		{
			/* Abort timer */
			if(holdTime != 0xFFFF) holdTime = 0;
		}

		/* Indication service ------------------------------------------------*/
		if(indicateResetNetWorkSettings)
		{
			/* Show indication of reset network settings event */
			SetStatusLED_ExtState(LED_StateFastBlink);
		}
		else
		{
			/* Show global synchronization state */
			if(GetNTP_TimeStatus() == NTP_TimeValid)
				SetStatusLED_ExtState(LED_StateBlink);
			else SetStatusLED_ExtState(LED_StateOn);
		}

		/* Flashing service */
		flashCount++;
		if(flashCount % (FLASHING_PERIOD/(4*BUTTONS_SAMPLE_PERIOD)) == 0)
		{
			if(fastFlashing) fastFlashing = false;
			else fastFlashing = true;
		}
		if(flashCount >= FLASHING_PERIOD/BUTTONS_SAMPLE_PERIOD)
		{
			flashCount = 0;
			if(flashing) flashing = false;
			else flashing = true;
		}

		/* Reset settings service --------------------------------------------*/
		if(holdTime >= HOLD_RESET_BUTTON_TIMEOUT)
		{
			if(holdTime != 0xFFFF)
			{
				holdTime = 0xFFFF;

				/* Reset network settings */
				SetAndApplyDefaults();

				/* Indicate this event */
				indicateResetNetWorkSettings = 5000;
			}
		}

		/* Service indication of reset network settings event */
		if(indicateResetNetWorkSettings)
		{
			if(indicateResetNetWorkSettings >= BUTTONS_SAMPLE_PERIOD)
				indicateResetNetWorkSettings -= BUTTONS_SAMPLE_PERIOD;
			else indicateResetNetWorkSettings = 0;
		}

		/* Wait for next cycle */
		vTaskDelay(1);
	}
}

static void SetStatusLED_ExtState(enum LED_State state)
{
	if(state == LED_StateOff) SetStatusLED_State(false);
	else if(state == LED_StateOn) SetStatusLED_State(true);
	else if(state == LED_StateBlink) SetStatusLED_State(flashing);
	else if(state == LED_StateFastBlink) SetStatusLED_State(fastFlashing);
}

static void SetAndApplyDefaults()
{
	/* Reset all settings */
	MainAppSetDefaults();

	/* Set static IP for temporary access to web-UI */
	WebServerSetStaticIP_UntilReboot();
	WebServerApplyNetworkSettings();
}
