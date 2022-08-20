/* Define to prevent recursive inclusion ------------------------------------ */
#ifndef _TRS_SYNC_PROTO_DRIVER_H_
#define _TRS_SYNC_PROTO_DRIVER_H_

/* Includes ----------------------------------------------------------------- */
#include <stdint.h>
#include <stdbool.h>

/* Public function prototypes ----------------------------------------------- */
void TRS_SyncProtoDriverInit();
void TRS_SyncProtoDriverSetTransmitDirection(bool direction);
bool TRS_SyncProtoDriverSendTX_Buff(uint8_t *buff, uint8_t lenght);
/* Driver functions, which can be overriden */
void TRS_SyncProtoDriverGetRX_Byte(uint8_t byte);

#endif /* _TRS_SYNC_PROTO_DRIVER_H_ */
