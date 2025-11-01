/*
 * CAN_Communication.c
 *
 *  Created on: 18-Oct-2025
 *      Author: PandurangaKarapothul
 */

#include"can_communication.h"
#include "drv_can.h"
#include"application.h"
#include "bsp.h"

/*Bootloader Two Can Ids -> 0x1A0->tx Status  and 0x1B0->rx Reciefing Command From GUI for example Binary File */

/*Application Two Can Ids -> 0x1A0->tx Status ,version  and 0x1B0->rx Reciefing Command From GUI for example Binary File */
CAN_DataFrame_St_t CAN_DataFrame_St[CAN_TOTAL_ID]={
		[CAN_ID_0x1A0]={
				CAN_Buffer_Idx_1,{0x1A0,DRV_CAN_ID_MODE_STANDARD,DRV_CAN_FRAME_TYPE_DATA,8,{0}}
		},
		[CAN_ID_0x1B0]={
				CAN_Buffer_Idx_2,{0x1B0,DRV_CAN_ID_MODE_STANDARD,DRV_CAN_FRAME_TYPE_DATA,8,{0}}
		},
};

void CAN_ID_0x1A0_mv(char *Data, uint32_t length)
{
	if(length > 8){
		length = 8;
	}

	for(uint32_t i=0; i<length; i++)
	{
		CAN_DataFrame_St[CAN_ID_0x1A0].DRV_CanFrame_St.DRV_Data_arru8[i]=Data[i];
	}
	DRV_CAN_ConfigTxBuffer_gen(DRV_CAN_INSTANCE_1, CAN_DataFrame_St[CAN_ID_0x1A0].CAN_BufferID_En, &CAN_DataFrame_St[CAN_ID_0x1A0].DRV_CanFrame_St, CAN_DataFrame_St[CAN_ID_0x1A0].DRV_CanFrame_St.DRV_CanId_u32);
	DRV_CAN_Transmit_gen(DRV_CAN_INSTANCE_1, CAN_DataFrame_St[CAN_ID_0x1A0].CAN_BufferID_En,  &CAN_DataFrame_St[CAN_ID_0x1A0].DRV_CanFrame_St);
}

void CAN_Rx_0x1B0_mv(uint8_t *buff)
{
	DRV_CanStatus_En status = DRV_CAN_ReceiveBlocking_gen(DRV_CAN_INSTANCE_1, CAN_Buffer_Idx_2, &CAN_DataFrame_St[CAN_ID_0x1B0].DRV_CanFrame_St);
	if(status == DRV_CAN_STATUS_SUCCESS)
	{
		for(uint32_t i=0; i<8; i++)
		{
			buff[i] = CAN_DataFrame_St[CAN_ID_0x1B0].DRV_CanFrame_St.DRV_Data_arru8[i];
		}
	}

}

void CAN_Callback(U8 instance,flexcan_event_type_t eventType,U32 bufferIdx,flexcan_state_t *flexcanState)
{
	(void)instance;
	(void)flexcanState;
	(void)eventType;
	(void)bufferIdx;
	/*switch(eventType)
	{
		case FLEXCAN_EVENT_RX_COMPLETE:
		{
			switch(bufferIdx)
			{
				case CAN_Buffer_Idx_2:
				{
					DRV_CAN_Receive_gen(DRV_CAN_INSTANCE_1, CAN_Buffer_Idx_2, &CAN_DataFrame_St[CAN_ID_0x1B0].DRV_CanFrame_St);
					for(uint32_t i=0; i<8; i++)
					{
						rx_buff[i] = CAN_DataFrame_St[CAN_ID_0x1B0].DRV_CanFrame_St.DRV_Data_arru8[i];
					}
				}
			}
			break;
		}
		default :{}
	}*/
}
