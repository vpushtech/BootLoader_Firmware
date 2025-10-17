/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "BL_flash.h"
#include "BL_UART.h"

void SystemClock_Config(void);

uint32_t bytes_written;
uint8_t cmd;

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  UART_Init(DRV_UART_INSTANCE_2);

  if(check_boot_flag() != BOOT_FLAG_VALUE && Read_app_status() == APP_STATUS_OK)
  {
	  JumpToUserApp();
  }
  JumpToBootloader();

  while(1);
}

void JumpToUserApp(void)
{
		uint32_t mspValue = *(volatile uint32_t *)FLASH_UserApp_ADDRESS;
		__set_MSP(mspValue);

		SCB->VTOR = FLASH_UserApp_ADDRESS;

		void (*app_reset_handler)(void) = (void *)(*((volatile uint32_t *)(FLASH_UserApp_ADDRESS + 4U)));
		app_reset_handler();
}

void JumpToBootloader(void)
{
	uint32_t Py_CRC=0;
	while(1)
	{
		UART_ReceiveDataBlocking(DRV_UART_INSTANCE_2,&cmd,sizeof(cmd));
		if(cmd == FLASH_WRITE_COMMAND)
		{
			Flash_Sector_Erase(FLASH_UserApp_ADDRESS,USER_APP_SIZE);
			update_data(0x00);
			if(bootloader_handle_FlashWrite() != HAL_OK)
			{
				NVIC_SystemReset();
			}
			UART_ReceiveDataBlocking(DRV_UART_INSTANCE_2,(uint8_t*)&Py_CRC,sizeof(Py_CRC));
			uint32_t crc = crc32_flash();
			if(crc == Py_CRC)
			{
				update_data(0x01);
				NVIC_SystemReset();
			}
		}
	}
}

HAL_StatusTypeDef bootloader_handle_FlashWrite(void)
{
	uint8_t rx_buf[4];
	HAL_Delay(1000);
	UART_SendData(DRV_UART_INSTANCE_2,(uint8_t *)"OK\r\n",3);
	UART_ReceiveDataBlocking(DRV_UART_INSTANCE_2,rx_buf, 4);
	uint32_t filesize = (rx_buf[0]) | (rx_buf[1] << 8) | (rx_buf[2] << 16) | (rx_buf[3] << 24);
	if(Flash_Write_Binary(filesize) != HAL_OK)
	{
		return HAL_ERROR;
	}
	return HAL_OK;
}

HAL_StatusTypeDef Flash_Write_Binary(uint32_t filesize)
{
    uint32_t flash_addr = FLASH_UserApp_ADDRESS;
    uint32_t chunk = 256;
    uint8_t rx_buf[chunk];
    bytes_written = 0;
    while(bytes_written < filesize)
    {
        if (UART_ReceiveData(DRV_UART_INSTANCE_2,rx_buf, chunk,3000) != UART_OK)
        {
            return HAL_ERROR;
        }
        Flash_Write_Byte(flash_addr, rx_buf,chunk);
        flash_addr += chunk;
        bytes_written += chunk;
    }
    return HAL_OK;
}

uint8_t Read_app_status(void)
{
    uint8_t status;
    status = *(uint8_t *)MetaDataAddress;
    return status;
}

HAL_StatusTypeDef update_data(uint8_t app_status)
{
	HAL_StatusTypeDef status;
	uint32_t Address = MetaDataAddress;

	Flash_Sector_Erase((uint32_t)BOOT_FLAG_ADDRESS,(16*1024));

	status = Flash_Write_Byte(MetaDataAddress, &app_status, sizeof(app_status));
	if (status != HAL_OK) goto end;

	status = Flash_Write_Word(Address + 4, (uint8_t *)&bytes_written, sizeof(bytes_written));
	if (status != HAL_OK) goto end;

end:
	HAL_FLASH_Lock();
	return status;
}

uint32_t crc32_calculate(uint8_t* data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t poly = 0x04C11DB7;

    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= ((uint32_t)data[i]) << 24;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80000000)
                crc = (crc << 1) ^ poly;
            else
                crc <<= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t crc32_flash()
{
    uint8_t* ptr = (uint8_t*)FLASH_UserApp_ADDRESS;
    return crc32_calculate(ptr, bytes_written);
}

uint32_t check_boot_flag(void)
{
    return *(volatile uint32_t*)BOOT_FLAG_ADDRESS;
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
