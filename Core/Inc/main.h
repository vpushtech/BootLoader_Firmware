/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

#define FLASH_WRITE_COMMAND		 		49
#define UART_TIMEOUT 					3000
#define FLASH_UserApp_ADDRESS 			0x08008000
#define MetaDataAddress 				0x08070004
#define BOOT_FLAG_ADDRESS 				0x08070000
#define BOOT_FLAG_VALUE    				0xABCDABCD
#define APP_STATUS_OK					1
#define USER_APP_SIZE					(32*1024)

void JumpToBootloader(void);
void JumpToUserApp(void);
HAL_StatusTypeDef Bootloader_EraseUserApp(void);
HAL_StatusTypeDef bootloader_handle_FlashWrite(void);
HAL_StatusTypeDef Flash_Write_Binary(uint32_t filesize);
void Error_Handler(void);
HAL_StatusTypeDef update_data(uint8_t status);
uint32_t crc32_flash();
uint32_t crc32_calculate(uint8_t* data, uint32_t length);
uint8_t Read_app_status(void);
uint32_t check_boot_flag(void);
void set_boot_flag(void);

#ifdef __cplusplus
}
#endif

#endif
