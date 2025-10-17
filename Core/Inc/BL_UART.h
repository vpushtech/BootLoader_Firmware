/*
 * BL_UART.h
 *
 *  Created on: Oct 15, 2025
 *      Author: PandurangaKarapothula
 */

#ifndef INC_BL_UART_H_
#define INC_BL_UART_H_

#include "stm32f4xx_hal.h"

typedef enum {
	UART_OK = 0x00,
	UART_ERROR = 0x01,
	UART_RECEVING_ERROR = 0x02,
	UART_SENDING_ERROR = 0x03,
}UART_status;

typedef enum {
	DRV_UART_INSTANCE_1 = 0,
	DRV_UART_INSTANCE_2,
	DRV_UART_INSTANCE_3,
	DRV_UART_MAX_INSTANCE
}DRV_UART_Instance_En;

typedef enum {
	DRV_UART_HW_USART1 = 0x01,
	DRV_UART_HW_USART2 = 0x02,
	DRV_UART_HW_USART3 = 0x03
}DRV_UART_HWInstance_EN;

#define UART_TIMEOUT			3000

UART_status UART_Init(DRV_UART_Instance_En uart_instance);
UART_status UART_SendData(DRV_UART_Instance_En uart_instance,uint8_t *data, uint32_t length);
UART_status UART_ReceiveDataBlocking(DRV_UART_Instance_En uart_instance,uint8_t *data, uint32_t length);
UART_status UART_ReceiveData(DRV_UART_Instance_En uart_instance,uint8_t *data, uint32_t length,uint32_t TIMEOUT);

void Error_Handler(void);

#endif /* INC_BL_UART_H_ */
