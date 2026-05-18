/*
 * CAN_Communication.c
 *
 * Created on: 18-Oct-2025
 * Author: PandurangaKarapothul
 * Description: CAN Communication Implementation - CAN Message Transmission and Reception
 * Version: 1.0
 * Modification History:
 * Date            Author              Description
 * ----------------------------------------------------------------------------
 * 18-Oct-2025     PandurangaKarapothul Initial CAN Communication Implementation
 * 18-Oct-2025     PandurangaKarapothul Added bootloader CAN IDs (0x1A0 Tx, 0x1B0 Rx)
 ******************************************************************************/

#include <bsp_config.h>
#include "can_communication.h"
#include "drv_can.h"

#define CAN_DATA_LENGTH (8U)

/* ==================== GLOBAL VARIABLES ==================== */
CAN_DataFrame_St_t CAN_DataFrame_St[CAN_TOTAL_ID] =
{
    [CAN_ID_0x1A0] =
    {
        CAN_Buffer_Idx_1,
        {0x1A0U, DRV_CAN_ID_MODE_STANDARD, DRV_CAN_FRAME_TYPE_DATA, 8U, {0U}}
    },
    [CAN_ID_0x1B0] =
    {
        CAN_Buffer_Idx_2,
        {0x1B0U, DRV_CAN_ID_MODE_STANDARD, DRV_CAN_FRAME_TYPE_DATA, 8U, {0U}}
    }
};

/* ==================== PUBLIC FUNCTIONS ==================== */
/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : CAN_ProcessTransmitFrame_mv
 * ----------------------------------------------------------------------------
 * Description   : Transmits data over CAN bus using ID 0x1A0
 * Parameters    : Data - Pointer to data buffer to transmit
 *                 length - Length of data in bytes
 * Return Value  : void
 * Notes         : Maximum transmission length is 8 bytes (CAN data field limit)
 * --------------------------------------------------------------------------*/
void CAN_ProcessTransmitFrame_mv(U8 cmd)
{
    U8  data[CAN_DATA_LENGTH] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};

    data[0] = cmd;

    Common_Memcpy_gst(
        &CAN_DataFrame_St[CAN_ID_0x1A0].DRV_CanFrame_St.DRV_Data_arru8[0],
        &data[0],
        CAN_DATA_LENGTH);

    DRV_CAN_ConfigTxBuffer_gen(
        DRV_CAN_INSTANCE_0,
        CAN_DataFrame_St[CAN_ID_0x1A0].CAN_BufferID_En,
        &CAN_DataFrame_St[CAN_ID_0x1A0].DRV_CanFrame_St,
        CAN_DataFrame_St[CAN_ID_0x1A0].DRV_CanFrame_St.DRV_CanId_u32);

    DRV_CAN_TransmitBlock_gen(
        DRV_CAN_INSTANCE_0,
        CAN_DataFrame_St[CAN_ID_0x1A0].CAN_BufferID_En,
        &CAN_DataFrame_St[CAN_ID_0x1A0].DRV_CanFrame_St);
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : CAN_ProcessReceiveFrame_mv
 * ----------------------------------------------------------------------------
 * Description   : Receives data from CAN bus using ID 0x1B0 (blocking receive)
 * Parameters    : buff - Pointer to buffer where received data will be stored
 * Return Value  : DRV_CanStatus_En - Status of receive operation
 * Notes         : Blocks until CAN message is received or timeout occurs
 * --------------------------------------------------------------------------*/
DRV_CanStatus_ten CAN_ProcessReceiveFrame_mv(U8 * buff)
{
    DRV_CanStatus_ten status;
    U32               i;

    status = DRV_CAN_ReceiveBlock_gen(
                 DRV_CAN_INSTANCE_0,
                 CAN_Buffer_Idx_2,
                 &CAN_DataFrame_St[CAN_ID_0x1B0].DRV_CanFrame_St);

    if (status == DRV_CAN_STATUS_SUCCESS)
    {
        for (i = 0U; i < CAN_DATA_LENGTH; i++)
        {
            buff[i] = CAN_DataFrame_St[CAN_ID_0x1B0].DRV_CanFrame_St.DRV_Data_arru8[i];
        }
    }
    else
    {
        /* Receive failed; buff is left unmodified */
    }

    return status;
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : CAN_Callback
 * ----------------------------------------------------------------------------
 * Description   : CAN driver callback function (currently unused/placeholder)
 * Parameters    : instance - CAN instance number
 *                 eventType - Type of CAN event
 *                 bufferIdx - Buffer index where event occurred
 *                 flexcanState - Pointer to FLEXCAN state structure
 * Return Value  : void
 * Notes         : Parameters are voided to avoid compiler warnings
 * --------------------------------------------------------------------------*/
void CAN_Callback(U8                   instance,
                  flexcan_event_type_t eventType,
                  U32                  bufferIdx,
                  flexcan_state_t *    flexcanState)
{
    (void)instance;
    (void)eventType;
    (void)bufferIdx;
    (void)flexcanState;
}
