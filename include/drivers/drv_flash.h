#ifndef DRIVERS_DRV_FLASH_H_
#define DRIVERS_DRV_FLASH_H_

/* ==================== INCLUDE ==================== */
#include "common_types.h"
#include "flash_driver.h"
#include "bsp_config.h"
#include "device_registers.h"

/* ==================== DEFINE ==================== */
#define DRV_FLASH_TIMEOUT_MS               (1000U)
#define DRV_FLASH_SECTOR_SIZE              FEATURE_FLS_PF_BLOCK_SECTOR_SIZE
#define DRV_FLASH_PHRASE_SIZE              (8U)

/* Data Flash (FlexNVM) addresses */
#define DRV_FLASH_DF_BASE_ADDRESS          FEATURE_FLS_DF_START_ADDRESS
#define DRV_FLASH_DF_END_ADDRESS           (FEATURE_FLS_DF_START_ADDRESS + FEATURE_FLS_DF_BLOCK_SIZE)
#define DRV_FLASH_DF_SIZE                  FEATURE_FLS_DF_BLOCK_SIZE

/* Program Flash addresses */
#define DRV_FLASH_PF_BASE_ADDRESS          (0x00000000U)  /* P-Flash starts at address 0 */
#define DRV_FLASH_PF_END_ADDRESS           (FEATURE_FLS_PF_BLOCK_SIZE)
#define DRV_FLASH_PF_SIZE                  FEATURE_FLS_PF_BLOCK_SIZE

/* For backward compatibility - using Data Flash as default */
#define DRV_FLASH_BASE_ADDRESS             DRV_FLASH_DF_BASE_ADDRESS
#define DRV_FLASH_END_ADDRESS              DRV_FLASH_DF_END_ADDRESS
#define DRV_FLASH_MAX_SIZE                 DRV_FLASH_DF_SIZE

#define DRV_FLASH_ERASED_VALUE             (0xFFU)

/* Additional useful defines */
#define DRV_FLASH_DF_SECTOR_SIZE           FEATURE_FLS_DF_BLOCK_SECTOR_SIZE
#define DRV_FLASH_DF_WRITE_UNIT_SIZE       FEATURE_FLS_DF_BLOCK_WRITE_UNIT_SIZE
#define DRV_FLASH_PF_WRITE_UNIT_SIZE       FEATURE_FLS_PF_BLOCK_WRITE_UNIT_SIZE
#define DRV_FLASH_PF_SECTOR_COUNT          FEATURE_FLS_PF_BLOCK_COUNT
#define DRV_FLASH_DF_SECTOR_COUNT          FEATURE_FLS_DF_BLOCK_COUNT

/* ==================== ENUMS ==================== */
typedef enum
{
    DRV_FLASH_SUCCESS,
    DRV_FLASH_FAILED,
    DRV_FLASH_WRITE_ERROR,
    DRV_FLASH_ERASE_ERROR,
    DRV_FLASH_INVALID_ADDRESS,
    DRV_FLASH_INVALID_SIZE,
    DRV_FLASH_INVALID_ALIGNMENT,
    DRV_FLASH_NOT_ERASED,
    DRV_FLASH_ALREADY_INITIALIZED,
    DRV_FLASH_NOT_INITIALIZED,
    DRV_FLASH_ADDRESS_OUT_OF_RANGE,
    DRV_FLASH_NULL_POINTER
} DRV_flashStatus_ten;

/* ==================== GLOBAL VARIABLES ==================== */
extern volatile bool DRV_flashInitStatus;

/* ==================== INITIALIZATION/CONFIGURATION ==================== */
DRV_flashStatus_ten DRV_FLASH_Init_gen(void);
/* ==================== ERASE OPERATIONS ==================== */
DRV_flashStatus_ten DRV_FLASH_EraseSector_gen(U32 address_argu32, U32 size_argu32);

/* ==================== PROGRAM/READ OPERATIONS ==================== */
DRV_flashStatus_ten DRV_FLASH_WriteBlock_gen(U32 address_argu32, const U8 *data_argptru8, U32 size_argu32);
DRV_flashStatus_ten DRV_FLASH_ReadBlock_gen(U32 address_argu32, U8 *data_argptru8, U32 size_argu32);
/* ==================== INTERRUPT HANDLERS ==================== */
void CCIF_Handler(void);
void CCIF_Callback(void);

#endif /* DRIVERS_DRV_FLASH_H_ */
