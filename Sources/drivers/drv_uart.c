/* ==================== INCLUDE FILES ==================== */
#include "drv_uart.h"
#include "lpuart_driver.h"
#include "lpuart1.h"
/* ==================== DEFINES ==================== */
#define TIMEOUT_MS             1000U
#define DEFAULT_IRQ_PRIORITY   3U

/* ==================== STATIC VARIABLES ==================== */
static const uint32_t DRV_UartInstances_arr[DRV_UART_MAX_INSTANCE] = {INST_LPUART1};
static lpuart_state_t DRV_lpuartState[DRV_UART_MAX_INSTANCE];

/* ==================== STATIC FUNCTIONS ==================== */

/*void rxCallback(void *driverState, uart_event_t event, void *userData)
{
    (void)driverState;
    (void)userData;
}*/


DRV_Uart_Status DRV_UART_Init(DRV_UART_Instance_En uart_instance)
{
    status_t status;
    if (uart_instance >= DRV_UART_MAX_INSTANCE)
    {
        return DRV_UART_STATUS_ERROR;
    }
    status = LPUART_DRV_Init(DRV_UartInstances_arr[uart_instance],&DRV_lpuartState[uart_instance],&lpuart1_InitConfig0);
    if( STATUS_SUCCESS==status)
    {
        //LPUART_DRV_InstallRxCallback(INST_LPUART1, rxCallback, NULL);
    }
    return DRV_UART_STATUS_ERROR;
}

DRV_Uart_Status DRV_UART_SendDataBlocking(DRV_UART_Instance_En uart_instance,const uint8_t *sendData,uint32_t length)
{
    status_t status;
    if ((uart_instance >= DRV_UART_MAX_INSTANCE) || (sendData == NULL) || (length == 0U))
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_SendDataBlocking(DRV_UartInstances_arr[uart_instance],sendData,length,TIMEOUT_MS);
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

DRV_Uart_Status DRV_UART_SendData(DRV_UART_Instance_En uart_instance,const uint8_t *sendData,uint32_t length)
{
    status_t status;
    if ((uart_instance >= DRV_UART_MAX_INSTANCE) || (sendData == NULL) || (length == 0U))
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_SendData(DRV_UartInstances_arr[uart_instance],sendData,length);
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

DRV_Uart_Status DRV_UART_ReceiveDataBlocking(DRV_UART_Instance_En uart_instance,uint8_t *recdata,uint32_t length)
{
    status_t status;
    if ((uart_instance >= DRV_UART_MAX_INSTANCE) || (recdata == NULL) || (length == 0U))
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_ReceiveDataBlocking(DRV_UartInstances_arr[uart_instance],recdata,length,TIMEOUT_MS);
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

DRV_Uart_Status DRV_UART_ReceiveData(DRV_UART_Instance_En uart_instance,uint8_t *recdata,uint32_t length)
{
    status_t status;
    if ((uart_instance >= DRV_UART_MAX_INSTANCE) || (recdata == NULL) || (length == 0U))
    {
        return DRV_UART_STATUS_ERROR;
    }
    status = LPUART_DRV_ReceiveData(DRV_UartInstances_arr[uart_instance],recdata,length);

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

DRV_Uart_Status DRV_UART_GetTransmitStatus(DRV_UART_Instance_En uart_instance,uint32_t *bytesRemaining)
{
    status_t status;
    if ((uart_instance >= DRV_UART_MAX_INSTANCE) || (bytesRemaining == NULL))
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_GetTransmitStatus(DRV_UartInstances_arr[uart_instance],bytesRemaining);

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

DRV_Uart_Status DRV_UART_GetReceiveStatus(DRV_UART_Instance_En uart_instance,uint32_t *bytesRemaining)
{
    status_t status;

    /* Validate parameters */
    if ((uart_instance >= DRV_UART_MAX_INSTANCE) || (bytesRemaining == NULL))
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_GetReceiveStatus(DRV_UartInstances_arr[uart_instance],
                                       bytesRemaining);

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

DRV_Uart_Status DRV_UART_SetRxBuffer(DRV_UART_Instance_En uart_instance,uint8_t *rxBuffer,uint32_t rxSize)
{
    if ((uart_instance >= DRV_UART_MAX_INSTANCE) || (rxBuffer == NULL) || (rxSize == 0U))
    {
        return DRV_UART_STATUS_ERROR;
    }
    LPUART_DRV_SetRxBuffer(DRV_UartInstances_arr[uart_instance], rxBuffer, rxSize);
    return DRV_UART_STATUS_SUCCESS;
}


DRV_Uart_Status DRV_UART_Deinit(DRV_UART_Instance_En uart_instance)
{
    status_t status;

    /* Validate instance */
    if (uart_instance >= DRV_UART_MAX_INSTANCE)
    {
        return DRV_UART_STATUS_ERROR;
    }

    status = LPUART_DRV_Deinit(DRV_UartInstances_arr[uart_instance]);

    return (status == STATUS_SUCCESS) ? DRV_UART_STATUS_SUCCESS : DRV_UART_STATUS_ERROR;
}

