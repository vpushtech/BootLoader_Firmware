/*
 * BL_flash.c
 *
 *  Created on: Oct 14, 2025
 *      Author: PandurangaKarapothula
 */
#include "BL_flash.h"

static const FlashRegion_t FlashSectors[] =
{
    {0, 0x08000000, 0x08003FFF,  16 * 1024,  FLASH_SECTOR_0},
    {1, 0x08004000, 0x08007FFF,  16 * 1024,  FLASH_SECTOR_1},
    {2, 0x08008000, 0x0800BFFF,  16 * 1024,  FLASH_SECTOR_2},
    {3, 0x0800C000, 0x0800FFFF,  16 * 1024,  FLASH_SECTOR_3},
    {4, 0x08010000, 0x0801FFFF,  64 * 1024,  FLASH_SECTOR_4},
    {5, 0x08020000, 0x0803FFFF, 128 * 1024,  FLASH_SECTOR_5},
    {6, 0x08040000, 0x0805FFFF, 128 * 1024,  FLASH_SECTOR_6},
    {7, 0x08060000, 0x0807FFFF, 128 * 1024,  FLASH_SECTOR_7}
};

#define FLASH_SECTOR_COUNT  (sizeof(FlashSectors)/sizeof(FlashSectors[0]))

flash_status Flash_Sector_Erase(uint32_t address, uint32_t size)
{
	flash_status status;
	FLASH_EraseInitTypeDef erase_init;
	uint32_t sector_error;
	uint32_t end_address = (address + size) -1;
	const FlashRegion_t *start_sector = GetFlashSector(address);
	const FlashRegion_t *end_sector   = GetFlashSector((address + size) - 1);

	if (address < FLASH_START_ADDRESS || end_address > FLASH_END_ADDRESS)
		return FLASH_ERROR_INVALID_ADDRESS;

	if(HAL_FLASH_Unlock() != HAL_OK)
		return FLASH_ERROR_UNLOCK;

	erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
	erase_init.Sector = start_sector->hal_sector_id;
	erase_init.NbSectors = (end_sector->sector_num - start_sector->sector_num) + 1;
	erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

	status = HAL_FLASHEx_Erase(&erase_init,&sector_error);
	if(status != FLASH_OK)
	{
		return FLASH_ERASE_ERROR;
	}

	HAL_FLASH_Lock();

	if (status != FLASH_OK || sector_error != 0xFFFFFFFF)
		return FLASH_ERASE_ERROR;

	return FLASH_OK;
}

flash_status Flash_App_Erase(void)
{
	flash_status status;

	status = Flash_Sector_Erase(APP_START_ADDRESS,APP_SIZE);
	if(status != FLASH_OK)
	{
		return FLASH_ERASE_ERROR;
	}
	return FLASH_OK;
}

flash_status Flash_Write_Byte(uint32_t address, uint8_t *buffer, uint32_t size)
{
	flash_status status;
	uint32_t end_address = address + size;

	if (address < FLASH_START_ADDRESS || end_address > FLASH_END_ADDRESS)
		return FLASH_ERROR_INVALID_ADDRESS;

	if (HAL_FLASH_Unlock() != HAL_OK)
		return FLASH_ERROR_UNLOCK;

	for (uint32_t i = 0; i < size; i++)
	{
		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address + i, buffer[i]);
		if (status != FLASH_OK)
		{
			HAL_FLASH_Lock();
			return FLASH_WRITE_ERROR;
		}
	}
	HAL_FLASH_Lock();
	return FLASH_OK;
}

flash_status Flash_Write_Word(uint32_t address, uint8_t *buffer, uint32_t size)
{
	flash_status status;
	uint32_t end_address = address + size;

	if (address < FLASH_START_ADDRESS || end_address > FLASH_END_ADDRESS)
		return FLASH_ERROR_INVALID_ADDRESS;

	if (HAL_FLASH_Unlock() != HAL_OK)
		return FLASH_ERROR_UNLOCK;

	for (uint32_t i = 0; i < size; i += 4)
	{
		uint32_t word = *(uint32_t *)(buffer + i);
		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, word);
		if (status != FLASH_OK)
		{
			HAL_FLASH_Lock();
			return FLASH_WRITE_ERROR;
		}
	}
	HAL_FLASH_Lock();
	return FLASH_OK;
}

flash_status Flash_Read(uint32_t address, uint8_t *buffer, uint32_t size)
{
	uint32_t end_address = address + size;

	if (address < FLASH_START_ADDRESS || end_address > FLASH_END_ADDRESS)
		return FLASH_ERROR_INVALID_ADDRESS;

	for(uint32_t i=0;i<size;i++)
	{
		buffer[i] = *(volatile uint8_t *)(address + i);
	}
	return FLASH_OK;
}

const FlashRegion_t* GetFlashSector(uint32_t address)
{
    for (uint32_t i = 0; i < FLASH_SECTOR_COUNT; i++)
    {
        if (address >= FlashSectors[i].start_addr && address <= FlashSectors[i].end_addr)
            return &FlashSectors[i];
    }
    return NULL;
}

