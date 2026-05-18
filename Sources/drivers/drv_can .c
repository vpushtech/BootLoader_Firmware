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
 *  28-Jul-2025 RUSHIKESH   CAN Testing Completed with PEAK CAN Device
 *  12-Aug-2025 RUSHIKESH   Guidelines Followed the naming Architecture Implementation
 *  29-Jan-2026 RUSHIKESH   Added validation, init tracking, and error handling
 *  01-May-2026 RUSHIKESH   MISRA C 2012 compliance applied
 ******************************************************************************/

/* ==================== INCLUDE FILES ==================== */

#include "drv_can.h"
#include "flexcan_driver.h"
#include "canCom1.h"
#include "common_types.h"
#include"can_communication.h"
/* ==================== COMPILE-TIME CONFIGURATION ==================== */
#define DRV_CAN_FD_CAPABLE  (0U)

/* ==================== PRIVATE HELPER MACRO ==================== */
#if (DRV_CAN_FD_CAPABLE == 1U)
#define DRV_CAN_FD_INFO_FIELDS  \
    .fd_enable  = (bool)DRV_CAN_FD_ENABLED,  \
    .enable_brs = (bool)DRV_CAN_BRS_ENABLED, \
    .fd_padding = 0U
#else
#define DRV_CAN_FD_INFO_FIELDS  \
    .fd_enable  = (bool)DRV_CAN_FD_DISABLED, \
    .enable_brs = (bool)DRV_CAN_BRS_DISABLED, \
    .fd_padding = 0U
#endif

/* ==================== STATIC VARIABLES ==================== */
static const U8 DRV_CanInstances_arrst[DRV_CAN_MAX_INSTANCE] =
{
    INST_CANCOM1
};
static const flexcan_user_config_t * const DRV_CanConfig_arrst[DRV_CAN_MAX_INSTANCE] =
{
    &canCom1_InitConfig0
};
static flexcan_msgbuff_t DRV_RecvBuff[DRV_CAN_MAX_BUFFERS];

/* ==================== GLOBAL VARIABLES ==================== */
volatile bool DRV_canInitStatus_b[DRV_CAN_MAX_INSTANCE] =
{
    false
};

/* ==================== PRIVATE FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_IsIdValid_mb
 *   Description   : Checks whether a CAN identifier falls within the valid
 *                   range for the requested ID mode.
 *   Parameters    : canId_argu32 - CAN identifier to validate.
 *                   idMode_argen - ID mode (DRV_CAN_ID_MODE_STANDARD or
 *                                  DRV_CAN_ID_MODE_EXTENDED).
 *   Return Value  : bool - true if the identifier is within range, else false.
 * -------------------------------------------------------------------------- */
static bool DRV_CAN_IsIdValid_mb(U32 canId_argu32, DRV_CanIdMode_ten idMode_argen)
{
    bool retVal;

    if (idMode_argen == DRV_CAN_ID_MODE_STANDARD)
    {
        retVal = (canId_argu32 <= DRV_CAN_STD_ID_MASK);
    }
    else if (idMode_argen == DRV_CAN_ID_MODE_EXTENDED)
    {
        retVal = (canId_argu32 <= DRV_CAN_EXT_ID_MASK);
    }
    else
    {
        retVal = false;
    }

    return retVal;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_SelectNvicIrq_mb
 *   Description   : Returns the NVIC IRQ line for the given CAN instance and
 *                   message buffer index.
 *   Parameters    : instance_argen  - CAN instance.
 *                   bufferIdx_argu8 - Message buffer index.
 *   Return Value  : DRV_NVIC_Irq_ten - The relevant NVIC IRQ identifier.
 * -------------------------------------------------------------------------- */
static DRV_NVIC_Irq_ten DRV_CAN_SelectNvicIrq_mb(DRV_CanInstance_ten instance_argen,
                                                   U8                  bufferIdx_argu8)
{
    DRV_NVIC_Irq_ten irq_en;
    switch (instance_argen)
    {
        case DRV_CAN_INSTANCE_0:
            if (bufferIdx_argu8 > DRV_CAN_IRQ_HIGH_BUF_THR)
            {
                irq_en = NVIC_CAN0_16_31_IRQ;
            }
            else
            {
                irq_en = NVIC_CAN0_0_15_IRQ;
            }
            break;

        default:
            irq_en = NVIC_CAN0_0_15_IRQ;
            break;
    }

    return irq_en;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_BuildDataInfo_mb
 *   Description   : Fills a flexcan_data_info_t structure from a driver frame
 *                   descriptor. CAN FD fields are set according to the
 *                   DRV_CAN_FD_CAPABLE compile-time constant.
 *   Parameters    : frame_argst      - Source driver frame descriptor.
 *                   dataInfo_argpst  - Destination SDK data-info structure
 *                                      (must not be NULL).
 *   Return Value  : void
 * -------------------------------------------------------------------------- */
static void DRV_CAN_BuildDataInfo_mb(const DRV_CanFrame_tst *frame_argst,
                                     flexcan_data_info_t    *dataInfo_argpst)
{
    dataInfo_argpst->msg_id_type =
        (frame_argst->DRV_IdMode_en == DRV_CAN_ID_MODE_STANDARD) ?
        FLEXCAN_MSG_ID_STD : FLEXCAN_MSG_ID_EXT;          /* DEV-CAN-001 */

    dataInfo_argpst->data_length = (uint32_t)frame_argst->DRV_DataLength_u8;
    dataInfo_argpst->is_remote   =
        (frame_argst->DRV_FrameType_en == DRV_CAN_FRAME_TYPE_REMOTE);

    /* CAN FD fields - driven by compile-time constant, no dead branch */
#if (DRV_CAN_FD_CAPABLE == 1U)
    dataInfo_argpst->fd_enable  = true;
    dataInfo_argpst->enable_brs = true;
#else
    dataInfo_argpst->fd_enable  = false;
    dataInfo_argpst->enable_brs = false;
#endif
    dataInfo_argpst->fd_padding = 0U;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_DecodeRxBuffer_mb
 *   Description   : Decodes a received FlexCAN message buffer into the driver
 *                   frame structure.
 *   Parameters    : recvBuff_argst - Source FlexCAN message buffer.
 *                   frame_argpst   - Destination driver frame (must not be NULL).
 *   Return Value  : void
 * -------------------------------------------------------------------------- */
static void DRV_CAN_DecodeRxBuffer_mb(const flexcan_msgbuff_t *recvBuff_argst,
                                       DRV_CanFrame_tst        *frame_argpst)
{
    U32 csWord_u32;
    U8  dataLen_u8;

    csWord_u32 = recvBuff_argst->cs;

    frame_argpst->DRV_CanId_u32 = recvBuff_argst->msgId;
    frame_argpst->DRV_IdMode_en =
        ((csWord_u32 & (1UL << DRV_CAN_CS_IDE_BIT)) != 0U) ?
        DRV_CAN_ID_MODE_EXTENDED : DRV_CAN_ID_MODE_STANDARD;

    frame_argpst->DRV_FrameType_en =
        ((csWord_u32 & (1UL << DRV_CAN_CS_RTR_BIT)) != 0U) ?
        DRV_CAN_FRAME_TYPE_REMOTE : DRV_CAN_FRAME_TYPE_DATA;

    dataLen_u8 = recvBuff_argst->dataLen;
    frame_argpst->DRV_DataLength_u8 = dataLen_u8;
    if (dataLen_u8 <= DRV_CAN_MAX_DATA_LENGTH)
    {
        Common_Memcpy_gst(frame_argpst->DRV_Data_arru8,
                          recvBuff_argst->data,
                          (U32)dataLen_u8);
    }
    else
    {
        frame_argpst->DRV_DataLength_u8 = 0U;
    }
}

/* ==================== PUBLIC FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_Init_gen
 *   Description   : Initializes the FlexCAN controller for the specified
 *                   instance. Installs the application event callback.
 *   Parameters    : instance_argen - CAN instance (DRV_CanInstance_ten).
 *   Return Value  : DRV_CanStatus_ten - DRV_CAN_STATUS_SUCCESS on success,
 *                                       otherwise an appropriate error code.
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_Init_gen(DRV_CanInstance_ten instance_argen)
{
    DRV_CanStatus_ten retStatus;
    status_t          halStatus;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == true)
    {
        retStatus = DRV_CAN_STATUS_SUCCESS;
    }
    else
    {
        halStatus = FLEXCAN_DRV_Init(DRV_CanInstances_arrst[instance_argen],
                                     &canCom1_State,
                                     DRV_CanConfig_arrst[instance_argen]);

        if (halStatus == STATUS_SUCCESS)
        {
            FLEXCAN_DRV_InstallEventCallback(DRV_CanInstances_arrst[instance_argen],
                                             CAN_Callback,
                                             NULL);
            DRV_canInitStatus_b[instance_argen] = true;
            retStatus = DRV_CAN_STATUS_SUCCESS;
        }
        else
        {
            retStatus = DRV_CAN_STATUS_ERROR;
        }
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_Deinit_gen
 *   Description   : Deinitializes the FlexCAN controller for the specified
 *                   instance and clears its initialization flag.
 *   Parameters    : instance_argen - CAN instance (DRV_CanInstance_ten).
 *   Return Value  : DRV_CanStatus_ten - DRV_CAN_STATUS_SUCCESS on success,
 *                                       otherwise an appropriate error code.
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_Deinit_gen(DRV_CanInstance_ten instance_argen)
{
    DRV_CanStatus_ten retStatus;
    status_t          halStatus;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == false)
    {
        retStatus = DRV_CAN_STATUS_NOT_INITIALIZED;
    }
    else
    {
        halStatus = FLEXCAN_DRV_Deinit(DRV_CanInstances_arrst[instance_argen]);

        if (halStatus == STATUS_SUCCESS)
        {
            DRV_canInitStatus_b[instance_argen] = false;
            retStatus = DRV_CAN_STATUS_SUCCESS;
        }
        else
        {
            retStatus = DRV_CAN_STATUS_ERROR;
        }
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_ConfigRxBuffer_gen
 *   Description   : Configures a FlexCAN message buffer for reception.
 *   Parameters    : instance_argen    - CAN instance.
 *                   bufferIdx_argu8   - Message buffer index [0..31].
 *                   frameConfig_argst - Frame configuration (ID mode, length).
 *                   rxMsgId_argu32    - CAN identifier to receive.
 *   Return Value  : DRV_CanStatus_ten
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_ConfigRxBuffer_gen(DRV_CanInstance_ten     instance_argen,
                                              U8                      bufferIdx_argu8,
                                              const DRV_CanFrame_tst *frameConfig_argst,
                                              U32                     rxMsgId_argu32)
{
    DRV_CanStatus_ten   retStatus;
    status_t            halStatus;
    flexcan_data_info_t rxInfo;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == false)
    {
        retStatus = DRV_CAN_STATUS_NOT_INITIALIZED;
    }
    else if (bufferIdx_argu8 >= (U8)DRV_CAN_MAX_BUFFERS)
    {
        retStatus = DRV_CAN_STATUS_INVALID_BUFFER_INDEX;
    }
    else if (frameConfig_argst == NULL)
    {
        retStatus = DRV_CAN_STATUS_NULL_POINTER;
    }
    else if (frameConfig_argst->DRV_DataLength_u8 > (U8)DRV_CAN_MAX_DATA_LENGTH)
    {
        retStatus = DRV_CAN_STATUS_INVALID_DATA_LENGTH;
    }
    else if (DRV_CAN_IsIdValid_mb(rxMsgId_argu32, frameConfig_argst->DRV_IdMode_en) == false)
    {
        retStatus = DRV_CAN_STATUS_INVALID_ID;
    }
    else
    {
        DRV_CAN_BuildDataInfo_mb(frameConfig_argst, &rxInfo);

        halStatus = FLEXCAN_DRV_ConfigRxMb(DRV_CanInstances_arrst[instance_argen],
                                           bufferIdx_argu8,
                                           &rxInfo,
                                           rxMsgId_argu32);

        retStatus = (halStatus == STATUS_SUCCESS) ?
                    DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_ERROR;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_ConfigTxBuffer_gen
 *   Description   : Configures a FlexCAN message buffer for transmission.
 *   Parameters    : instance_argen    - CAN instance.
 *                   bufferIdx_argu8   - Message buffer index [0..31].
 *                   frameConfig_argst - Frame configuration.
 *                   canId_argu32      - CAN identifier for transmission.
 *   Return Value  : DRV_CanStatus_ten
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_ConfigTxBuffer_gen(DRV_CanInstance_ten     instance_argen,
                                              U8                      bufferIdx_argu8,
                                              const DRV_CanFrame_tst *frameConfig_argst,
                                              U32                     canId_argu32)
{
    DRV_CanStatus_ten   retStatus;
    status_t            halStatus;
    flexcan_data_info_t txInfo;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == false)
    {
        retStatus = DRV_CAN_STATUS_NOT_INITIALIZED;
    }
    else if (bufferIdx_argu8 >= (U8)DRV_CAN_MAX_BUFFERS)
    {
        retStatus = DRV_CAN_STATUS_INVALID_BUFFER_INDEX;
    }
    else if (frameConfig_argst == NULL)
    {
        retStatus = DRV_CAN_STATUS_NULL_POINTER;
    }
    else if (frameConfig_argst->DRV_DataLength_u8 > (U8)DRV_CAN_MAX_DATA_LENGTH)
    {
        retStatus = DRV_CAN_STATUS_INVALID_DATA_LENGTH;
    }
    else if (DRV_CAN_IsIdValid_mb(canId_argu32, frameConfig_argst->DRV_IdMode_en) == false)
    {
        retStatus = DRV_CAN_STATUS_INVALID_ID;
    }
    else
    {
        DRV_CAN_BuildDataInfo_mb(frameConfig_argst, &txInfo);

        halStatus = FLEXCAN_DRV_ConfigTxMb(DRV_CanInstances_arrst[instance_argen],
                                           bufferIdx_argu8,
                                           &txInfo,
                                           canId_argu32);

        retStatus = (halStatus == STATUS_SUCCESS) ?
                    DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_ERROR;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_TransmitBlock_gen
 *   Description   : Transmits a CAN frame using blocking (polling) mode.
 *                   Verifies the relevant NVIC IRQ is enabled before sending.
 *   Parameters    : instance_argen  - CAN instance.
 *                   bufferIdx_argu8 - Message buffer index [0..31].
 *                   frame_argst     - Frame to transmit (must not be NULL).
 *   Return Value  : DRV_CanStatus_ten
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_TransmitBlock_gen(DRV_CanInstance_ten     instance_argen,
                                             U8                      bufferIdx_argu8,
                                             const DRV_CanFrame_tst *frame_argst)
{
    DRV_CanStatus_ten   retStatus;
    status_t            halStatus;
    DRV_NVIC_Irq_ten    nvicIrq_en;
    flexcan_data_info_t txInfo;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == false)
    {
        retStatus = DRV_CAN_STATUS_NOT_INITIALIZED;
    }
    else if (bufferIdx_argu8 >= (U8)DRV_CAN_MAX_BUFFERS)
    {
        retStatus = DRV_CAN_STATUS_INVALID_BUFFER_INDEX;
    }
    else if (frame_argst == NULL)
    {
        retStatus = DRV_CAN_STATUS_NULL_POINTER;
    }
    else if (frame_argst->DRV_DataLength_u8 > (U8)DRV_CAN_MAX_DATA_LENGTH)
    {
        retStatus = DRV_CAN_STATUS_INVALID_DATA_LENGTH;
    }
    else if (DRV_CAN_IsIdValid_mb(frame_argst->DRV_CanId_u32,
                                   frame_argst->DRV_IdMode_en) == false)
    {
        retStatus = DRV_CAN_STATUS_INVALID_ID;
    }
    else
    {
        nvicIrq_en = DRV_CAN_SelectNvicIrq_mb(instance_argen, bufferIdx_argu8);

        if (DRV_NVIC_IsIRQEnabled_gb(nvicIrq_en) != true)
        {
            retStatus = DRV_CAN_INTERRUPT_NOT_ENABLED;
        }
        else
        {
            DRV_CAN_BuildDataInfo_mb(frame_argst, &txInfo);

            halStatus = FLEXCAN_DRV_SendBlocking(
                            DRV_CanInstances_arrst[instance_argen],
                            bufferIdx_argu8,
                            &txInfo,
                            frame_argst->DRV_CanId_u32,
                            frame_argst->DRV_Data_arru8,
                            DRV_CAN_TIMEOUT);

            retStatus = (halStatus == STATUS_SUCCESS) ?
                        DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_TRANSMIT_FAILED;
        }
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_TransmitNonBlock_gen
 *   Description   : Transmits a CAN frame using non-blocking (interrupt) mode.
 *   Parameters    : instance_argen  - CAN instance.
 *                   bufferIdx_argu8 - Message buffer index [0..31].
 *                   frame_argst     - Frame to transmit (must not be NULL).
 *   Return Value  : DRV_CanStatus_ten
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_TransmitNonBlock_gen(DRV_CanInstance_ten     instance_argen,
                                                U8                      bufferIdx_argu8,
                                                const DRV_CanFrame_tst *frame_argst)
{
    DRV_CanStatus_ten   retStatus;
    status_t            halStatus;
    flexcan_data_info_t txInfo;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == false)
    {
        retStatus = DRV_CAN_STATUS_NOT_INITIALIZED;
    }
    else if (bufferIdx_argu8 >= (U8)DRV_CAN_MAX_BUFFERS)
    {
        retStatus = DRV_CAN_STATUS_INVALID_BUFFER_INDEX;
    }
    else if (frame_argst == NULL)
    {
        retStatus = DRV_CAN_STATUS_NULL_POINTER;
    }
    else if (frame_argst->DRV_DataLength_u8 > (U8)DRV_CAN_MAX_DATA_LENGTH)
    {
        retStatus = DRV_CAN_STATUS_INVALID_DATA_LENGTH;
    }
    else if (DRV_CAN_IsIdValid_mb(frame_argst->DRV_CanId_u32,
                                   frame_argst->DRV_IdMode_en) == false)
    {
        retStatus = DRV_CAN_STATUS_INVALID_ID;
    }
    else
    {
        DRV_CAN_BuildDataInfo_mb(frame_argst, &txInfo);

        halStatus = FLEXCAN_DRV_Send(DRV_CanInstances_arrst[instance_argen],
                                     bufferIdx_argu8,
                                     &txInfo,
                                     frame_argst->DRV_CanId_u32,
                                     frame_argst->DRV_Data_arru8);

        retStatus = (halStatus == STATUS_SUCCESS) ?
                    DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_TRANSMIT_FAILED;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_ReceiveBlock_gen
 *   Description   : Receives a CAN frame using blocking (polling) mode.
 *                   Verifies the relevant NVIC IRQ is enabled.
 *   Parameters    : instance_argen  - CAN instance.
 *                   bufferIdx_argu8 - Message buffer index [0..31].
 *                   frame_argst     - Output frame (must not be NULL).
 *   Return Value  : DRV_CanStatus_ten
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_ReceiveBlock_gen(DRV_CanInstance_ten  instance_argen,
                                           U8                   bufferIdx_argu8,
                                           DRV_CanFrame_tst    *frame_argst)
{
    DRV_CanStatus_ten retStatus;
    status_t          halStatus;
    DRV_NVIC_Irq_ten  nvicIrq_en;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == false)
    {
        retStatus = DRV_CAN_STATUS_NOT_INITIALIZED;
    }
    else if (bufferIdx_argu8 >= (U8)DRV_CAN_MAX_BUFFERS)
    {
        retStatus = DRV_CAN_STATUS_INVALID_BUFFER_INDEX;
    }
    else if (frame_argst == NULL)
    {
        retStatus = DRV_CAN_STATUS_NULL_POINTER;
    }
    else
    {
        nvicIrq_en = DRV_CAN_SelectNvicIrq_mb(instance_argen, bufferIdx_argu8);

        if (DRV_NVIC_IsIRQEnabled_gb(nvicIrq_en) != true)
        {
            retStatus = DRV_CAN_INTERRUPT_NOT_ENABLED;
        }
        else
        {
            halStatus = FLEXCAN_DRV_ReceiveBlocking(
                            DRV_CanInstances_arrst[instance_argen],
                            bufferIdx_argu8,
                            &DRV_RecvBuff[bufferIdx_argu8],
                            DRV_CAN_TIMEOUT);

            if (halStatus != STATUS_SUCCESS)
            {
                retStatus = DRV_CAN_STATUS_RECEIVE_FAILED;
            }
            else
            {
                frame_argst->DRV_CanId_u32      = 0U;
                frame_argst->DRV_DataLength_u8  = 0U;
                Common_Memcpy_gst(frame_argst->DRV_Data_arru8,
                                  0,
                                  sizeof(frame_argst->DRV_Data_arru8));

                DRV_CAN_DecodeRxBuffer_mb(&DRV_RecvBuff[bufferIdx_argu8],
                                          frame_argst);
                retStatus = DRV_CAN_STATUS_SUCCESS;
            }
        }
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_ReceiveNonBlock_gen
 *   Description   : Starts a non-blocking (interrupt-driven) CAN receive
 *                   operation. Frame data is populated from the receive buffer.
 *   Parameters    : instance_argen  - CAN instance.
 *                   bufferIdx_argu8 - Message buffer index [0..31].
 *                   frame_argst     - Output frame (must not be NULL).
 *   Return Value  : DRV_CanStatus_ten
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_ReceiveNonBlock_gen(DRV_CanInstance_ten  instance_argen,
                                              U8                   bufferIdx_argu8,
                                              DRV_CanFrame_tst    *frame_argst)
{
    DRV_CanStatus_ten retStatus;
    status_t          halStatus;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == false)
    {
        retStatus = DRV_CAN_STATUS_NOT_INITIALIZED;
    }
    else if (bufferIdx_argu8 >= (U8)DRV_CAN_MAX_BUFFERS)
    {
        retStatus = DRV_CAN_STATUS_INVALID_BUFFER_INDEX;
    }
    else if (frame_argst == NULL)
    {
        retStatus = DRV_CAN_STATUS_NULL_POINTER;
    }
    else
    {
        halStatus = FLEXCAN_DRV_Receive(DRV_CanInstances_arrst[instance_argen],
                                        bufferIdx_argu8,
                                        &DRV_RecvBuff[bufferIdx_argu8]);

        if (halStatus != STATUS_SUCCESS)
        {
            retStatus = DRV_CAN_STATUS_RECEIVE_FAILED;
        }
        else
        {
            frame_argst->DRV_CanId_u32     = 0U;
            frame_argst->DRV_DataLength_u8 = 0U;
            Common_Memcpy_gst(frame_argst->DRV_Data_arru8,
                              0,
                              sizeof(frame_argst->DRV_Data_arru8));

            DRV_CAN_DecodeRxBuffer_mb(&DRV_RecvBuff[bufferIdx_argu8],
                                      frame_argst);
            retStatus = DRV_CAN_STATUS_SUCCESS;
        }
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_SetRxFilter_gen
 *   Description   : Configures the individual receive mask for the specified
 *                   message buffer.
 *   Parameters    : instance_argen  - CAN instance.
 *                   bufferIdx_argu8 - Message buffer index [0..31].
 *                   mask_argu32     - Filter mask value.
 *                   idMode_argen    - ID mode (standard / extended).
 *   Return Value  : DRV_CanStatus_ten
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_SetRxFilter_gen(DRV_CanInstance_ten instance_argen,
                                           U8                  bufferIdx_argu8,
                                           U32                 mask_argu32,
                                           DRV_CanIdMode_ten   idMode_argen)
{
    DRV_CanStatus_ten        retStatus;
    status_t                 halStatus;
    flexcan_msgbuff_id_type_t sdkIdType;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == false)
    {
        retStatus = DRV_CAN_STATUS_NOT_INITIALIZED;
    }
    else if (bufferIdx_argu8 >= (U8)DRV_CAN_MAX_BUFFERS)
    {
        retStatus = DRV_CAN_STATUS_INVALID_BUFFER_INDEX;
    }
    else
    {
        sdkIdType = (idMode_argen == DRV_CAN_ID_MODE_STANDARD) ?
                    FLEXCAN_MSG_ID_STD : FLEXCAN_MSG_ID_EXT;

        halStatus = FLEXCAN_DRV_SetRxIndividualMask(
                        DRV_CanInstances_arrst[instance_argen],
                        sdkIdType,
                        bufferIdx_argu8,
                        mask_argu32);

        retStatus = (halStatus == STATUS_SUCCESS) ?
                    DRV_CAN_STATUS_SUCCESS : DRV_CAN_STATUS_ERROR;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_EnablePretendedNetworking_gen
 *   Description   : Configures and enables Pretended Networking (PN) mode for
 *                   low-power CAN wakeup using the NXP SDK driver.
 *   Parameters    : instance_argen  - CAN instance.
 *                   pnConfig_argst  - Pointer to PN configuration (must not be NULL).
 *   Return Value  : DRV_CanStatus_ten
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_EnablePretendedNetworking_gen(
                          DRV_CanInstance_ten       instance_argen,
                          const DRV_CanPnConfig_tst *pnConfig_argst)
{
    DRV_CanStatus_ten  retStatus;
    flexcan_pn_config_t sdkPnCfg;
    bool               isExtended;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == false)
    {
        retStatus = DRV_CAN_STATUS_NOT_INITIALIZED;
    }
    else if (pnConfig_argst == NULL)
    {
        retStatus = DRV_CAN_STATUS_NULL_POINTER;
    }
    else
    {
        isExtended = (pnConfig_argst->idMode == DRV_CAN_ID_MODE_EXTENDED);
        sdkPnCfg.wakeUpTimeout             = pnConfig_argst->enableTimeoutWakeup;
        sdkPnCfg.wakeUpMatch               = pnConfig_argst->enableMatchWakeup;
        sdkPnCfg.numMatches                = 1U;
        sdkPnCfg.matchTimeout              = (uint16_t)pnConfig_argst->timeoutValue;
        sdkPnCfg.filterComb                = FLEXCAN_FILTER_ID;
        sdkPnCfg.idFilter1.extendedId      = isExtended;
        sdkPnCfg.idFilter1.remoteFrame     = false;
        sdkPnCfg.idFilter1.id              = pnConfig_argst->matchId;
        sdkPnCfg.idFilter2.extendedId      = isExtended;
        sdkPnCfg.idFilter2.remoteFrame     = false;
        sdkPnCfg.idFilter2.id              = DRV_CAN_STD_ID_MASK;
        sdkPnCfg.idFilterType              = FLEXCAN_FILTER_MATCH_EXACT;
        sdkPnCfg.payloadFilterType         = FLEXCAN_FILTER_MATCH_EXACT;

        Common_Memcpy_gst(&sdkPnCfg.payloadFilter, 0, sizeof(sdkPnCfg.payloadFilter));
        FLEXCAN_DRV_ConfigPN(DRV_CanInstances_arrst[instance_argen],
                             true,
                             &sdkPnCfg);

        retStatus = DRV_CAN_STATUS_SUCCESS;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_DisablePretendedNetworking_gen
 *   Description   : Disables Pretended Networking mode, returning the CAN
 *                   controller to normal operation.
 *   Parameters    : instance_argen - CAN instance.
 *   Return Value  : DRV_CanStatus_ten
 * -------------------------------------------------------------------------- */
DRV_CanStatus_ten DRV_CAN_DisablePretendedNetworking_gen(DRV_CanInstance_ten instance_argen)
{
    DRV_CanStatus_ten retStatus;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retStatus = DRV_CAN_STATUS_INVALID_INSTANCE;
    }
    else if (DRV_canInitStatus_b[instance_argen] == false)
    {
        retStatus = DRV_CAN_STATUS_NOT_INITIALIZED;
    }
    else
    {
        FLEXCAN_DRV_ConfigPN(DRV_CanInstances_arrst[instance_argen], false, NULL);
        retStatus = DRV_CAN_STATUS_SUCCESS;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  ---------------------------------------------------------------------------
 *   Function Name : DRV_CAN_GetPnWakeupReason_gen
 *   Description   : Reads and returns the Pretended Networking wakeup reason
 *                   bitmask. Clears the asserted flags after reading
 *                   (write-1-to-clear register semantics).
 *   Parameters    : instance_argen - CAN instance.
 *   Return Value  : DRV_CanPnWakeup_ten - Bitmask of detected wakeup events,
 *                                         or DRV_CAN_PN_WAKEUP_NONE on error.
 * -------------------------------------------------------------------------- */
DRV_CanPnWakeup_ten DRV_CAN_GetPnWakeupReason_gen(DRV_CanInstance_ten instance_argen)
{
    DRV_CanPnWakeup_ten retReason;
    U32                 wuMtc_u32;
    U32                 reasonFlags_u32;

    if (instance_argen >= DRV_CAN_MAX_INSTANCE)
    {
        retReason = DRV_CAN_PN_WAKEUP_NONE;
    }
    else
    {
        wuMtc_u32 = 0U;
        switch (instance_argen)
        {
            case DRV_CAN_INSTANCE_0:
                wuMtc_u32 = CAN0->WU_MTC;
                break;

            default:
                wuMtc_u32 = 0U;
                break;
        }

        reasonFlags_u32 = (U32)DRV_CAN_PN_WAKEUP_NONE;
        if ((wuMtc_u32 & CAN_WU_MTC_WTOF_MASK) != 0U)
        {
            reasonFlags_u32 |= (U32)DRV_CAN_PN_WAKEUP_TIMEOUT;
            switch (instance_argen)
            {
                case DRV_CAN_INSTANCE_0:
                    CAN0->WU_MTC |= CAN_WU_MTC_WTOF_MASK;
                    break;

                default:
                    break;
            }
        }
        else
        {

        }
        if ((wuMtc_u32 & CAN_WU_MTC_WUMF_MASK) != 0U)
        {
            reasonFlags_u32 |= (U32)DRV_CAN_PN_WAKEUP_MATCH;

            switch (instance_argen)
            {
                case DRV_CAN_INSTANCE_0:
                    CAN0->WU_MTC |= CAN_WU_MTC_WUMF_MASK;
                    break;

                default:
                    break;
            }
        }
        else
        {

        }
        retReason = (DRV_CanPnWakeup_ten)reasonFlags_u32;
    }

    return retReason;
}
