/*
 * application.c
 *
 *  Created on: 19-Oct-2025
 *      Author: PandurangaKarapothul
 */

#include"application.h"

BIN can_tx_b=false;
static U32 CAN_Transmition_counter_u32=0;
void APP_Timer0_Callback(void)
{
	CAN_Transmition_counter_u32++;
	if(0==CAN_Transmition_counter_u32%500)
	{
		can_tx_b=true;
		CAN_Transmition_counter_u32=0;
	}
}
