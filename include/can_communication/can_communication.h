/*
 * CAN_Communication.h
 *
 * Created on: 18-Oct-2025
 * Author: PandurangaKarapothul
 * Description: CAN Communication Header - CAN Buffer IDs, Message IDs and Function Declarations
 * Version: 1.0
 * Modification History:
 * Date            Author              Description
 * ----------------------------------------------------------------------------
 * 18-Oct-2025     PandurangaKarapothul Initial CAN Communication Header
 * 18-Oct-2025     PandurangaKarapothul Added bootloader CAN ID definitions
 ******************************************************************************/

#ifndef CAN_COMMUNICATION_CAN_COMMUNICATION_H_
#define CAN_COMMUNICATION_CAN_COMMUNICATION_H_

#include <bsp_config.h>
#include "drv_can.h"
#include "flexcan_driver.h"

typedef enum
{
    CAN_Buffer_Idx_1 = 0,
    CAN_Buffer_Idx_2 = 1
} CAN_BufferId_En_t;

typedef enum
{
    CAN_ID_0x1A0  = 0,
    CAN_ID_0x1B0  = 1,
    CAN_TOTAL_ID  = 2
} CAN_ID;

typedef struct
{
    CAN_BufferId_En_t CAN_BufferID_En;
    DRV_CanFrame_tst  DRV_CanFrame_St;
} CAN_DataFrame_St_t;

extern CAN_DataFrame_St_t CAN_DataFrame_St[CAN_TOTAL_ID];

extern void              CAN_Callback(U8 instance,
                                      flexcan_event_type_t eventType,
                                      U32 bufferIdx,
                                      flexcan_state_t * flexcanState);
extern DRV_CanStatus_ten CAN_ProcessReceiveFrame_mv(U8 * buff);
extern void              CAN_ProcessTransmitFrame_mv(U8 cmd);

#endif /* CAN_COMMUNICATION_CAN_COMMUNICATION_H_ */
