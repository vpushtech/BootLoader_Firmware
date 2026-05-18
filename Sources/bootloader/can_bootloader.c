/*
 * can_bootloader.c
 *
 *  Created on: 24-Apr-2026
 *  Author: PandurangaKarapothul
 *  Description: CAN Bootloader Implementation - Firmware Update via CAN Bus
 *  Version: 1.0
 *  Modification History:
 *  Date            Author              Description
 *  ----------------------------------------------------------------------------
 *  24-Apr-2026     PandurangaKarapothul Initial CAN Bootloader Implementation
 *  24-Apr-2026     PandurangaKarapothul Added CRC32 verification
 *  24-Apr-2026     PandurangaKarapothul Added Flash write with validation
 ******************************************************************************/

#include <bootloader/can_bootloader.h>
#include <nvm.h>
#include <bootloader/ota_bootloader.h>

extern BIN uart_flag_b;

static DRV_CanStatus_ten BSP_CANFlashWrite_men(void);
static U32               BSP_CRCFlash_mu32(void);
static U32               BSP_CalculateCRC_mu32(const U8 * data_argptr8, U32 length_arg32);

/* ==================== PUBLIC FUNCTIONS ==================== */
/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_ProcessCanComm_gv
 *  ----------------------------------------------------------------------------
 *  Description   : Main CAN bootloader processing loop. Handles firmware update
 *                  commands received via CAN bus. Performs flash write, CRC
 *                  verification, and application validation.
 *  Parameters    : void
 *  Return Value  : void (never returns - triggers reset on completion/error)
 *  Notes         : This function runs in an infinite loop waiting for CAN commands.
 *                  Upon successful firmware update, system resets to run new application.
 *  --------------------------------------------------------------------------*/
void BSP_ProcessCanComm_gv(void)
{
    U32 rxCRC              = 0U;
    U8  rxbuf[8]           = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
    U32 resetHandlerAddress;
    DRV_CanStatus_ten flashStatus;

    while (1 == 1)  // Infinite loop for continuous CAN command processing
    {
        (void)CAN_ProcessReceiveFrame_mv((U8 *)rxbuf);  // Receive CAN frame

        if (uart_flag_b != (BIN)0)  // Check UART flag for boot status update
        {
            DRV_WDG_Refresh_gen(DRV_WDG_INSTANCE_1);  // Refresh watchdog timer
            NVM_FlshStoredData_st.APP_bootFlag_u16  = 0x5650U;
            NVM_FlshStoredData_st.APP_appStatus_u16 = 0xAAAAU;
            NVM_BootFlagAppStatus_Update(
                NVM_FlshStoredData_st.APP_bootFlag_u16,
                NVM_FlshStoredData_st.APP_appStatus_u16);
        }

        if (rxbuf[0] == FLASH_WRITE_CMD)  // Check if command is flash write
        {
            DRV_FLASH_EraseSector_gen((U32)APP_START_ADDRESS, (U32)APP_SIZE);  // Erase application sector

            NVM_FlshStoredData_st.APP_appStatus_u16 = 0xAAAAU;
            NVM_FlshStoredData_st.APP_bootFlag_u16  = 0x5651U;
            NVM_BootFlagAppStatus_Update(
                NVM_FlshStoredData_st.APP_bootFlag_u16,
                NVM_FlshStoredData_st.APP_appStatus_u16);

            flashStatus = BSP_CANFlashWrite_men();  // Write firmware via CAN

            if (flashStatus != DRV_CAN_STATUS_SUCCESS)  // Handle flash write error
            {
                CAN_ProcessTransmitFrame_mv(CAN_FLASH_WRITING_ERROR);
                NVM_FlshStoredData_st.APP_appStatus_u16 = 0xAAAAU;
                NVM_FlshStoredData_st.APP_bootFlag_u16  = 0x5651U;
                NVM_BootFlagAppStatus_Update(
                    NVM_FlshStoredData_st.APP_bootFlag_u16,
                    NVM_FlshStoredData_st.APP_appStatus_u16);
                SystemSoftwareReset();  // Reset on error
            }
            else
            {
                /* Flash write succeeded; continue to CRC verification */
            }

            (void)CAN_ProcessReceiveFrame_mv((U8 *)rxbuf);  // Receive CRC value

            rxCRC = (U32)rxbuf[0]
                  | ((U32)rxbuf[1] << 8U)
                  | ((U32)rxbuf[2] << 16U)
                  | ((U32)rxbuf[3] << 24U);

            if (BSP_CRCFlash_mu32() == rxCRC)  // Verify CRC checksum
            {
                resetHandlerAddress =
                    *(volatile const U32 *)(APP_START_ADDRESS + 4U);

                if ((resetHandlerAddress < (U32)APP_START_ADDRESS) ||
                    (resetHandlerAddress > (U32)APP_END_ADDRESS))  // Validate reset handler address
                {
                    CAN_ProcessTransmitFrame_mv(CAN_WRONG_APP_ADDRESS);
                }
                else
                {
                    DRV_WDG_Refresh_gen(DRV_WDG_INSTANCE_1);  // Refresh watchdog
                    CAN_ProcessTransmitFrame_mv(CAN_BIN_FLASHED);  // Send success response

                    NVM_FlshStoredData_st.APP_appStatus_u16 = 0x5651U;
                    NVM_FlshStoredData_st.APP_bootFlag_u16  = 0xAAAAU;
                    NVM_BootFlagAppStatus_Update(
                        NVM_FlshStoredData_st.APP_bootFlag_u16,
                        NVM_FlshStoredData_st.APP_appStatus_u16);

                    DRV_Timer_Delay_gst(
                        DRV_TIMER0,
                        LPTI_TIMER0_CH0,
                        DRV_DELAY_UNITS_MILLISECOND,
                        2000U);  // Delay before reset

                    SystemSoftwareReset();  // Reset to run new application
                }
            }
            else if (FlashWriteFlag_garg8 != (U8)0)  // CRC mismatch with flash write flag
            {
                CAN_ProcessTransmitFrame_mv(CAN_FLASH_WRITING_ERROR);
            }
            else
            {
                /* CRC mismatch with no flash write flag; no action required */
            }
        }
        else
        {
            /* Command not recognised; continue polling */
        }
    }
}

/* ==================== PRIVATE FUNCTIONS ==================== */
/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_CANFlashWrite_men
 *  ----------------------------------------------------------------------------
 *  Description   : Receives firmware data through CAN and writes it to flash memory.
 *                  First receives total file size, then writes 8-byte chunks
 *                  sequentially to flash starting at APP_START_ADDRESS.
 *  Parameters    : void
 *  Return Value  : DRV_CanStatus_En - DRV_CAN_STATUS_SUCCESS on success,
 *                                     DRV_CAN_STATUS_ERROR on failure
 *  Notes         : Uses 8-byte CAN frames for firmware data transfer.
 *                  Sets FlashWriteFlag_garg8 when any flash write occurs.
 *  --------------------------------------------------------------------------*/
static DRV_CanStatus_ten BSP_CANFlashWrite_men(void)
{
    U32               fileSize            = 0U;
    U8                rxBuffer_garg8[8]   = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
    U32               flashAddress        = (U32)APP_START_ADDRESS;
    const U32         chunkSize           = 8U;
    DRV_CanStatus_ten receiveStatus;

    BytesWritten_garg32 = 0U;

    CAN_ProcessTransmitFrame_mv(CAN_RECEIVE_BIN_CMD);  // Request binary receive

    (void)CAN_ProcessReceiveFrame_mv((U8 *)rxBuffer_garg8);  // Receive file size

    fileSize = (U32)rxBuffer_garg8[0]
             | ((U32)rxBuffer_garg8[1] << 8U)
             | ((U32)rxBuffer_garg8[2] << 16U)
             | ((U32)rxBuffer_garg8[3] << 24U);

    while (BytesWritten_garg32 < fileSize)  // Loop until all bytes received
    {
        receiveStatus = CAN_ProcessReceiveFrame_mv((U8 *)rxBuffer_garg8);  // Receive data chunk

        if (receiveStatus != DRV_CAN_STATUS_SUCCESS)
        {
            return DRV_CAN_STATUS_ERROR;  // Return error on receive failure
        }

        DRV_FLASH_WriteBlock_gen(flashAddress, rxBuffer_garg8, chunkSize);  // Write to flash

        flashAddress        += chunkSize;
        BytesWritten_garg32 += chunkSize;
        FlashWriteFlag_garg8 = 0x01U;

        DRV_WDG_Refresh_gen(DRV_WDG_INSTANCE_1);  // Refresh watchdog during write
    }

    return DRV_CAN_STATUS_SUCCESS;
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_CRCFlash_mu32
 *  ----------------------------------------------------------------------------
 *  Description   : Calculates CRC32 checksum for the flashed firmware data.
 *                  Iterates through all bytes written to flash and computes
 *                  their CRC value using the standard CRC-32 algorithm.
 *  Parameters    : void
 *  Return Value  : U32 - Calculated CRC32 value of the flashed firmware
 *  Notes         : Uses BytesWritten_garg32 to know how many bytes to checksum.
 *  --------------------------------------------------------------------------*/
static U32 BSP_CRCFlash_mu32(void)
{
    const U8 * const ptr = (const U8 *)APP_START_ADDRESS;
    return BSP_CalculateCRC_mu32(ptr, BytesWritten_garg32);  // Calculate CRC over flashed data
}

/* ----------------------------------------------------------------------------
 *  FUNCTION NAME : BSP_CalculateCRC_mu32
 *  ----------------------------------------------------------------------------
 *  Description   : Computes CRC32 checksum for the given data buffer using
 *                  polynomial 0x04C11DB7. Implements standard CRC-32 algorithm
 *                  with initial value 0xFFFFFFFF and final XOR 0xFFFFFFFF.
 *  Parameters    : data_argptr8 - Pointer to the data buffer
 *                  length_arg32 - Length of data in bytes
 *  Return Value  : U32 - Calculated CRC32 value
 *  --------------------------------------------------------------------------*/
static U32 BSP_CalculateCRC_mu32(const U8 * data_argptr8, U32 length_arg32)
{
    U32       crc  = 0xFFFFFFFFU;
    const U32 poly = 0x04C11DB7U;
    U32       i;
    U8        bit;

    for (i = 0U; i < length_arg32; i++)  // Process each byte
    {
        crc ^= ((U32)data_argptr8[i]) << 24U;  // XOR byte with MSB of CRC

        for (bit = 0U; bit < 8U; bit++)  // Process each bit
        {
            if ((crc & 0x80000000U) != 0U)  // Check MSB
            {
                crc = (crc << 1U) ^ poly;  // Shift left and XOR with polynomial
            }
            else
            {
                crc <<= 1U;  // Just shift left
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;  // Final XOR with complement
}
