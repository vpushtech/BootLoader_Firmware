/*
 * BL_UART.c
 *
 *  Created on: Oct 15, 2025
 *      Author: PandurangaKarapothula
 */

#include "BL_UART.h"

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

static void MX_USART2_UART_Init(void);

static const DRV_UART_HWInstance_EN DRV_UartInstances_arr[DRV_UART_MAX_INSTANCE] = {DRV_UART_HW_USART1,
																					DRV_UART_HW_USART2,
																					DRV_UART_HW_USART3};

UART_status UART_Init(DRV_UART_Instance_En uart_instance)
{
	switch(DRV_UartInstances_arr[uart_instance])
	{
		case DRV_UART_HW_USART1:
			//MX_USART1_UART_Init();
			break;

		case DRV_UART_HW_USART2:
			MX_USART2_UART_Init();
			break;

		case DRV_UART_HW_USART3:
			//MX_USART3_UART_Init();
			break;

		default:
			return UART_ERROR;
	}
	return UART_OK;
}

UART_status UART_SendData(DRV_UART_Instance_En uart_instance,uint8_t *data, uint32_t length)
{
	UART_status status;

	if(data == NULL || length == 0)
		return UART_ERROR;

	switch(DRV_UartInstances_arr[uart_instance])
	{
	case DRV_UART_HW_USART1:
		status = HAL_UART_Transmit(&huart1,data,length,HAL_MAX_DELAY);
		break;

	case DRV_UART_HW_USART2:
		status = HAL_UART_Transmit(&huart2,data,length,HAL_MAX_DELAY);
		break;

	case DRV_UART_HW_USART3:
		status = HAL_UART_Transmit(&huart3,data,length,HAL_MAX_DELAY);
		break;

	default:
		return UART_ERROR;
	}
	if(status != UART_OK)
	{
		return UART_SENDING_ERROR;
	}
	return UART_OK;
}

UART_status UART_ReceiveDataBlocking(DRV_UART_Instance_En uart_instance,uint8_t *data, uint32_t length)
{
	UART_status status;

	switch(DRV_UartInstances_arr[uart_instance])
	{
		case DRV_UART_HW_USART1:
			status = HAL_UART_Receive(&huart1,data,length,HAL_MAX_DELAY);
			break;

		case DRV_UART_HW_USART2:
			status = HAL_UART_Receive(&huart2,data,length,HAL_MAX_DELAY);
			break;

		case DRV_UART_HW_USART3:
			status = HAL_UART_Receive(&huart3,data,length,HAL_MAX_DELAY);
			break;

		default:
			return UART_ERROR;
	}

	if(status != UART_OK)
	{
		return UART_RECEVING_ERROR;
	}
	return UART_OK;
}

UART_status UART_ReceiveData(DRV_UART_Instance_En uart_instance,uint8_t *data, uint32_t length,uint32_t TIMEOUT)
{
	UART_status status;

	switch(DRV_UartInstances_arr[uart_instance])
	{
		case DRV_UART_HW_USART1:
			status = HAL_UART_Receive(&huart1,data,length,TIMEOUT);
			break;

		case DRV_UART_HW_USART2:
			status = HAL_UART_Receive(&huart2,data,length,TIMEOUT);
			break;

		case DRV_UART_HW_USART3:
			status = HAL_UART_Receive(&huart3,data,length,TIMEOUT);
			break;

		default:
			return UART_ERROR;
	}
	if(status != UART_OK)
	{
		return UART_RECEVING_ERROR;
	}
	return UART_OK;
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

