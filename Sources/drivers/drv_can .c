/*
 * drv_can.c
 *
 *  Description     : CAN Driver
 *  Author          : Rushikesh
 *  Created On      : 13-Jul-2025
 *  Version         : 2.0
 *  Modification History:
 *  Date        Author      Description
 *  ----------------------------------------------------------------------------
 *  13-Jul-2025 RUSHIKESH   CAN Driver Architecture Implementation
 *  28-Jul_2025 RUSHIKESH	CAN Testing Completed with PEAK CAN Device (created own CAN Matrix)
 *  12-Aug-2025 RUSHIKESH   Guidelines Followed the naming Architecture Implementation
 ******************************************************************************/
/* ==================== INCLUDE FILES ==================== */
#include <drv_can.h>
#include "flexcan_driver.h"
#include "canCom1.h"
#include "common_types.h"
#include"can_communication.h"
/* ==================== DEFINES & MACROS ==================== */
#define CAN_FD_UNABLE 0

/* ==================== STATIC VARIABLES ==================== */
static const U8 DRV_CanInstances_arrst[DRV_CAN_MAX_INSTANCE] = {
    INST_CANCOM1
};

static const flexcan_user_config_t* DRV_CanConfig_arrst[DRV_CAN_MAX_INSTANCE] = {
    &canCom1_InitConfig0
};
/* ==================== GLOBLE VARIABLES ==================== */
static flexcan_msgbuff_t DRV_RecvBuff[10] = {0};

flexcan_state_t DRV_flexcanState[DRV_CAN_MAX_INSTANCE];

/* ==================== PRIVATE FUNCTION PROTOTYPES ==================== */


/* ==================== PUBLIC FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_CAN_Init_gen
*   Description   : Initializes CAN controller for specified instance
*   Parameters    : instance_argen - CAN instance to initialize
*   Return Value  : DRV_CanStatus_En - Status of initialization
*  --------------------------------------------------------------------------- */
DRV_CanStatus_En DRV_CAN_Init_gen(DRV_CanInstance_En instance_argen)
{
    status_t status_En = FLEXCAN_DRV_Init(DRV_CanInstances_arrst[instance_argen],
                                     &DRV_flexcanState[instance_argen],
                                     DRV_CanConfig_arrst[instance_argen]);

    if (status_En == STATUS_SUCCESS)
    {
        FLEXCAN_DRV_InstallEventCallback(DRV_CanInstances_arrst[instance_argen], CAN_Callback, NULL);
        return DRV_CAN_STATUS_SUCCESS;
    }

    return DRV_CAN_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_CAN_ConfigRxBuffer_gen
*   Description   : Configures CAN receive buffer
*   Parameters    : instance_argen - CAN instance
*                   bufferIdx_argu8 - Buffer index to configure
*                   frameConfig_argst - Frame configuration
*                   ReceivedMsgId - Message ID to receive
*   Return Value  : DRV_CanStatus_En - Status of configuration
*  --------------------------------------------------------------------------- */
DRV_CanStatus_En DRV_CAN_ConfigRxBuffer_gen(DRV_CanInstance_En instance_argen,
                                  U8 bufferIdx_argu8,
                                  const DRV_CanFrame_St_t* frameConfig_argst,
                                  U32 ReceivedMsgId)
{
    flexcan_data_info_t rxInfo = {
        .msg_id_type = (frameConfig_argst->DRV_IdMode_En == DRV_CAN_ID_MODE_STANDARD) ?
                      FLEXCAN_MSG_ID_STD : FLEXCAN_MSG_ID_EXT,
        .data_length = frameConfig_argst->DRV_DataLength_u8,
#if CAN_FD_UNABLE
        .fd_enable = DRV_CAN_FD_ENABLED,
        .enable_brs = DRV_CAN_BRS_ENABLED,
        .fd_padding = 0
#else
        .fd_enable = DRV_CAN_FD_DISABLED,
        .enable_brs = DRV_CAN_BRS_DISABLED,
        .fd_padding = 0
#endif
    };

    status_t status_En = FLEXCAN_DRV_ConfigRxMb(DRV_CanInstances_arrst[instance_argen],
                                           bufferIdx_argu8,
                                           &rxInfo,
                                           ReceivedMsgId);
    return (status_En == STATUS_SUCCESS) ? DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_CAN_ConfigTxBuffer_gen
*   Description   : Configures CAN transmit buffer
*   Parameters    : instance_argen - CAN instance
*                   bufferIdx_argu8 - Buffer index to configure
*                   frameConfig_argst - Frame configuration
*                   canId_u32 - CAN ID to transmit
*   Return Value  : DRV_CanStatus_En - Status of configuration
*  --------------------------------------------------------------------------- */
DRV_CanStatus_En DRV_CAN_ConfigTxBuffer_gen(DRV_CanInstance_En instance_argen,
                                   U8 bufferIdx_argu8,
                                   const DRV_CanFrame_St_t* frameConfig_argst,
                                   U32 canId_u32)
{
    const flexcan_data_info_t txInfo = {
        .msg_id_type = (frameConfig_argst->DRV_IdMode_En == DRV_CAN_ID_MODE_STANDARD) ?
                      FLEXCAN_MSG_ID_STD : FLEXCAN_MSG_ID_EXT,
        .data_length = frameConfig_argst->DRV_DataLength_u8,
        .is_remote = (frameConfig_argst->DRV_FrameType_En == DRV_CAN_FRAME_TYPE_REMOTE),
#if CAN_FD_UNABLE
        .fd_enable = DRV_CAN_FD_ENABLED,
        .enable_brs = DRV_CAN_BRS_ENABLED,
        .fd_padding = 0
#else
        .fd_enable = DRV_CAN_FD_DISABLED,
        .enable_brs = DRV_CAN_BRS_DISABLED,
        .fd_padding = 0
#endif
    };

    status_t status_En = FLEXCAN_DRV_ConfigTxMb(DRV_CanInstances_arrst[instance_argen],
                                           bufferIdx_argu8,
                                           &txInfo,
                                           canId_u32);

    return (status_En == STATUS_SUCCESS) ? DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_CAN_Transmit_gen
*   Description   : Transmits CAN frame
*   Parameters    : instance_enarg - CAN instance
*                   bufferIdx_argu8 - Buffer index to use
*                   frameargst - Frame to transmit
*   Return Value  : DRV_CanStatus_En - Status of transmission
*  --------------------------------------------------------------------------- */
DRV_CanStatus_En DRV_CAN_Transmit_gen(DRV_CanInstance_En instance_enarg,
                            U8 bufferIdx_argu8,
                            const DRV_CanFrame_St_t* frameargst)
{
    const flexcan_data_info_t txInfo = {
        .msg_id_type = (frameargst->DRV_IdMode_En == DRV_CAN_ID_MODE_STANDARD) ?
                      FLEXCAN_MSG_ID_STD : FLEXCAN_MSG_ID_EXT,
        .data_length = frameargst->DRV_DataLength_u8,
        .is_remote = (frameargst->DRV_FrameType_En == DRV_CAN_FRAME_TYPE_REMOTE),
#if CAN_FD_UNABLE
        .fd_enable = DRV_CAN_FD_ENABLED,
        .enable_brs = DRV_CAN_BRS_ENABLED,
        .fd_padding = 0
#else
        .fd_enable = DRV_CAN_FD_DISABLED,
        .enable_brs = DRV_CAN_BRS_DISABLED,
        .fd_padding = 0
#endif
    };

    status_t status_En = FLEXCAN_DRV_Send(DRV_CanInstances_arrst[instance_enarg],
                                     bufferIdx_argu8,
                                     &txInfo,
                                     frameargst->DRV_CanId_u32,
                                     frameargst->DRV_Data_arru8);

    return (status_En == STATUS_SUCCESS) ? DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_CAN_Receive_gen
*   Description   : Receives CAN frame
*   Parameters    : instance_enarg - CAN instance
*                   bufferIdx_argu8 - Buffer index to receive from
*                   frame_argst - Frame structure to fill with received data
*   Return Value  : DRV_CanStatus_En - Status of receive operation
*  --------------------------------------------------------------------------- */
DRV_CanStatus_En DRV_CAN_Receive_gen(DRV_CanInstance_En instance_enarg,
                           U8 bufferIdx_argu8,
                           DRV_CanFrame_St_t* frame_argst)
{
    status_t status_En = FLEXCAN_DRV_Receive(DRV_CanInstances_arrst[instance_enarg], bufferIdx_argu8, &DRV_RecvBuff[bufferIdx_argu8]);
    frame_argst->DRV_DataLength_u8 = DRV_RecvBuff[bufferIdx_argu8].dataLen;
    frame_argst->DRV_CanId_u32 = DRV_RecvBuff[bufferIdx_argu8].msgId;
    frame_argst->DRV_IdMode_En = (DRV_RecvBuff[bufferIdx_argu8].cs & (1U << 21)) ?
                            DRV_CAN_ID_MODE_EXTENDED : DRV_CAN_ID_MODE_STANDARD;
    frame_argst->DRV_FrameType_En = (DRV_RecvBuff[bufferIdx_argu8].cs & (1U << 20)) ?
                              DRV_CAN_FRAME_TYPE_REMOTE : DRV_CAN_FRAME_TYPE_DATA;
    frame_argst->DRV_DataLength_u8 = DRV_RecvBuff[bufferIdx_argu8].dataLen;
    Common_Memcpy_gv(frame_argst->DRV_Data_arru8, DRV_RecvBuff[bufferIdx_argu8].data, DRV_RecvBuff[bufferIdx_argu8].dataLen);

    return (status_En == STATUS_SUCCESS) ? DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_CAN_ReceiveBlocking_gen
*   Description   : Receives CAN frame blocking some timeout
*   Parameters    : instance_enarg - CAN instance
*                   bufferIdx_argu8 - Buffer index to receive from
*                   frame_argst - Frame structure to fill with received data
*   Return Value  : DRV_CanStatus_En - Status of receive operation
*  --------------------------------------------------------------------------- */
DRV_CanStatus_En DRV_CAN_ReceiveBlocking_gen(DRV_CanInstance_En instance_enarg,
                           U8 bufferIdx_argu8,
                           DRV_CanFrame_St_t* frame_argst)
{
    status_t status_En = FLEXCAN_DRV_ReceiveBlocking(DRV_CanInstances_arrst[instance_enarg], bufferIdx_argu8, &DRV_RecvBuff[bufferIdx_argu8],CAN_TIMEOUT);
    frame_argst->DRV_DataLength_u8 = DRV_RecvBuff[bufferIdx_argu8].dataLen;
    frame_argst->DRV_CanId_u32 = DRV_RecvBuff[bufferIdx_argu8].msgId;
    frame_argst->DRV_IdMode_En = (DRV_RecvBuff[bufferIdx_argu8].cs & (1U << 21)) ?
                            DRV_CAN_ID_MODE_EXTENDED : DRV_CAN_ID_MODE_STANDARD;
    frame_argst->DRV_FrameType_En = (DRV_RecvBuff[bufferIdx_argu8].cs & (1U << 20)) ?
                              DRV_CAN_FRAME_TYPE_REMOTE : DRV_CAN_FRAME_TYPE_DATA;
    frame_argst->DRV_DataLength_u8 = DRV_RecvBuff[bufferIdx_argu8].dataLen;
    Common_Memcpy_gv(frame_argst->DRV_Data_arru8, DRV_RecvBuff[bufferIdx_argu8].data, DRV_RecvBuff[bufferIdx_argu8].dataLen);

    return (status_En == STATUS_SUCCESS) ? DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_CAN_Deinit_gen
*   Description   : Deinitializes CAN controller
*   Parameters    : instance_enarg - CAN instance to deinitialize
*   Return Value  : DRV_CanStatus_En - Status of deinitialization
*  --------------------------------------------------------------------------- */
DRV_CanStatus_En DRV_CAN_Deinit_gen(DRV_CanInstance_En instance_enarg)
{
    status_t status_En = FLEXCAN_DRV_Deinit(DRV_CanInstances_arrst[instance_enarg]);
    return (status_En == STATUS_SUCCESS) ? DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_CAN_SetRxFilter_gen
*   Description   : Sets CAN receive filter
*   Parameters    : instance_argen - CAN instance
*                   bufferIdx_argu8 - Buffer index to configure
*                   mask_argu32 - Filter mask
*                   idMode_argen - ID mode (standard/extended)
*   Return Value  : DRV_CanStatus_En - Status of filter configuration
*  --------------------------------------------------------------------------- */
DRV_CanStatus_En DRV_CAN_SetRxFilter_gen(DRV_CanInstance_En instance_argen,
                               U8 bufferIdx_argu8,
                               U32 mask_argu32,
                               DRV_CanIdMode_En idMode_argen)
{
    status_t status_En = FLEXCAN_DRV_SetRxIndividualMask(DRV_CanInstances_arrst[instance_argen],
                                                    (idMode_argen == DRV_CAN_ID_MODE_STANDARD) ?
                                                    FLEXCAN_MSG_ID_STD : FLEXCAN_MSG_ID_EXT,
                                                    bufferIdx_argu8,
                                                    mask_argu32);

    return (status_En == STATUS_SUCCESS) ? DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_ERROR;
}


