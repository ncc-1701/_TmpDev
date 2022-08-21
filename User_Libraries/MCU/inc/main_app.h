/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _MAIN_APP_H_
#define _MAIN_APP_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/* Structs' definitions */
struct Unique_ID 
{
	uint16_t off0;
	uint16_t off2;
	uint32_t off4;
	uint32_t off8;
};

/* Exported functions prototypes ---------------------------------------------*/
void MainAppInit();
void MainAppTask();
void MainAppProcess();
void MainAppTimeCriticalTask();
void MainAppTimeCriticalProcess();
void MainAppPerSecondTask();
void MainAppPerSecondProcess();
void MainAppSetDefaults();

/* Read unique ID */
void uid_read(struct Unique_ID *id);

/* Some CPU functions */
void SoftResetCPU();
void ExternResetWD();

#endif /* _MAIN_APP_H_ */
