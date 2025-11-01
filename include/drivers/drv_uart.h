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
} DRV_UART_Instance_En;

typedef enum
{
    DRV_UART_STATUS_SUCCESS = 0,
    DRV_UART_STATUS_ERROR,
    DRV_UART_STATUS_BUSY,
    DRV_UART_STATUS_TIMEOUT
} DRV_Uart_Status;


/* ==================== FUNCTION PROTOTYPES ==================== */

/* Initialization/Deinitialization */
DRV_Uart_Status DRV_UART_Init(DRV_UART_Instance_En uart_instance);
DRV_Uart_Status DRV_UART_Deinit(DRV_UART_Instance_En uart_instance);

/* Transmission Operations */
DRV_Uart_Status DRV_UART_SendDataBlocking(DRV_UART_Instance_En uart_instance,
                                         const uint8_t *sendData,
                                         uint32_t length);
DRV_Uart_Status DRV_UART_SendData(DRV_UART_Instance_En uart_instance,
                                 const uint8_t *sendData,
                                 uint32_t length);

/* Reception Operations */
DRV_Uart_Status DRV_UART_ReceiveData(DRV_UART_Instance_En uart_instance,
                                    uint8_t *recdata,
                                    uint32_t length);
DRV_Uart_Status DRV_UART_ReceiveDataBlocking(DRV_UART_Instance_En uart_instance,
                                           uint8_t *recdata,
                                           uint32_t length);

/* Status and Control */
DRV_Uart_Status DRV_UART_GetTransmitStatus(DRV_UART_Instance_En uart_instance,
                                          uint32_t *bytesRemaining);
DRV_Uart_Status DRV_UART_GetReceiveStatus(DRV_UART_Instance_En uart_instance,
                                         uint32_t *bytesRemaining);

/* Buffer Management */
DRV_Uart_Status DRV_UART_SetRxBuffer(DRV_UART_Instance_En uart_instance,
                                    uint8_t *rxBuffer,
                                    uint32_t rxSize);




/* IRQ Handlers */
void LPUART1_RxTx_IRQHandler(void);

#endif /* PERIPH_DRV_DRV_UART_H_ */
