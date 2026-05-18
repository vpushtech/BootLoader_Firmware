/*******************************************************************************
 *  Description     : UART Driver
 *  Author          : Rushikesh
 *  Created On      : 08-Jul-2025
 *  Version         : 2.0
 *  Modification History:
 *  Date        Author      Description
 *  ----------------------------------------------------------------------------
 *  18-Jul-2025 RUSHIKESH   UART Driver Architecture Implementation
 *  31-Jul-2025 RUSHIKESH   UART Driver Testing completed
 *  11-Aug-2025 RUSHIKESH   Naming Architecture Implementation
 *  26-Jan-2026 RUSHIKESH   Added validation, init tracking, error handling
 *  01-May-2026 RUSHIKESH   MISRA C 2012 Compliance
 */

/* ==================== INCLUDE FILES ==================== */
#include "drv_uart.h"
#include "bsp_config.h"

/* ==================== STATIC (MODULE-PRIVATE) CONSTANTS ==================== */
static const U8 uartInstances_arrst[DRV_MAX_UART_INSTANCE] =
{
    INST_LPUART1,
    INST_LPUART1
};
static const lpuart_user_config_t * const uartConfig_arrst[DRV_MAX_UART_INSTANCE] =
{
    &lpuart1_InitConfig0,
    &lpuart1_InitConfig0
};

/* ==================== STATE VARIABLES ==================== */
static lpuart_state_t * const DRV_uartState_mst[DRV_MAX_UART_INSTANCE] =
{
    &lpuart1_State,
    &lpuart1_State
};

/* ==================== GLOBAL VARIABLES ==================== */
volatile BIN DRV_uartInitStatus_mb[DRV_MAX_UART_INSTANCE] = {(BIN)0, (BIN)0};

/* ==================== CALLBACK ARRAY ==================== */
static void (*uartCallback_arrst[DRV_MAX_UART_INSTANCE])(void *, uart_event_t, void *) =
{
    uart_callback0_gv,
    uart_callback1_gv
};

/* ==================== PRIVATE HELPER FUNCTION PROTOTYPES ==================== */
static DRV_uartStatus_ten DRV_UART_ValidateInstance_prv(U8 uartPinIdx_argu8);

static DRV_uartStatus_ten DRV_UART_ValidateTransferParams_prv(
    U8        uartPinIdx_argu8,
    const U8 *data_argptru8,
    U32       size_argu32);

static DRV_uartStatus_ten DRV_UART_CheckIRQEnabled_prv(U8 uartPinIdx_argu8);

static DRV_uartStatus_ten DRV_UART_MapHalStatus_prv(status_t halStatus_arg);

/* ==================== PRIVATE HELPER FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_UART_ValidateInstance_prv
 *  Description   : Validates that a UART instance index is within range and
 *                  that the instance has been initialized.
 *  Parameters    : uartPinIdx_argu8 - UART instance index to validate
 *  Return Value  : DRV_UART_SUCCESS          - Valid and initialized
 *                  DRV_UART_INVALID_INSTANCE  - Index out of range
 *                  DRV_UART_NOT_INITIALIZED   - Instance not initialized
 * --------------------------------------------------------------------------- */
static DRV_uartStatus_ten DRV_UART_ValidateInstance_prv(U8 uartPinIdx_argu8)
{
    DRV_uartStatus_ten retStatus;
    if (uartPinIdx_argu8 >= (U8)DRV_MAX_UART_INSTANCE)
    {
        retStatus = DRV_UART_INVALID_INSTANCE;
    }
    else if (DRV_uartInitStatus_mb[uartPinIdx_argu8] == (BIN)0)
    {
        retStatus = DRV_UART_NOT_INITIALIZED;
    }
    else
    {
        retStatus = DRV_UART_SUCCESS;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_UART_ValidateTransferParams_prv
 *  Description   : Validates the data pointer and transfer size for any
 *                  read or write operation.
 *  Parameters    : uartPinIdx_argu8 - UART instance index (already range-checked)
 *                  data_argptru8    - Data buffer pointer to validate
 *                  size_argu32      - Transfer size to validate
 *  Return Value  : DRV_UART_SUCCESS           - Parameters valid
 *                  DRV_UART_NULL_POINTER       - data_argptru8 is NULL
 *                  DRV_UART_INVALID_DATA_SIZE  - Size is 0 or exceeds maximum
 * --------------------------------------------------------------------------- */
static DRV_uartStatus_ten DRV_UART_ValidateTransferParams_prv(
    U8        uartPinIdx_argu8,
    const U8 *data_argptru8,
    U32       size_argu32)
{
    DRV_uartStatus_ten retStatus;
    (void)uartPinIdx_argu8;

    if (data_argptru8 == NULL)
    {
        retStatus = DRV_UART_NULL_POINTER;
    }
    else if ((size_argu32 == 0U) || (size_argu32 > DRV_UART_MAX_DATA_SIZE))
    {
        retStatus = DRV_UART_INVALID_DATA_SIZE;
    }
    else
    {
        retStatus = DRV_UART_SUCCESS;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_UART_CheckIRQEnabled_prv
 *  Description   : Checks whether the NVIC IRQ line associated with the
 *                  given UART instance is currently enabled.
 *  Parameters    : uartPinIdx_argu8 - UART instance index (already range-checked)
 *  Return Value  : DRV_UART_SUCCESS               - IRQ is enabled
 *                  DRV_UART_INTERRUPT_NOT_ENABLED  - IRQ is not enabled
 *                  DRV_UART_INVALID_INSTANCE       - Unknown instance index
 * --------------------------------------------------------------------------- */
static DRV_uartStatus_ten DRV_UART_CheckIRQEnabled_prv(U8 uartPinIdx_argu8)
{
    DRV_uartStatus_ten retStatus;
    BIN                irqEnabled_b;
    if (uartPinIdx_argu8 == (U8)DRV_UART_INSTANCE_0)
    {
        irqEnabled_b = DRV_NVIC_IsIRQEnabled_gb(NVIC_LPUART0_IRQ);
        if (irqEnabled_b != (BIN)1)
        {
            retStatus = DRV_UART_INTERRUPT_NOT_ENABLED;
        }
        else
        {
            retStatus = DRV_UART_SUCCESS;
        }
    }
    else if (uartPinIdx_argu8 == (U8)DRV_UART_INSTANCE_1)
    {
        irqEnabled_b = DRV_NVIC_IsIRQEnabled_gb(NVIC_LPUART1_IRQ);

        if (irqEnabled_b != (BIN)1)
        {
            retStatus = DRV_UART_INTERRUPT_NOT_ENABLED;
        }
        else
        {
            retStatus = DRV_UART_SUCCESS;
        }
    }
    else
    {
        retStatus = DRV_UART_INVALID_INSTANCE;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_UART_MapHalStatus_prv
 *  Description   : Maps an LPUART HAL status_t return code to the driver's
 *                  DRV_uartStatus_ten enumeration.
 *  Parameters    : halStatus_arg - HAL return code
 *  Return Value  : DRV_UART_SUCCESS  - STATUS_SUCCESS
 *                  DRV_UART_BUSY     - STATUS_BUSY
 *                  DRV_UART_TIMEOUT  - STATUS_TIMEOUT
 *                  DRV_UART_FAILED   - Any other status
 * --------------------------------------------------------------------------- */
static DRV_uartStatus_ten DRV_UART_MapHalStatus_prv(status_t halStatus_arg)
{
    DRV_uartStatus_ten retStatus;
    if (halStatus_arg == STATUS_SUCCESS)
    {
        retStatus = DRV_UART_SUCCESS;
    }
    else if (halStatus_arg == STATUS_BUSY)
    {
        retStatus = DRV_UART_BUSY;
    }
    else if (halStatus_arg == STATUS_TIMEOUT)
    {
        retStatus = DRV_UART_TIMEOUT;
    }
    else
    {
        retStatus = DRV_UART_FAILED;
    }

    return retStatus;
}

/* ==================== PUBLIC FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_UART_Init_gen
 *  Description   : Initializes the LPUART peripheral for the specified instance
 *                  and installs the receive callback.
 *  Parameters    : uartPinIdx_argu8 - UART instance index
 *  Return Value  : DRV_UART_SUCCESS            - Initialization succeeded
 *                  DRV_UART_INVALID_INSTANCE    - Index out of valid range
 *                  DRV_UART_ALREADY_INITIALIZED - Instance already initialized
 *                  DRV_UART_FAILED              - Underlying HAL call failed
 * --------------------------------------------------------------------------- */
DRV_uartStatus_ten DRV_UART_Init_gen(U8 uartPinIdx_argu8)
{
    DRV_uartStatus_ten retStatus;
    status_t           halStatus;
    if (uartPinIdx_argu8 >= (U8)DRV_MAX_UART_INSTANCE)
    {
        retStatus = DRV_UART_INVALID_INSTANCE;
    }

    else if (DRV_uartInitStatus_mb[uartPinIdx_argu8] != (BIN)0)
    {
        retStatus = DRV_UART_ALREADY_INITIALIZED;
    }
    else
    {
        halStatus = LPUART_DRV_Init(
            uartInstances_arrst[uartPinIdx_argu8],
            DRV_uartState_mst[uartPinIdx_argu8],
            uartConfig_arrst[uartPinIdx_argu8]);

        if (halStatus == STATUS_SUCCESS)
        {
            LPUART_DRV_InstallRxCallback(
                uartInstances_arrst[uartPinIdx_argu8],
                uartCallback_arrst[uartPinIdx_argu8],
                NULL);

            DRV_uartInitStatus_mb[uartPinIdx_argu8] = (BIN)1;
            retStatus = DRV_UART_SUCCESS;
        }
        else
        {
            retStatus = DRV_UART_FAILED;
        }
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_UART_DeInit_gen
 *  Description   : Deinitializes the LPUART peripheral for the specified instance.
 *  Parameters    : uartPinIdx_argu8 - UART instance index
 *  Return Value  : DRV_UART_SUCCESS          - Deinitialization succeeded
 *                  DRV_UART_INVALID_INSTANCE  - Index out of valid range
 *                  DRV_UART_NOT_INITIALIZED   - Instance was not initialized
 *                  DRV_UART_FAILED            - Underlying HAL call failed
 * --------------------------------------------------------------------------- */
DRV_uartStatus_ten DRV_UART_DeInit_gen(U8 uartPinIdx_argu8)
{
    DRV_uartStatus_ten retStatus;
    status_t           halStatus;
    retStatus = DRV_UART_ValidateInstance_prv(uartPinIdx_argu8);

    if (retStatus == DRV_UART_SUCCESS)
    {
        halStatus = LPUART_DRV_Deinit(uartInstances_arrst[uartPinIdx_argu8]);

        if (halStatus == STATUS_SUCCESS)
        {
            DRV_uartInitStatus_mb[uartPinIdx_argu8] = (BIN)0;
            retStatus = DRV_UART_SUCCESS;
        }
        else
        {
            retStatus = DRV_UART_FAILED;
        }
    }
    else
    {
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_UART_WriteBlock_gen
 *  Description   : Transmits data over UART in blocking mode. Blocks until
 *                  the transfer completes or a timeout occurs.
 *  Parameters    : uartPinIdx_argu8 - UART instance index
 *                  data_argptru8    - Pointer to transmit data buffer
 *                  size_argu32      - Number of bytes to transmit
 *  Return Value  : DRV_UART_SUCCESS               - Transmission completed
 *                  DRV_UART_INVALID_INSTANCE        - Index out of range
 *                  DRV_UART_NOT_INITIALIZED         - Instance not initialized
 *                  DRV_UART_NULL_POINTER            - data_argptru8 is NULL
 *                  DRV_UART_INVALID_DATA_SIZE        - size_argu32 invalid
 *                  DRV_UART_INTERRUPT_NOT_ENABLED   - IRQ line not enabled
 *                  DRV_UART_BUSY                    - Peripheral busy
 *                  DRV_UART_TIMEOUT                 - Transfer timed out
 *                  DRV_UART_FAILED                  - Other HAL failure
 * --------------------------------------------------------------------------- */
DRV_uartStatus_ten DRV_UART_WriteBlock_gen(
    U8        uartPinIdx_argu8,
    const U8 *data_argptru8,
    U32       size_argu32)
{
    DRV_uartStatus_ten retStatus;
    status_t           halStatus;
    retStatus = DRV_UART_ValidateInstance_prv(uartPinIdx_argu8);

    if (retStatus == DRV_UART_SUCCESS)
    {
        retStatus = DRV_UART_ValidateTransferParams_prv(
            uartPinIdx_argu8, data_argptru8, size_argu32);
    }
    else
    {
    }

    if (retStatus == DRV_UART_SUCCESS)
    {
        retStatus = DRV_UART_CheckIRQEnabled_prv(uartPinIdx_argu8);
    }
    else
    {

    }

    if (retStatus == DRV_UART_SUCCESS)
    {
        halStatus = LPUART_DRV_SendDataBlocking(
            uartInstances_arrst[uartPinIdx_argu8],
            data_argptru8,
            size_argu32,
            DRV_UART_TIMEOUT_MS);

        retStatus = DRV_UART_MapHalStatus_prv(halStatus);
    }
    else
    {

    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_UART_ReadBlock_gen
 *  Description   : Receives data over UART in blocking mode. Blocks until
 *                  the transfer completes or a timeout occurs.
 *  Parameters    : uartPinIdx_argu8 - UART instance index
 *                  data_argptru8    - Pointer to receive data buffer
 *                  size_argu32      - Number of bytes to receive
 *  Return Value  : DRV_UART_SUCCESS               - Reception completed
 *                  DRV_UART_INVALID_INSTANCE        - Index out of range
 *                  DRV_UART_NOT_INITIALIZED         - Instance not initialized
 *                  DRV_UART_NULL_POINTER            - data_argptru8 is NULL
 *                  DRV_UART_INVALID_DATA_SIZE        - size_argu32 invalid
 *                  DRV_UART_INTERRUPT_NOT_ENABLED   - IRQ line not enabled
 *                  DRV_UART_BUSY                    - Peripheral busy
 *                  DRV_UART_TIMEOUT                 - Transfer timed out
 *                  DRV_UART_FAILED                  - Other HAL failure
 * --------------------------------------------------------------------------- */
DRV_uartStatus_ten DRV_UART_ReadBlock_gen(
    U8  uartPinIdx_argu8,
    U8 *data_argptru8,
    U32 size_argu32)
{
    DRV_uartStatus_ten retStatus;
    status_t           halStatus;
    retStatus = DRV_UART_ValidateInstance_prv(uartPinIdx_argu8);

    if (retStatus == DRV_UART_SUCCESS)
    {
        retStatus = DRV_UART_ValidateTransferParams_prv(
            uartPinIdx_argu8, (const U8 *)data_argptru8, size_argu32);
    }
    else
    {
    }

    if (retStatus == DRV_UART_SUCCESS)
    {
        retStatus = DRV_UART_CheckIRQEnabled_prv(uartPinIdx_argu8);
    }
    else
    {
    }

    if (retStatus == DRV_UART_SUCCESS)
    {
        halStatus = LPUART_DRV_ReceiveDataBlocking(
            uartInstances_arrst[uartPinIdx_argu8],
            data_argptru8,
            size_argu32,
            DRV_UART_TIMEOUT_MS);

        retStatus = DRV_UART_MapHalStatus_prv(halStatus);
    }
    else
    {
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_UART_ReadNonBlock_gen
 *  Description   : Initiates an asynchronous UART receive operation and returns
 *                  immediately. Completion is signalled via the installed callback.
 *  Parameters    : uartPinIdx_argu8 - UART instance index
 *                  data_argptru8    - Pointer to receive data buffer
 *                  size_argu32      - Number of bytes to receive
 *  Return Value  : DRV_UART_SUCCESS               - Transfer initiated
 *                  DRV_UART_INVALID_INSTANCE        - Index out of range
 *                  DRV_UART_NOT_INITIALIZED         - Instance not initialized
 *                  DRV_UART_NULL_POINTER            - data_argptru8 is NULL
 *                  DRV_UART_INVALID_DATA_SIZE        - size_argu32 invalid
 *                  DRV_UART_INTERRUPT_NOT_ENABLED   - IRQ line not enabled
 *                  DRV_UART_BUSY                    - Peripheral busy
 *                  DRV_UART_FAILED                  - Other HAL failure
 * --------------------------------------------------------------------------- */
DRV_uartStatus_ten DRV_UART_ReadNonBlock_gen(
    U8  uartPinIdx_argu8,
    U8 *data_argptru8,
    U32 size_argu32)
{
    DRV_uartStatus_ten retStatus;
    status_t           halStatus;

    retStatus = DRV_UART_ValidateInstance_prv(uartPinIdx_argu8);

    if (retStatus == DRV_UART_SUCCESS)
    {
        retStatus = DRV_UART_ValidateTransferParams_prv(
            uartPinIdx_argu8, (const U8 *)data_argptru8, size_argu32);
    }
    else
    {
    }

    if (retStatus == DRV_UART_SUCCESS)
    {
        retStatus = DRV_UART_CheckIRQEnabled_prv(uartPinIdx_argu8);
    }
    else
    {
    }

    if (retStatus == DRV_UART_SUCCESS)
    {
        halStatus = LPUART_DRV_ReceiveData(
            uartInstances_arrst[uartPinIdx_argu8],
            data_argptru8,
            size_argu32);

        retStatus = DRV_UART_MapHalStatus_prv(halStatus);
    }
    else
    {

    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_UART_SetRxBuffer_gen
 *  Description   : Sets the receive buffer for interrupt-driven (callback-based)
 *                  reception. The buffer is consumed as bytes arrive via IRQ.
 *  Parameters    : uartPinIdx_argu8 - UART instance index
 *                  data_argptru8    - Pointer to receive buffer
 *                  size_argu32      - Buffer size in bytes
 *  Return Value  : DRV_UART_SUCCESS               - Buffer configured
 *                  DRV_UART_INVALID_INSTANCE        - Index out of range
 *                  DRV_UART_NOT_INITIALIZED         - Instance not initialized
 *                  DRV_UART_NULL_POINTER            - data_argptru8 is NULL
 *                  DRV_UART_INVALID_DATA_SIZE        - size_argu32 invalid
 *                  DRV_UART_INTERRUPT_NOT_ENABLED   - IRQ line not enabled
 * --------------------------------------------------------------------------- */
DRV_uartStatus_ten DRV_UART_SetRxBuffer_gen(
    U8  uartPinIdx_argu8,
    U8 *data_argptru8,
    U32 size_argu32)
{
    DRV_uartStatus_ten retStatus;
    retStatus = DRV_UART_ValidateInstance_prv(uartPinIdx_argu8);

    if (retStatus == DRV_UART_SUCCESS)
    {
        retStatus = DRV_UART_ValidateTransferParams_prv(
            uartPinIdx_argu8, (const U8 *)data_argptru8, size_argu32);
    }
    else
    {
    }

    if (retStatus == DRV_UART_SUCCESS)
    {
        retStatus = DRV_UART_CheckIRQEnabled_prv(uartPinIdx_argu8);
    }
    else
    {
    }

    if (retStatus == DRV_UART_SUCCESS)
    {

        LPUART_DRV_SetRxBuffer(
            uartInstances_arrst[uartPinIdx_argu8],
            data_argptru8,
            size_argu32);

        retStatus = DRV_UART_SUCCESS;
    }
    else
    {

    }

    return retStatus;
}

/* ======================================== END ======================================== */
