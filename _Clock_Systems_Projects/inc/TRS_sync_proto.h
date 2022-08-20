/* Define to prevent recursive inclusion ------------------------------------ */
#ifndef _TRS_SYNC_PROTO_H_
#define _TRS_SYNC_PROTO_H_

/* Includes ----------------------------------------------------------------- */
#include <stdint.h>
#include <stdbool.h>

/* Application includes */
#include "rtc.h"

/* Public constants --------------------------------------------------------- */
enum TRS_SyncProtoCommState
{
	NO_COMM_WITH_PC,
	PRES_SINHR_WITH_PC
};

/* Public function prototypes ----------------------------------------------- */
void TRS_SyncProtoInit();
void TRS_SyncProtoSetDefaults();

#endif /* _TRS_SYNC_PROTO_H_ */
