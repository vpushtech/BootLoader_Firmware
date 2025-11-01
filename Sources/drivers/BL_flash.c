/*
 * BL_flash.c
 * Fixed for safe erase/program on S32K1xx
 */
#include "BL_flash.h"
#include "S32K144.h"
#include "interrupt_manager.h"
#include<string.h>

flash_ssd_config_t flashSSDConfig;

START_FUNCTION_DECLARATION_RAMSECTION
void CCIF_Callback(void)
END_FUNCTION_DECLARATION_RAMSECTION

flash_status Flash_init(void)
{
    INT_SYS_InstallHandler(FTFC_IRQn, CCIF_Handler, (isr_t*)0);

    if (FLASH_DRV_Init(&Flash_InitConfig0, &flashSSDConfig) != STATUS_SUCCESS)
        return FLASH_ERROR;

    return FLASH_OK;
}

flash_status Flash_Sector_Erase(uint32_t address, uint32_t size)
{
    flash_status status;

    INT_SYS_DisableIRQGlobal();
    status = FLASH_DRV_EraseSector(&flashSSDConfig, address, size);
    INT_SYS_EnableIRQGlobal();

    if (status != FLASH_OK)
        return FLASH_ERROR;
    return FLASH_OK;
}

flash_status Flash_App_Erase(void)
{
    uint32_t addr = APP_START_ADDRESS;
    const uint32_t sector_size = SECTOR_SIZE;

    for (uint32_t i = 0; i < (APP_SIZE / sector_size); i++)
    {
        flash_status status = Flash_Sector_Erase(addr, sector_size);
        if (status != FLASH_OK)
            return FLASH_ERROR;
        addr += sector_size;
    }
    return FLASH_OK;
}

void CriticalSection_Enter_Simple(void)
{
    INT_SYS_DisableIRQGlobal();
}

void CriticalSection_Exit_Simple(void)
{
    INT_SYS_EnableIRQGlobal();
}

flash_status Flash_Write_Byte(uint32_t address, uint8_t *buffer, uint32_t size)
{
    flash_status status = FLASH_ERROR;
    if (!buffer || size == 0 || size % 8 != 0 || address % 8 != 0) {
        return FLASH_ERROR;
    }
    if (address < APP_START_ADDRESS || address >= (PFLASH_END - size)) {
        return FLASH_ERROR;
    }

    CriticalSection_Enter_Simple();
    status = FLASH_DRV_Program(&flashSSDConfig, address, size, buffer);
    CriticalSection_Exit_Simple();

    return status;
}

flash_status Flash_Read(uint32_t address, uint8_t *buffer, uint32_t size)
{
    if (address + size > PFLASH_END)
        return FLASH_ERROR_INVALID_ADDRESS;

    for (uint32_t i = 0; i < size; ++i)
        buffer[i] = *(volatile uint8_t *)(address + i);
    return FLASH_OK;
}

void CCIF_Handler(void)
{
    FTFx_FCNFG &= ~FTFx_FCNFG_CCIE_MASK;
}

START_FUNCTION_DEFINITION_RAMSECTION
void CCIF_Callback(void)
{
    if ((FTFx_FCNFG & FTFx_FCNFG_CCIE_MASK) == 0u)
    {
        FTFx_FCNFG |= FTFx_FCNFG_CCIE_MASK;
    }
}
END_FUNCTION_DEFINITION_RAMSECTION
