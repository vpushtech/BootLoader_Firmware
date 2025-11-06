/*
 * BL_flash.c
 * Fixed for safe erase/program on S32K1xx
 */
#include <drv_flash.h>

/* ==================== GLOBLE VARIABLES ==================== */
flash_ssd_config_t flashSSDConfig;

/* ==================== PUBLIC FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_FLASH_Init_gen
*   Description   : Initializes FLASH controller and configures CCIF interrupt
*   Parameters    : None
*   Return Value  : DRV_FlashStatus_en - Status of initialization
*  --------------------------------------------------------------------------- */
DRV_FlashStatus_en DRV_FLASH_Init_gen(void)
{
    INT_SYS_InstallHandler(FTFC_IRQn, CCIF_Handler, (isr_t*)0);

    DRV_FlashStatus_en status = FLASH_DRV_Init(&Flash_InitConfig0, &flashSSDConfig);
    if (status != FLASH_OK)
        return FLASH_ERROR;
    return FLASH_OK;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_FLASH_SectorErase_gen
*   Description   : Erases a sector of FLASH memory with interrupt protection
*   Parameters    : address - Starting address of sector to erase
*                   size - Size of sector to erase in bytes
*   Return Value  : DRV_FlashStatus_en - Status of erase operation
*  --------------------------------------------------------------------------- */
DRV_FlashStatus_en DRV_FLASH_SectorErase_gen(uint32_t address_argu32, uint32_t size_argu32)
{
	DRV_FlashStatus_en status;

    INT_SYS_DisableIRQGlobal();
    status = FLASH_DRV_EraseSector(&flashSSDConfig, address_argu32, size_argu32);
    INT_SYS_EnableIRQGlobal();

    if (status != FLASH_OK)
        return FLASH_ERROR;
    return FLASH_OK;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_FLASH_Write_gen
*   Description   : Programs data to FLASH memory with address validation and
*                   interrupt protection. Requires 8-byte aligned address and size_argu32.
*   Parameters    : address - Destination address in FLASH (must be 8-byte aligned)
*                   buffer - Pointer to source data buffer
*                   size - Number of bytes to program (must be multiple of 8)
*   Return Value  : DRV_FlashStatus_en - Status of write operation
*  --------------------------------------------------------------------------- */
DRV_FlashStatus_en DRV_FLASH_Write_gen(uint32_t address_argu32, uint8_t *data_argptru8, uint32_t size_argu32)
{
	DRV_FlashStatus_en status = FLASH_ERROR;
    if (!data_argptru8 || size_argu32 == 0 || size_argu32 % 8 != 0 || address_argu32 % 8 != 0) {
        return FLASH_ERROR;
    }
    if (address_argu32 < APP_START_ADDRESS || address_argu32 >= (PFLASH_END - size_argu32)) {
        return FLASH_ERROR;
    }

    INT_SYS_DisableIRQGlobal();
    status = FLASH_DRV_Program(&flashSSDConfig, address_argu32, size_argu32, data_argptru8);
    INT_SYS_EnableIRQGlobal();

    if (status != FLASH_OK)
		return FLASH_ERROR;
	return FLASH_OK;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_FLASH_Read_gen
*   Description   : Reads data from FLASH memory with address boundary validation
*   Parameters    : address - Source address in FLASH to read from
*                   buffer - Pointer to destination buffer
*                   size - Number of bytes to read
*   Return Value  : DRV_FlashStatus_en - Status of read operation
*  --------------------------------------------------------------------------- */
DRV_FlashStatus_en DRV_FLASH_Read_gen(uint32_t address_argu32, uint8_t *data_argptru8, uint32_t size_argu32)
{
    if (address_argu32 + size_argu32 > PFLASH_END)
        return FLASH_ERROR_INVALID_ADDRESS;

    for (uint32_t i = 0; i < size_argu32; ++i)
        data_argptru8[i] = *(volatile uint8_t *)(address_argu32 + i);
    return FLASH_OK;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : CCIF_Handler
*   Description   : Interrupt Service Routine for FLASH Command Complete Interrupt.
*                   Clears the CCIF interrupt flag.
*   Parameters    : None
*   Return Value  : None
*  --------------------------------------------------------------------------- */
void CCIF_Handler(void)
{
    FTFx_FCNFG &= ~FTFx_FCNFG_CCIE_MASK;
}
