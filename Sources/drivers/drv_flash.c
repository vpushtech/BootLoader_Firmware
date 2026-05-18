/*******************************************************************************
 *  Description     : Flash Driver
 *  Author          : Rushikesh
 *  Created On      : 08-Jul-2025
 *  Version         : 2.0
 *  Modification History:
 *  Date        Author      Description
 *  ----------------------------------------------------------------------------
 *  18-Jul-2025 RUSHIKESH   Flash Driver Architecture Implementation
 *  31-Jul-2025 RUSHIKESH   Flash Driver Testing completed
 *  11-Aug-2025 RUSHIKESH   Guidelines Followed the naming Architecture Implementation
 * 29-Jan-2026 RUSHIKESH   Added init tracking, address validation, and proper error handling
 ******************************************************************************/

/* ==================== INCLUDE FILES ==================== */
#include "drv_flash.h"

/* ==================== STATIC VARIABLES ==================== */
static flash_ssd_config_t DRV_flashSSDConfig_st;

/* ==================== GLOBAL VARIABLES ==================== */
volatile BIN DRV_flashInitStatus_mb = false;

/* ==================== PUBLIC FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_FLASH_IsAddressInRange_mb
*   Description   : Checks if address is within valid flash range
*   Parameters    : address_argu32 - Address to check
*                   size_argu32 - Size to check
*   Return Value  : bool - True if valid, false otherwise
*  --------------------------------------------------------------------------- */
static BIN DRV_FLASH_IsAddressInRange_mb(U32 address_argu32, U32 size_argu32)
{
    U32 end_address = address_argu32 + size_argu32 - 1;

    /* Check if address is within DF block (Data Flash) */
    if ((address_argu32 >= DRV_FLASH_BASE_ADDRESS) &&
        (end_address < DRV_FLASH_END_ADDRESS))
    {
        return true;
    }

    /* Check if address is within PF block (Program Flash) */
    if ((end_address < DRV_FLASH_PF_END_ADDRESS))
    {
        return true;
    }

    return false;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_FLASH_IsSectorErased_mb
*   Description   : Checks if flash sector is already erased
*   Parameters    : address_argu32 - Starting address
*                   size_argu32 - Size to check
*   Return Value  : bool - True if erased, false otherwise
*  --------------------------------------------------------------------------- */
static BIN DRV_FLASH_IsSectorErased_mb(U32 address_argu32, U32 size_argu32)
{
    volatile U8 *flash_address = (volatile U8 *)address_argu32;
    U32 index_u32;

    for (index_u32 = 0; index_u32 < size_argu32; index_u32++)
    {
        if (flash_address[index_u32] != DRV_FLASH_ERASED_VALUE)
        {
            return false;
        }
    }

    return true;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_FLASH_Init_gen
*   Description   : Initializes flash driver and installs interrupt handler
*   Parameters    : None
*   Return Value  : DRV_flashStatus_ten - Status of initialization
*  --------------------------------------------------------------------------- */
DRV_flashStatus_ten DRV_FLASH_Init_gen(void)
{
    status_t status;

    /* Check if flash is already initialized */
    if (DRV_flashInitStatus_mb)
    {
        return DRV_FLASH_ALREADY_INITIALIZED;
    }

    /* Install CCIF interrupt handler */
    INT_SYS_InstallHandler(FTFC_IRQn, CCIF_Handler, (isr_t*)0);

    /* Initialize flash driver */
    status = FLASH_DRV_Init(&Flash1_InitConfig0, &DRV_flashSSDConfig_st);

    if (status == STATUS_SUCCESS)
    {
        DRV_flashInitStatus_mb = true;
        return DRV_FLASH_SUCCESS;
    }

    return DRV_FLASH_FAILED;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_FLASH_EraseSector_gen
*   Description   : Erases flash sector(s) at specified address
*   Parameters    : address_argu32 - Starting address to erase
*                   size_argu32 - Size to erase
*   Return Value  : DRV_flashStatus_ten - Status of erase operation
*  --------------------------------------------------------------------------- */
DRV_flashStatus_ten DRV_FLASH_EraseSector_gen(U32 address_argu32, U32 size_argu32)
{
    status_t status;

    /* Check if flash is initialized */
    if (!DRV_flashInitStatus_mb)
    {
        return DRV_FLASH_NOT_INITIALIZED;
    }

    /* Validate parameters */
    if (size_argu32 == 0)
    {
        return DRV_FLASH_INVALID_SIZE;
    }

    /* Check address range */
    if (!DRV_FLASH_IsAddressInRange_mb(address_argu32, size_argu32))
    {
        return DRV_FLASH_ADDRESS_OUT_OF_RANGE;
    }

    /* Check 8-byte alignment */
    if ((address_argu32 % DRV_FLASH_PHRASE_SIZE != 0) ||
        (size_argu32 % DRV_FLASH_PHRASE_SIZE != 0))
    {
        return DRV_FLASH_INVALID_ALIGNMENT;
    }

    /* Check if sector is already erased */
    if (DRV_FLASH_IsSectorErased_mb(address_argu32, size_argu32))
    {
        return DRV_FLASH_SUCCESS; /* Already erased, no need to erase again */
    }

    /* Enter critical section */
    INT_SYS_DisableIRQGlobal();

    /* Perform sector erase */
    status = FLASH_DRV_EraseSector(&DRV_flashSSDConfig_st, address_argu32, size_argu32);

    /* Exit critical section */
    INT_SYS_EnableIRQGlobal();

    if (status != STATUS_SUCCESS)
    {
        return DRV_FLASH_ERASE_ERROR;
    }

    return DRV_FLASH_SUCCESS;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_FLASH_WriteBlock_gen
*   Description   : Writes data to flash memory
*   Parameters    : address_argu32 - Destination address
*                   data_argptru8 - Pointer to source data
*                   size_argu32 - Number of bytes to write
*   Return Value  : DRV_flashStatus_ten - Status of write operation
*  --------------------------------------------------------------------------- */
DRV_flashStatus_ten DRV_FLASH_WriteBlock_gen(U32 address_argu32, const U8 *data_argptru8, U32 size_argu32)
{
    status_t status;
    U32 index_u32;
    volatile U8 *flash_address;

    /* Check if flash is initialized */
    if (!DRV_flashInitStatus_mb)
    {
        return DRV_FLASH_NOT_INITIALIZED;
    }

    /* Validate parameters */
    if (data_argptru8 == NULL)
    {
        return DRV_FLASH_NULL_POINTER;
    }

    if (size_argu32 == 0)
    {
        return DRV_FLASH_INVALID_SIZE;
    }

    /* Check address range */
    if (!DRV_FLASH_IsAddressInRange_mb(address_argu32, size_argu32))
    {
        return DRV_FLASH_ADDRESS_OUT_OF_RANGE;
    }

    /* Check 8-byte alignment */
    if ((address_argu32 % DRV_FLASH_PHRASE_SIZE != 0) ||
        (size_argu32 % DRV_FLASH_PHRASE_SIZE != 0))
    {
        return DRV_FLASH_INVALID_ALIGNMENT;
    }

    /* Check if flash area is erased before writing */
    flash_address = (volatile U8 *)address_argu32;
    for (index_u32 = 0; index_u32 < size_argu32; index_u32++)
    {
        if (flash_address[index_u32] != DRV_FLASH_ERASED_VALUE)
        {
            /* Sector needs to be erased first */
            return DRV_FLASH_NOT_ERASED;
        }
    }

    /* Enter critical section */
    INT_SYS_DisableIRQGlobal();

    /* Perform flash programming */
    status = FLASH_DRV_Program(&DRV_flashSSDConfig_st, address_argu32, size_argu32, data_argptru8);

    /* Exit critical section */
    INT_SYS_EnableIRQGlobal();

    if (status != STATUS_SUCCESS)
    {
        return DRV_FLASH_WRITE_ERROR;
    }

    return DRV_FLASH_SUCCESS;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_FLASH_ReadBlock_gen
*   Description   : Reads data from flash memory
*   Parameters    : address_argu32 - Source address
*                   data_argptru8 - Pointer to destination buffer
*                   size_argu32 - Number of bytes to read
*   Return Value  : DRV_flashStatus_ten - Status of read operation
*  --------------------------------------------------------------------------- */
DRV_flashStatus_ten DRV_FLASH_ReadBlock_gen(U32 address_argu32, U8 *data_argptru8, U32 size_argu32)
{
    volatile U8 *flashAddress_ptru8;
    U32 index_u32;

    /* Check if flash is initialized */
    if (!DRV_flashInitStatus_mb)
    {
        return DRV_FLASH_NOT_INITIALIZED;
    }

    /* Validate parameters */
    if (data_argptru8 == NULL)
    {
        return DRV_FLASH_NULL_POINTER;
    }

    if (size_argu32 == 0)
    {
        return DRV_FLASH_INVALID_SIZE;
    }

    /* Check address range */
    if (!DRV_FLASH_IsAddressInRange_mb(address_argu32, size_argu32))
    {
        return DRV_FLASH_ADDRESS_OUT_OF_RANGE;
    }

    /* Read data from flash */
    flashAddress_ptru8 = (volatile U8 *)address_argu32;

    for (index_u32 = 0; index_u32 < size_argu32; index_u32++)
    {
        data_argptru8[index_u32] = flashAddress_ptru8[index_u32];
    }

    return DRV_FLASH_SUCCESS;
}

/* ==================== INTERRUPT HANDLERS ==================== */

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : CCIF_Handler
*   Description   : Command Complete Interrupt Handler
*   Parameters    : None
*   Return Value  : None
*  --------------------------------------------------------------------------- */
void CCIF_Handler(void)
{
    FTFx_FCNFG &= ~FTFx_FCNFG_CCIE_MASK;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : CCIF_Callback
*   Description   : Command Complete Callback Function (RAM section)
*   Parameters    : None
*   Return Value  : None
*  --------------------------------------------------------------------------- */
START_FUNCTION_DECLARATION_RAMSECTION
void CCIF_Callback(void)
END_FUNCTION_DECLARATION_RAMSECTION

START_FUNCTION_DEFINITION_RAMSECTION
void CCIF_Callback(void)
{
    if ((FTFx_FCNFG & FTFx_FCNFG_CCIE_MASK) == 0u)
    {
        FTFx_FCNFG |= FTFx_FCNFG_CCIE_MASK;
    }
}
END_FUNCTION_DEFINITION_RAMSECTION
