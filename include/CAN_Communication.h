/*
 * CAN_Communication.h
 *
 *  Created on: 18-Oct-2025
 *      Author: PandurangaKarapothul
 */

#ifndef CAN_COMMUNICATION_CAN_COMMUNICATION_H_
#define CAN_COMMUNICATION_CAN_COMMUNICATION_H_

#include"bsp.h"

typedef enum{
	CAN_Buffer_Idx_1,
	CAN_Buffer_Idx_2,
}CAN_BufferId_En_t;

typedef enum{
	CAN_ID_0x1A0,
	CAN_ID_0x1B0,
	CAN_TOTAL_ID
}CAN_ID;

typedef struct
{
	CAN_BufferId_En_t CAN_BufferID_En;
	DRV_CanFrame_St_t  DRV_CanFrame_St;
}CAN_DataFrame_St_t;

extern CAN_DataFrame_St_t CAN_DataFrame_St[CAN_TOTAL_ID];
void CAN_Callback(U8  instance,flexcan_event_type_t eventType,U32 bufferIdx,flexcan_state_t *flexcanState);
void CAN_Rx_0x1B0_mv(uint8_t *buff);
void  CAN_ID_0x1A0_mv(char *Data, uint32_t length);
#endif /* CAN_COMMUNICATION_CAN_COMMUNICATION_H_ */
