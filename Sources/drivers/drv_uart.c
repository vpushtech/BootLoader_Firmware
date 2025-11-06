/* ==================== INCLUDE FILES ==================== */
#include "drv_uart.h"
#include "lpuart_driver.h"
#include "lpuart1.h"
/* ==================== DEFINES ==================== */
#define TIMEOUT_MS             1000U

/* ==================== STATIC VARIABLES ==================== */
static const uint32_t uartInstances_arrst[DRV_UART_MAX_INSTANCE] = {INST_LPUART1};
static lpuart_state_t uartConfig_arrst[DRV_UART_MAX_INSTANCE];

/* ==================== STATIC FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_UART_Init
*   Description   : Initializes UART controller for specified instance
*   Parameters    : uart_instance - UART instance to initialize
*   Return Value  : DRV_Uart_Status - Status of initialization
*                   (DRV_UART_STATUS_SUCCESS/DRV_UART_STATUS_ERROR)
*  --------------------------------------------------------------------------- */
DRV_Uart_Status_ten DRV_UART_Init(DRV_UART_Instance_ten uartPinIdx_argu8)
{
    status_t status;
    if (uartPinIdx_argu8 >= DRV_UART_MAX_INSTANCE)
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_Init(uartInstances_arrst[uartPinIdx_argu8],&uartConfig_arrst[uartPinIdx_argu8],&lpuart1_InitConfig0);
    if( STATUS_SUCCESS == status)
    {
    	return DRV_UART_STATUS_SUCCESS;
    }
    return DRV_UART_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_UART_SendDataBlocking
*   Description   : Transmits data via UART in blocking mode with timeout
*   Parameters    : uart_instance - UART instance to use
*                   sendData - Pointer to data buffer to transmit
*                   length - Number of bytes to transmit
*   Return Value  : DRV_Uart_Status - Status of transmission
*                   (SUCCESS/ERROR/BUSY/TIMEOUT)
*  --------------------------------------------------------------------------- */
DRV_Uart_Status_ten DRV_UART_SendDataBlocking(DRV_UART_Instance_ten uartPinIdx_argu8,const uint8_t *data_argptru8,uint32_t size_argu32)
{
    status_t status;
    if ((uartPinIdx_argu8 >= DRV_UART_MAX_INSTANCE) || (data_argptru8 == NULL) || (size_argu32 == 0U))
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_SendDataBlocking(uartInstances_arrst[uartPinIdx_argu8],data_argptru8,size_argu32,TIMEOUT_MS);
    if (status == STATUS_SUCCESS)
    {
        return DRV_UART_STATUS_SUCCESS;
    }
    else if (status == STATUS_BUSY)
    {
        return DRV_UART_STATUS_BUSY;
    }
    else if (status == STATUS_TIMEOUT)
    {
        return DRV_UART_STATUS_TIMEOUT;
    }
    return DRV_UART_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_UART_SendData
*   Description   : Transmits data via UART in non-blocking mode
*   Parameters    : uart_instance - UART instance to use
*                   sendData - Pointer to data buffer to transmit
*                   length - Number of bytes to transmit
*   Return Value  : DRV_Uart_Status - Status of transmission
*                   (SUCCESS/ERROR/BUSY)
*  --------------------------------------------------------------------------- */
DRV_Uart_Status_ten DRV_UART_SendData(DRV_UART_Instance_ten uartPinIdx_argu8,const uint8_t *data_argptru8,uint32_t size_argu32)
{
    status_t status;
    if ((uartPinIdx_argu8 >= DRV_UART_MAX_INSTANCE) || (data_argptru8 == NULL) || (size_argu32 == 0U))
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_SendData(uartInstances_arrst[uartPinIdx_argu8],data_argptru8,size_argu32);
    if (status == STATUS_SUCCESS)
    {
        return DRV_UART_STATUS_SUCCESS;
    }
    else if (status == STATUS_BUSY)
    {
        return DRV_UART_STATUS_BUSY;
    }

    return DRV_UART_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_UART_ReceiveDataBlocking
*   Description   : Receives data via UART in blocking mode with timeout
*   Parameters    : uart_instance - UART instance to use
*                   recdata - Pointer to buffer for received data
*                   length - Number of bytes to receive
*   Return Value  : DRV_Uart_Status - Status of reception
*                   (SUCCESS/ERROR/BUSY/TIMEOUT)
*  --------------------------------------------------------------------------- */
DRV_Uart_Status_ten DRV_UART_ReceiveDataBlocking(DRV_UART_Instance_ten uartPinIdx_argu8,uint8_t *data_argptru8,uint32_t size_argu32)
{
    status_t status;
    if ((uartPinIdx_argu8 >= DRV_UART_MAX_INSTANCE) || (data_argptru8 == NULL) || (size_argu32 == 0U))
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_ReceiveDataBlocking(uartInstances_arrst[uartPinIdx_argu8],data_argptru8,size_argu32,TIMEOUT_MS);
    if (status == STATUS_SUCCESS)
    {
        return DRV_UART_STATUS_SUCCESS;
    }
    else if (status == STATUS_BUSY)
    {
        return DRV_UART_STATUS_BUSY;
    }
    else if (status == STATUS_TIMEOUT)
    {
        return DRV_UART_STATUS_TIMEOUT;
    }

    return DRV_UART_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_UART_ReceiveData
*   Description   : Receives data via UART in non-blocking mode
*   Parameters    : uart_instance - UART instance to use
*                   recdata - Pointer to buffer for received data
*                   length - Number of bytes to receive
*   Return Value  : DRV_Uart_Status - Status of reception
*                   (SUCCESS/ERROR/BUSY)
*  --------------------------------------------------------------------------- */
DRV_Uart_Status_ten DRV_UART_ReceiveData(DRV_UART_Instance_ten uartPinIdx_argu8,uint8_t *data_argptru8,uint32_t size_argu32)
{
    status_t status;
    if ((uartPinIdx_argu8 >= DRV_UART_MAX_INSTANCE) || (data_argptru8 == NULL) || (size_argu32 == 0U))
    {
        return DRV_UART_STATUS_ERROR;
    }
    status = LPUART_DRV_ReceiveData(uartInstances_arrst[uartPinIdx_argu8],data_argptru8,size_argu32);

    if (status == STATUS_SUCCESS)
    {
        return DRV_UART_STATUS_SUCCESS;
    }
    else if (status == STATUS_BUSY)
    {
        return DRV_UART_STATUS_BUSY;
    }
    return DRV_UART_STATUS_ERROR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_UART_SetRxBuffer
*   Description   : Sets the receive buffer for interrupt-driven reception
*   Parameters    : uart_instance - UART instance to configure
*                   rxBuffer - Pointer to receive buffer
*                   rxSize - Size of receive buffer in bytes
*   Return Value  : DRV_Uart_Status - Status of buffer configuration
*                   (SUCCESS/ERROR)
*  --------------------------------------------------------------------------- */
DRV_Uart_Status_ten DRV_UART_SetRxBuffer(DRV_UART_Instance_ten uartPinIdx_argu8,uint8_t *rxBuffer,uint32_t size_argu32)
{
    if ((uartPinIdx_argu8 >= DRV_UART_MAX_INSTANCE) || (rxBuffer == NULL) || (size_argu32 == 0U))
    {
        return DRV_UART_STATUS_ERROR;
    }
    LPUART_DRV_SetRxBuffer(uartInstances_arrst[uartPinIdx_argu8], rxBuffer, size_argu32);
    return DRV_UART_STATUS_SUCCESS;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_UART_Deinit
*   Description   : Deinitializes UART controller for specified instance
*   Parameters    : uart_instance - UART instance to deinitialize
*   Return Value  : DRV_Uart_Status - Status of deinitialization
*                   (SUCCESS/ERROR)
*  --------------------------------------------------------------------------- */
DRV_Uart_Status_ten DRV_UART_Deinit(DRV_UART_Instance_ten uartPinIdx_argu8)
{
    status_t status;

    /* Validate instance */
    if (uartPinIdx_argu8 >= DRV_UART_MAX_INSTANCE)
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_Deinit(uartInstances_arrst[uartPinIdx_argu8]);

    return (status == STATUS_SUCCESS) ? DRV_UART_STATUS_SUCCESS : DRV_UART_STATUS_ERROR;
}
