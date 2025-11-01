/*
 * BL_flash.h
 *
 *  S32K144 – 512 KB P-Flash, 4 KB sectors, 8-byte phrase programming
 *  Bootloader: 0x0000_0000 … 0x0000_7FFF   (32 KB)
 *  Application: 0x0000_8000 … 0x0007_FFFF   (480 KB)
 *  FCF (must stay untouched): 0x400 … 0x40F
 */

#ifndef BL_FLASH_H_
#define BL_FLASH_H_

#include "stdint.h"
#include "flash_driver.h"
#include "clockMan1.h"
#include "drv_nvic.h"

typedef enum {
    FLASH_OK = 0,
    FLASH_ERROR,
    FLASH_WRITE_ERROR,
    FLASH_ERASE_ERROR,
    FLASH_ERROR_UNLOCK,
    FLASH_ERROR_INVALID_ADDRESS,
} flash_status;

#define PFLASH_START                0x00000000U
#define PFLASH_SIZE                 (512U * 1024U)
#define PFLASH_END                  (PFLASH_START + PFLASH_SIZE - 1U)

#define SECTOR_SIZE                 FEATURE_FLS_PF_BLOCK_SECTOR_SIZE   /* 4 KB */

#define BOOT_START_ADDRESS          0x00000000U
#define BOOT_SIZE                   (32U * 1024U)
#define BOOT_END_ADDRESS            (BOOT_START_ADDRESS + BOOT_SIZE - 1U)

#define APP_START_ADDRESS           0x00008000U
#define APP_SIZE                    (480U * 1024U)
#define APP_END_ADDRESS             0x0007EFFF

#define META_START_ADDRESS          0x0007F000U
#define META_SIZE                   (4U * 1024U)
#define META_END_ADDRESS            (META_START_ADDRESS + META_SIZE - 1U)

flash_status Flash_init(void);
flash_status Flash_Sector_Erase(uint32_t address, uint32_t size);
flash_status Flash_App_Erase(void);
flash_status Flash_Write_Byte(uint32_t address, uint8_t *buffer, uint32_t size);
flash_status Flash_Read(uint32_t address, uint8_t *buffer, uint32_t size);

void CCIF_Handler(void);
void CCIF_Callback(void);

#endif /* BL_FLASH_H_ */
