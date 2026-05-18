/*
 * ota_bootloader.c
 * Author: PandurangaKarapothul
 * Description: BLE OTA Bootloader Implementation - Handles firmware updates via UART
 * Modification History:
 * Date            Author              Description
 * ----------------------------------------------------------------------------
 *PandurangaKarapothul Initial OTA bootloader implementation
 *PandurangaKarapothul Added state machine for frame reception
 *PandurangaKarapothul Added checksum verification
 ******************************************************************************/

/* ==================== INCLUDES ==================== */
#include <bootloader/ota_bootloader.h>
#include "drv_nvic.h"
#include "bsp_config.h"

/* ==================== STATIC VARIABLES ==================== */
static U8  OTA_Index_u8           = 1U;
static U8  OTA_RxIndex_u8         = 0U;
static U8  OTA_DataCount_u8       = 0U;
static U8  OTA_PreviousCommand_u8 = 0U;
static U16 OTA_TotalFrameLen_u16  = OTA_FRAME_OVERHEAD;
static BIN     OTA_RxInitiated_b      = false;
BIN uart_flag_b=false;
/* ==================== GLOBAL VARIABLES ==================== */
U8         OTA_UartTxErrorCount_u8 = 0U;
U8         OTA_UartRxErrorCount_u8 = 0U;
BIN            OTA_UartTxInit_b        = false;
BIN            OTA_UartRxInit_b        = true;
U8         OTA_UartRxByte_u8       = 0U;

/* OTA protocol global structures */
OTA_Packet_St   OTA_Packet_st  = {0};
OTA_Ack_ten     OTA_Ack_en     = OTA_ACK_IDLE_E;
OTA_RxState_ten OTA_RxState_en = OTA_RX_STATE_START_OF_FRAME_E;
OTA_Command_ten OTA_Command_en = OTA_CMD_OTA_UPDATE_E;
U8         OTA_StatusIndex_u8 = 0U;

/* ==================== STATIC FUNCTION PROTOTYPES ==================== */
static void OTA_RxStateMachine_gv(U8 rxData_argu8);
static void OTA_Reset_gv(void);
static void OTA_StopReset_gv(void);

/* ==================== PUBLIC FUNCTIONS ==================== */

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : OTA_Init_gv
 * ----------------------------------------------------------------------------
 * Description   : Initializes OTA bootloader module and UART communication
 * Parameters    : void
 * Return Value  : void
 * Notes         : Sets up UART with NVIC interrupt for non-blocking reception
 * --------------------------------------------------------------------------*/
void OTA_Init_gv(void)
{
    /* Reset packet structure flags */
    OTA_Packet_st.receiveFlag_b  = false;
    OTA_Packet_st.transmitFlag_b = false;

    /* Initialize state machine variables */
    OTA_RxState_en               = OTA_RX_STATE_START_OF_FRAME_E;
    OTA_Index_u8                 = 1U;
    OTA_RxIndex_u8               = 0U;
    OTA_DataCount_u8             = 0U;
    OTA_TotalFrameLen_u16        = OTA_FRAME_OVERHEAD;
    OTA_UartRxInit_b             = true;

    /* Initialize UART peripheral for OTA communication */
    DRV_UART_Init_gen(OTA_UART_INSTANCE);

    /* Configure NVIC interrupt for UART with priority 6 */
    DRV_NVIC_IRQConfig_gen(NVIC_LPUART1_IRQ, 6);

    /* Start non-blocking UART read */
    DRV_UART_ReadNonBlock_gen(OTA_UART_INSTANCE, &OTA_UartRxByte_u8, 1U);
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : OTA_ProcessRxData_gv
 * ----------------------------------------------------------------------------
 * Description   : Processes received UART byte and builds OTA frame
 * Parameters    : rxData_argu8 - Received byte from UART
 * Return Value  : void
 * Notes         : Called from UART callback on each received byte
 * --------------------------------------------------------------------------*/
void OTA_ProcessRxData_gv(U8 rxData_argu8)
{
    OTA_RxInitiated_b = true;

    /* Only process if not already receiving a frame */
    if (!OTA_Packet_st.receiveFlag_b)
    {
        /* Store byte in receive buffer */
        OTA_Packet_st.rxBuffer_au8[OTA_RxIndex_u8] = rxData_argu8;

        /* Run state machine to process the byte */
        OTA_RxStateMachine_gv(rxData_argu8);
        OTA_RxIndex_u8++;

        /* Extract payload length when enough bytes received */
        if (OTA_RxIndex_u8 == (OTA_PAYLOAD_LEN_LSB_INDEX + 1U))
        {
            U16 payloadLen =
                ((U16)OTA_Packet_st.rxBuffer_au8[OTA_PAYLOAD_LEN_MSB_INDEX] << 8U)
                | (U16)OTA_Packet_st.rxBuffer_au8[OTA_PAYLOAD_LEN_LSB_INDEX];
            OTA_TotalFrameLen_u16 = payloadLen + (U16)OTA_FRAME_OVERHEAD;
        }

        /* Check if complete frame has been received */
        if (OTA_RxIndex_u8 >= OTA_TotalFrameLen_u16)
        {
            OTA_UartRxErrorCount_u8     = 0U;
            OTA_UartRxInit_b            = false;
            OTA_UartTxInit_b            = true;
            OTA_Packet_st.receiveFlag_b = true;
        }
    }
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : OTA_TxScheduler_gv
 * ----------------------------------------------------------------------------
 * Description   : Main OTA scheduler - processes received frames and sends responses
 * Parameters    : void
 * Return Value  : void
 * Notes         : Must be called regularly in main loop
 * --------------------------------------------------------------------------*/
void OTA_TxScheduler_gv(void)
{
    /* Process received frame if available and not already transmitting */
    if (OTA_Packet_st.receiveFlag_b && !OTA_Packet_st.transmitFlag_b)
    {
        U8 ack[1] = {OTA_ACK_FAIL};

        switch (OTA_Ack_en)
        {
            case OTA_ACK_OTA_UPDATE_E:
                /* Process OTA update command */
                OTA_ProcessOtaCommand_gv(
                    OTA_Packet_st.rxPayloadData_au8,
                    OTA_Packet_st.rxPayloadLength_u16);
                break;

            case OTA_ACK_RECEIVED_DATA_INVALID_E:
                /* Send FAIL ACK for invalid data */
                ack[0] = OTA_ACK_FAIL;
                OTA_SendData_gv(OTA_CMD_OTA_UPDATE_E, 1U, ack);
                break;

            case OTA_ACK_IDLE_E:
            default:
                /* Reset for next frame reception */
                OTA_Ack_en                        = OTA_ACK_IDLE_E;
                OTA_RxIndex_u8                    = 0U;
                OTA_TotalFrameLen_u16             = OTA_FRAME_OVERHEAD;
                OTA_Packet_st.rxPayloadLength_u16 = 0U;
                OTA_Packet_st.receiveFlag_b       = false;
                OTA_Packet_st.transmitFlag_b      = false;
                /* Start next non-blocking read */
                DRV_UART_ReadNonBlock_gen(
                    OTA_UART_INSTANCE, &OTA_UartRxByte_u8, 1U);
                break;
        }
    }

    /* Handle error conditions */
    if (OTA_UartRxErrorCount_u8 > 5U)
        OTA_Reset_gv();
    else if (OTA_UartRxErrorCount_u8 > 3U)
        OTA_StopReset_gv();
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : OTA_ProcessOtaCommand_gv
 * ----------------------------------------------------------------------------
 * Description   : Processes OTA command by calling BSP handler
 * Parameters    : payload - Pointer to payload data
 *                 payloadLen - Length of payload in bytes
 * Return Value  : void
 * --------------------------------------------------------------------------*/
void OTA_ProcessOtaCommand_gv(U8 *payload, U16 payloadLen)
{
    if ((payload == NULL) || (payloadLen == 0U))
        return;

    BSP_ProcessOtaSubCommand_mv(payload, payloadLen);
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : OTA_SendData_gv
 * ----------------------------------------------------------------------------
 * Description   : Sends OTA response frame over UART
 * Parameters    : cmd - Command type
 *                 payloadLen - Length of payload in bytes
 *                 payload - Pointer to payload data
 * Return Value  : void
 * Notes         : Constructs complete frame with SOF, checksum, and EOF
 * --------------------------------------------------------------------------*/
void OTA_SendData_gv(OTA_Command_ten cmd, U16 payloadLen, U8 *payload)
{
    U8            ti = 0U;
    U8            pi = 0U;
    U8            csbuf[OTA_MAX_PAYLOAD_BYTES + 1U];
    DRV_uartStatus_ten txStatus;

    /* Build frame header */
    OTA_Packet_st.txBuffer_au8[ti++] = (U8)((OTA_SOF >> 8U) & 0xFFU);
    OTA_Packet_st.txBuffer_au8[ti++] = (U8)( OTA_SOF        & 0xFFU);
    OTA_Packet_st.txBuffer_au8[ti++] = OTA_DEVICE_TO_HOST;
    OTA_Packet_st.txBuffer_au8[ti++] = (U8)cmd;

    /* Payload length field (MSB then LSB) */
    OTA_Packet_st.txBuffer_au8[ti++] = (U8)((payloadLen >> 8U) & 0xFFU);
    OTA_Packet_st.txBuffer_au8[ti++] = (U8)( payloadLen        & 0xFFU);

    csbuf[0] = (U8)cmd;
    for (pi = 0U; pi < (U8)payloadLen; pi++)
    {
        OTA_Packet_st.txBuffer_au8[ti++] = payload[pi];
        csbuf[pi + 1U]                    = payload[pi];
    }

    /* Add checksum byte */
    OTA_Packet_st.txBuffer_au8[ti++] =
        OTA_CalculateChecksum_gv(csbuf, (U8)(pi + 1U));

    /* Add frame trailer (EOF) */
    OTA_Packet_st.txBuffer_au8[ti++] = (U8)((OTA_EOF >> 8U) & 0xFFU);
    OTA_Packet_st.txBuffer_au8[ti++] = (U8)( OTA_EOF        & 0xFFU);

    /* Transmit frame */
    txStatus = DRV_UART_WriteBlock_gen(
        OTA_UART_INSTANCE, OTA_Packet_st.txBuffer_au8, ti);

    if (txStatus == DRV_UART_SUCCESS)
    {
        /* Reset for next frame */
        OTA_UartTxInit_b                  = false;
        OTA_UartRxInit_b                  = true;
        OTA_Packet_st.receiveFlag_b       = false;
        OTA_Packet_st.transmitFlag_b      = false;
        OTA_UartTxErrorCount_u8           = 0U;
        OTA_RxIndex_u8                    = 0U;
        OTA_TotalFrameLen_u16             = OTA_FRAME_OVERHEAD;
        OTA_Packet_st.rxPayloadLength_u16 = 0U;
        OTA_Ack_en                        = OTA_ACK_IDLE_E;

        /* Clear buffers */
        OTA_ResetBuffer_gv(OTA_Packet_st.rxBuffer_au8,
                           (U8)sizeof(OTA_Packet_st.rxBuffer_au8));
        OTA_ResetBuffer_gv(OTA_Packet_st.txBuffer_au8,
                           (U8)sizeof(OTA_Packet_st.txBuffer_au8));
    }
    else
    {
        OTA_UartTxErrorCount_u8++;
    }
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : OTA_CalculateChecksum_gv
 * ----------------------------------------------------------------------------
 * Description   : Calculates checksum for OTA frame (inverted sum)
 * Parameters    : data - Pointer to data buffer
 *                 len - Length of data in bytes
 * Return Value  : U8 - Calculated checksum value
 * Notes         : Checksum = ~(sum of all bytes)
 * --------------------------------------------------------------------------*/
U8 OTA_CalculateChecksum_gv(U8 *data, U8 len)
{
    U8 i, cs = 0U;
    for (i = 0U; i < len; i++)
        cs = (U8)(cs + data[i]);
    return (U8)(0xFFU & (U8)(~cs));
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : OTA_ResetBuffer_gv
 * ----------------------------------------------------------------------------
 * Description   : Resets buffer by filling with zeros
 * Parameters    : buf - Pointer to buffer
 *                 len - Length of buffer in bytes
 * Return Value  : void
 * --------------------------------------------------------------------------*/
void OTA_ResetBuffer_gv(U8 *buf, U8 len)
{
    U8 i;
    for (i = 0U; i < len; i++)
        buf[i] = 0x00U;
}

/* ==================== STATIC FUNCTIONS ==================== */

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : OTA_RxStateMachine_gv
 * ----------------------------------------------------------------------------
 * Description   : State machine for OTA frame reception
 * Parameters    : rx - Received byte
 * Return Value  : void
 * Notes         : Implements OTA frame protocol parsing
 * --------------------------------------------------------------------------*/
static void OTA_RxStateMachine_gv(U8 rx)
{
    U8 cs = 0U, csi = 0U;
    U8 csbuf[OTA_MAX_PAYLOAD_BYTES + 1U];
    static U8 s_lenByteCount_u8 = 0U;

    switch (OTA_RxState_en)
    {
        case OTA_RX_STATE_START_OF_FRAME_E:
            if (OTA_Index_u8 == 2U)
            {
                /* Check SOF LSB */
                if ((U8)(OTA_SOF & 0xFFU) == rx)
                {
                    OTA_RxState_en = OTA_RX_STATE_DIRECTION_E;
                    OTA_Index_u8   = 1U;
                }
                else
                {
                    OTA_Packet_st.rxCommand_u8 = (U8)OTA_CMD_INVALID_E;
                    OTA_RxState_en = OTA_RX_STATE_DATA_INVALID_E;
                    OTA_Index_u8   = 1U;
                }
            }
            else
            {
                /* Check SOF MSB */
                if ((U8)((OTA_SOF >> 8U) & 0xFFU) == rx)
                    OTA_Index_u8++;
                else
                {
                    OTA_Packet_st.rxCommand_u8 = (U8)OTA_CMD_INVALID_E;
                    OTA_RxState_en = OTA_RX_STATE_DATA_INVALID_E;
                    OTA_Index_u8   = 1U;
                }
            }
            break;

        case OTA_RX_STATE_DIRECTION_E:
            if (OTA_HOST_TO_DEVICE == rx)
                OTA_RxState_en = OTA_RX_STATE_COMMAND_ID_E;
            else
            {
                OTA_Packet_st.rxCommand_u8 = (U8)OTA_CMD_INVALID_E;
                OTA_RxState_en = OTA_RX_STATE_DATA_INVALID_E;
            }
            break;

        case OTA_RX_STATE_COMMAND_ID_E:
            if ((U8)OTA_CMD_OTA_UPDATE_E == rx)
            {
                OTA_Packet_st.rxCommand_u8        = rx;
                OTA_Ack_en                         = OTA_ACK_OTA_UPDATE_E;
                s_lenByteCount_u8                  = 0U;
                OTA_Packet_st.rxPayloadLength_u16  = 0U;
                OTA_RxState_en = OTA_RX_STATE_PAYLOAD_LENGTH_E;
            }
            else
            {
                OTA_Packet_st.rxCommand_u8 = (U8)OTA_CMD_INVALID_E;
                OTA_RxState_en = OTA_RX_STATE_DATA_INVALID_E;
            }
            break;


        case OTA_RX_STATE_PAYLOAD_LENGTH_E:
            if (s_lenByteCount_u8 == 0U)
            {
                /* MSB of payload length */
                OTA_Packet_st.rxPayloadLength_u16 = (U16)((U16)rx << 8U);
                s_lenByteCount_u8 = 1U;
            }
            else
            {
                /* LSB of payload length */
                OTA_Packet_st.rxPayloadLength_u16 |= (U16)rx;
                s_lenByteCount_u8 = 0U;
                OTA_DataCount_u8  = 0U;
                OTA_RxState_en    = OTA_RX_STATE_PAYLOAD_DATA_E;
            }
            break;

        case OTA_RX_STATE_PAYLOAD_DATA_E:
            if (OTA_DataCount_u8 < (U8)OTA_Packet_st.rxPayloadLength_u16)
            {
                OTA_Packet_st.rxPayloadData_au8[OTA_DataCount_u8] = rx;
                OTA_DataCount_u8++;
            }
            if (OTA_DataCount_u8 == (U8)OTA_Packet_st.rxPayloadLength_u16)
            {
                OTA_RxState_en   = OTA_RX_STATE_CHECKSUM_E;
                OTA_DataCount_u8 = 0U;
            }
            break;

        case OTA_RX_STATE_CHECKSUM_E:
            /* Build checksum buffer from command and payload */
            csbuf[0] = OTA_Packet_st.rxCommand_u8;
            for (csi = 1U; csi <= (U8)OTA_Packet_st.rxPayloadLength_u16; csi++)
                csbuf[csi] = OTA_Packet_st.rxPayloadData_au8[csi - 1U];

            /* Calculate and compare checksum */
            cs = OTA_CalculateChecksum_gv(csbuf, csi);
            OTA_RxState_en = (cs == rx) ? OTA_RX_STATE_END_OF_FRAME_E
                                        : OTA_RX_STATE_DATA_INVALID_E;
            break;

        case OTA_RX_STATE_END_OF_FRAME_E:
            if (OTA_Index_u8 == 2U)
            {
                /* Check EOF LSB */
                if ((U8)(OTA_EOF & 0xFFU) == rx)
                {
                    DRV_WDG_Refresh_gen(DRV_WDG_INSTANCE_1);
                    OTA_PreviousCommand_u8       = OTA_Packet_st.rxCommand_u8;
                    OTA_RxState_en               = OTA_RX_STATE_START_OF_FRAME_E;
                    OTA_Index_u8                 = 1U;
                    OTA_Packet_st.transmitFlag_b = false;
                }
                else
                {
                    OTA_RxState_en = OTA_RX_STATE_DATA_INVALID_E;
                    OTA_Index_u8   = 1U;
                }
            }
            else
            {
                /* Check EOF MSB */
                if ((U8)((OTA_EOF >> 8U) & 0xFFU) == rx)
                    OTA_Index_u8++;
                else
                {
                    OTA_RxState_en = OTA_RX_STATE_DATA_INVALID_E;
                    OTA_Index_u8   = 1U;
                }
            }
            break;


        case OTA_RX_STATE_DATA_INVALID_E:
            OTA_Ack_en                        = OTA_ACK_RECEIVED_DATA_INVALID_E;
            OTA_Packet_st.rxPayloadLength_u16 = 0U;
            OTA_RxState_en                    = OTA_RX_STATE_START_OF_FRAME_E;
            OTA_Packet_st.transmitFlag_b      = false;
            OTA_UartRxErrorCount_u8++;
            break;

        default:
            break;
    }
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : OTA_StopReset_gv
 * ----------------------------------------------------------------------------
 * Description   : Partial reset of OTA module (keeps some state)
 * Parameters    : void
 * Return Value  : void
 * --------------------------------------------------------------------------*/
static void OTA_StopReset_gv(void)
{
    /* Reset counters and indices */
    OTA_Index_u8          = 1U;
    OTA_RxIndex_u8        = 0U;
    OTA_DataCount_u8      = 0U;
    OTA_TotalFrameLen_u16 = OTA_FRAME_OVERHEAD;
    OTA_StatusIndex_u8    = 0U;
    OTA_UartTxInit_b      = false;
    OTA_UartRxInit_b      = true;
    OTA_Ack_en            = OTA_ACK_IDLE_E;
    OTA_RxState_en        = OTA_RX_STATE_START_OF_FRAME_E;

    /* Reset packet structure */
    OTA_Packet_st.receiveFlag_b       = false;
    OTA_Packet_st.transmitFlag_b      = false;
    OTA_Packet_st.rxCommand_u8        = 0U;
    OTA_Packet_st.rxPayloadLength_u16 = 0U;
    OTA_Packet_st.txPayloadLength_u8  = 0U;

    /* Clear all buffers */
    OTA_ResetBuffer_gv(OTA_Packet_st.txBuffer_au8,
                       (U8)sizeof(OTA_Packet_st.txBuffer_au8));
    OTA_ResetBuffer_gv(OTA_Packet_st.rxBuffer_au8,
                       (U8)sizeof(OTA_Packet_st.rxBuffer_au8));
    OTA_ResetBuffer_gv(OTA_Packet_st.rxPayloadData_au8,
                       (U8)sizeof(OTA_Packet_st.rxPayloadData_au8));

    /* Restart UART read */
    DRV_UART_ReadNonBlock_gen(OTA_UART_INSTANCE, &OTA_UartRxByte_u8, 1U);
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : OTA_Reset_gv
 * ----------------------------------------------------------------------------
 * Description   : Full reset of OTA module (resets all state variables)
 * Parameters    : void
 * Return Value  : void
 * --------------------------------------------------------------------------*/
static void OTA_Reset_gv(void)
{
    /* Reset all counters and indices */
    OTA_Index_u8           = 1U;
    OTA_RxIndex_u8         = 0U;
    OTA_DataCount_u8       = 0U;
    OTA_PreviousCommand_u8 = 0U;
    OTA_TotalFrameLen_u16  = OTA_FRAME_OVERHEAD;
    OTA_StatusIndex_u8     = 0U;
    OTA_RxInitiated_b      = false;

    /* Reset error counters */
    OTA_UartTxErrorCount_u8 = 0U;
    OTA_UartRxErrorCount_u8 = 0U;
    OTA_UartTxInit_b        = false;
    OTA_UartRxInit_b        = true;
    OTA_UartRxByte_u8       = 0U;

    /* Reset state machines */
    OTA_Ack_en     = OTA_ACK_IDLE_E;
    OTA_RxState_en = OTA_RX_STATE_START_OF_FRAME_E;
    OTA_Command_en = OTA_CMD_OTA_UPDATE_E;

    /* Reset packet structure */
    OTA_Packet_st.receiveFlag_b       = false;
    OTA_Packet_st.transmitFlag_b      = false;
    OTA_Packet_st.rxCommand_u8        = 0U;
    OTA_Packet_st.rxPayloadLength_u16 = 0U;
    OTA_Packet_st.txPayloadLength_u8  = 0U;

    /* Clear all buffers */
    OTA_ResetBuffer_gv(OTA_Packet_st.txBuffer_au8,
                       (U8)sizeof(OTA_Packet_st.txBuffer_au8));
    OTA_ResetBuffer_gv(OTA_Packet_st.rxBuffer_au8,
                       (U8)sizeof(OTA_Packet_st.rxBuffer_au8));
    OTA_ResetBuffer_gv(OTA_Packet_st.rxPayloadData_au8,
                       (U8)sizeof(OTA_Packet_st.rxPayloadData_au8));

    /* Restart UART read */
    DRV_UART_ReadNonBlock_gen(OTA_UART_INSTANCE, &OTA_UartRxByte_u8, 1U);
}

/* ==================== UART CALLBACK FUNCTIONS ==================== */

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : uart_callback1_gv
 * ----------------------------------------------------------------------------
 * Description   : UART callback for OTA UART instance (LPUART1)
 * Parameters    : driverState_argp - Driver state pointer
 *                 event_argin - UART event type
 *                 userData_argp - User data pointer
 * Return Value  : void
 * Notes         : Handles RX_FULL and ERROR events
 * --------------------------------------------------------------------------*/
void uart_callback1_gv(void *driverState_argp, uart_event_t event_argin,
                        void *userData_argp)
{
    (void)driverState_argp;
    (void)userData_argp;

    if (event_argin == UART_EVENT_RX_FULL)
    {
        /* Process received byte */
    	uart_flag_b=true;
        OTA_ProcessRxData_gv(OTA_UartRxByte_u8);

        /* Set up next receive */
        DRV_UART_SetRxBuffer_gen(OTA_UART_INSTANCE, &OTA_UartRxByte_u8, 1U);
        DRV_UART_ReadNonBlock_gen(OTA_UART_INSTANCE, &OTA_UartRxByte_u8, 1U);
    }
    if (event_argin == UART_EVENT_ERROR)
    {
        /* Re-initialize on error */
        DRV_UART_SetRxBuffer_gen(OTA_UART_INSTANCE, &OTA_UartRxByte_u8, 1U);
        DRV_UART_ReadNonBlock_gen(OTA_UART_INSTANCE, &OTA_UartRxByte_u8, 1U);
    }
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : uart_callback0_gv
 * ----------------------------------------------------------------------------
 * Description   : UART callback for other UART instance (unused)
 * Parameters    : driverState_argp - Driver state pointer
 *                 event_argin - UART event type
 *                 userData_argp - User data pointer
 * Return Value  : void
 * --------------------------------------------------------------------------*/
void uart_callback0_gv(void *driverState_argp, uart_event_t event_argin,
                        void *userData_argp)
{
    (void)driverState_argp;
    (void)event_argin;
    (void)userData_argp;
}
