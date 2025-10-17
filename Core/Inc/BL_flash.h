/*
 * BL_flash.h
 *
 *  Created on: Oct 14, 2025
 *      Author: PandurangaKarapothula
 */

#ifndef BL_FLASH_H_
#define BL_FLASH_H_

#include "stm32f4xx_hal.h"

typedef enum {
	FLASH_OK = 0x00,
	FLASH_ERROR = 0x01,
	FLASH_WRITE_ERROR = 0x02,
	FLASH_ERASE_ERROR = 0x03,
	FLASH_ERROR_UNLOCK = 0x04,
	FLASH_ERROR_INVALID_ADDRESS = 0x05,
}flash_status;

typedef struct
{
    uint8_t   sector_num;
    uint32_t  start_addr;
    uint32_t  end_addr;
    uint32_t  size;
    uint32_t  hal_sector_id;
} FlashRegion_t;

#define FLASH_START_ADDRESS				0x08000000
#define FLASH_SIZE						(512*1024)
#define FLASH_END_ADDRESS				0x0807FFFF

#define BOOT_START_ADDRESS				0x08000000
#define BOOT_SIZE						(32*1024)
#define BOOT_END_ADDRESS				0x08007FFF

#define APP_START_ADDRESS				0x08008000
#define APP_SIZE						(480*1024)
#define APP_END_ADDRESS					0x0807FFFF

flash_status Flash_Sector_Erase(uint32_t address, uint32_t size);
flash_status Flash_App_Erase(void);
flash_status Flash_Write_Byte(uint32_t address, uint8_t *data, uint32_t size);
flash_status Flash_Write_Word(uint32_t address, uint8_t *data, uint32_t size);
flash_status Flash_Read(uint32_t address, uint8_t *buffer, uint32_t size);
const FlashRegion_t* GetFlashSector(uint32_t address);

#endif /* BL_FLASH_H_ */
