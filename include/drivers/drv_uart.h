#ifndef PERIPH_DRV_DRV_UART_H_
#define PERIPH_DRV_DRV_UART_H_

/*******************************************************************************
 *  HEADER FILE INCLUDES
 ******************************************************************************/
#include "status.h"
#include <stdint.h>
#include <stdbool.h>

/* ==================== TYPE DEFINITIONS ==================== */
typedef enum
{
    DRV_UART_INSTANCE_1 = 0,
    DRV_UART_MAX_INSTANCE
} DRV_UART_Instance_ten;

typedef enum
{
	DRV_UART_STATUS_SUCCESS = 0,
	DRV_UART_STATUS_ERROR,
	DRV_UART_STATUS_BUSY,
	DRV_UART_STATUS_TIMEOUT
} DRV_Uart_Status_ten;


/* ==================== FUNCTION PROTOTYPES ==================== */

/* ==================== INITIALIZATION/CONFIGURATION ==================== */
DRV_Uart_Status_ten DRV_UART_Init(DRV_UART_Instance_ten uartPinIdx_argu8);
DRV_Uart_Status_ten DRV_UART_Deinit(DRV_UART_Instance_ten uartPinIdx_argu8);

/* ==================== TRANSMIT & OPERATIONS ==================== */
DRV_Uart_Status_ten DRV_UART_SendDataBlocking(DRV_UART_Instance_ten uartPinIdx_argu8,const uint8_t *data_argptru8,uint32_t size_argu32);
DRV_Uart_Status_ten DRV_UART_SendData(DRV_UART_Instance_ten uartPinIdx_argu8,const uint8_t *data_argptru8,uint32_t size_argu32);
DRV_Uart_Status_ten DRV_UART_SetRxBuffer(DRV_UART_Instance_ten uartPinIdx_argu8,uint8_t *rxBuffer,uint32_t size_argu32);

/* ==================== RECEIVE & OPERATIONS ==================== */
DRV_Uart_Status_ten DRV_UART_ReceiveDataBlocking(DRV_UART_Instance_ten uartPinIdx_argu8,uint8_t *data_argptru8,uint32_t size_argu32);
DRV_Uart_Status_ten DRV_UART_ReceiveData(DRV_UART_Instance_ten uartPinIdx_argu8,uint8_t *data_argptru8,uint32_t size_argu32);

#endif /* PERIPH_DRV_DRV_UART_H_ */
