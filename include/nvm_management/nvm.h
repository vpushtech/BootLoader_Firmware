/*
 * nvm.h
 *
 *  Description     : Non-Volatile Memory Manager
 *                    Manages all BMS variables stored in D-Flash (FlexNVM).
 *                    Three storage strategies are used:
 *                      1. SoC + SoH        : Dual-block sequential wear-levelling.
 *                      2. BootFlag +
 *                         AppStatus        : Single-block sequential wear-levelling.
 *                      3. U32 Shared Block : Fixed-slot, erase-per-write.
 *  Author          : Rushikesh
 *  Created On      : 10-Mar-2026
 *  Version         : 2.3
 *  Modification History:
 *  Date        Author      Description
 *  ----------------------------------------------------------------------------
 *  10-Mar-2026 RUSHIKESH   Initial implementation – SoC/SoH dual-block layout
 *  20-Mar-2026 RUSHIKESH   Added U32 shared slot block
 *  01-Apr-2026 RUSHIKESH   Added BootFlag + AppStatus single-block region
 *  15-Apr-2026 RUSHIKESH   Removed BootFlag/AppStatus from U32 shared block
 *  01-May-2026 RUSHIKESH   MISRA C:2012 compliance, version 2.3
 ******************************************************************************/

#ifndef BMS_APPLICATION_NVM_H_
#define BMS_APPLICATION_NVM_H_

/* ==================== INCLUDE FILES ==================== */

#include "drv_flash.h"
#include "common_types.h"

/* ==================== D-FLASH ADDRESS MAP ==================== */

/*
 * Base : DRV_FLASH_DF_BASE_ADDRESS
 *
 *  Offset   Size    Region
 *  ------   ------  -------------------------------------------------------
 *  0x0000   2 KB    BMS Configuration          - RESERVED (do not modify)
 *  0x0800   2 KB    SoC + SoH Block A          - Wear-levelled primary
 *  0x1000   2 KB    SoC + SoH Block B          - Wear-levelled secondary
 *  0x1800   2 KB    BootFlag + AppStatus Block  - Single-block sequential
 *  0x2000   2 KB    U32 Shared Block            - Fixed 8-byte slot per variable
 *  ------   ------  -------------------------------------------------------
 *  Total    12 KB   of 64 KB D-Flash available
 */

#define NVM_DF_SECTOR_SIZE          (DRV_FLASH_DF_SECTOR_SIZE)        /* 0x0800 = 2 KB */
#define NVM_DF_BASE_ADDR            (DRV_FLASH_DF_BASE_ADDRESS)

#define NVM_BMS_CFG_ADDR            (NVM_DF_BASE_ADDR + 0x0000UL)
#define NVM_BMS_CFG_SIZE            (NVM_DF_SECTOR_SIZE)

#define NVM_SOC_SOH_BLK1_ADDR      (NVM_BMS_CFG_ADDR + NVM_BMS_CFG_SIZE)
#define NVM_SOC_SOH_BLK1_SIZE      (NVM_DF_SECTOR_SIZE)
#define NVM_SOC_SOH_BLK2_ADDR      (NVM_SOC_SOH_BLK1_ADDR + NVM_SOC_SOH_BLK1_SIZE)
#define NVM_SOC_SOH_BLK2_SIZE      (NVM_DF_SECTOR_SIZE)

#define NVM_FLAG_BLK_ADDR          (NVM_SOC_SOH_BLK2_ADDR + NVM_SOC_SOH_BLK2_SIZE)
#define NVM_FLAG_BLK_SIZE          (NVM_DF_SECTOR_SIZE)

#define NVM_U32_SHARED_ADDR        (NVM_FLAG_BLK_ADDR + NVM_FLAG_BLK_SIZE)
#define NVM_U32_SHARED_SIZE        (NVM_DF_SECTOR_SIZE)

/* ==================== RECORD CONSTANTS ==================== */

#define NVM_RECORD_SIZE             (8U)

#define NVM_SLOT_ACCUM_CHG_CAP      (0U)
#define NVM_SLOT_ACCUM_CHG_ENRG     (1U)
#define NVM_SLOT_BAT_CYCLE          (2U)
#define NVM_SLOT_ACCUM_DIS_CAP      (3U)
#define NVM_SLOT_ACCUM_DIS_ENRG     (4U)
#define NVM_SLOT_PACK_CAPACITY      (5U)
#define NVM_SLOT_SD_CARD_SECTOR     (6U)
#define NVM_SLOT_MCU_RESET          (7U)
#define NVM_U32_SLOT_COUNT          (8U)

/* ==================== SOC/SOH SCALING ==================== */

#define NVM_VALUE_CHANGE_THRESHOLD  (1.0)
#define NVM_VALUE_SCALE_FACTOR      (100.0)
#define NVM_DEFAULT_SOC_SOH         (0.0)

/* ==================== ENUM DEFINITIONS ==================== */

typedef enum
{
    NVM_STATUS_OK            = 0,
    NVM_STATUS_ERROR         = 1,
    NVM_STATUS_ERASE_FAILED  = 2,
    NVM_STATUS_WRITE_FAILED  = 3,
    NVM_STATUS_READ_FAILED   = 4,
    NVM_STATUS_INVALID_PARAM = 5
} NvmStatus_ten;

/* ==================== STRUCTURE DEFINITIONS ==================== */

typedef struct
{
    U32    block1Addr_u32;
    U32    block1Size_u32;
    U32    block2Addr_u32;
    U32    block2Size_u32;
    U32    currentWriteAddr_u32;
    U32    currentBlockEnd_u32;
    double prevValueA_d;
    double prevValueB_d;
} NvmBlockControl_tst;

typedef struct
{
    double APP_SOCpercent_d;
    double APP_SOHpercent_d;
    U32    APP_AccumChargeCap_u32;
    U32    APP_AccumChargeEnergy_u32;
    U32    APP_BatteryCycle_u32;
    U32    APP_AccumDischargeCap_u32;
    U32    APP_AccumDischargeEnergy_u32;
    U32    APP_bootFlag_u16;
    U32    APP_appStatus_u16;
    U32    APP_PackCapacity_u32;
    U8     McuReset;
    U32    CurrentSectorAddr_u32;
} NVM_FlshStoredData_tst;

/* ==================== GLOBAL VARIABLES ==================== */

extern NVM_FlshStoredData_tst NVM_FlshStoredData_st;

/* ==================== INITIALISATION ==================== */

extern void NVM_Init_gv(void);

/* ==================== SOC + SOH ==================== */

extern double        NVM_SoC_Read(void);
extern double        NVM_SoH_Read(void);
extern NvmStatus_ten NVM_SoCSoH_Update(double soc_d, double soh_d);

/* ==================== BOOTFLAG + APPSTATUS ==================== */

extern U16           NVM_BootFlag_Read(void);
extern U16           NVM_AppStatus_Read(void);
extern NvmStatus_ten NVM_BootFlagAppStatus_Update(U16 bootFlag_u16, U16 appStatus_u16);

/* ==================== U32 SHARED BLOCK : READ ==================== */

extern U32 NVM_AccumChargeCap_Read(void);
extern U32 NVM_AccumChargeEnergy_Read(void);
extern U32 NVM_BatteryCycle_Read(void);
extern U32 NVM_AccumDischargeCap_Read(void);
extern U32 NVM_AccumDischargeEnergy_Read(void);
extern U32 NVM_PackCapacity_Read(void);
extern U32 NVM_SdCardCurrentSector_Read(void);
extern U32 NVM_McuReset_Read(void);

/* ==================== U32 SHARED BLOCK : UPDATE ==================== */

extern NvmStatus_ten NVM_AccumChargeCap_Update(U32 value_u32);
extern NvmStatus_ten NVM_AccumChargeEnergy_Update(U32 value_u32);
extern NvmStatus_ten NVM_BatteryCycle_Update(U32 value_u32);
extern NvmStatus_ten NVM_AccumDischargeCap_Update(U32 value_u32);
extern NvmStatus_ten NVM_AccumDischargeEnergy_Update(U32 value_u32);
extern NvmStatus_ten NVM_PackCapacity_Update(U32 value_u32);
extern NvmStatus_ten NVM_SdCardCurrentSector_Update(U32 value_u32);
extern NvmStatus_ten NVM_McuReset_Update(U32 value_u32);

/* ==================== CRC UTILITY ==================== */

extern U16 NVM_CalculateCrc16_gen(const U8 *data_argpcu8, U32 length_argu32);

#endif /* BMS_APPLICATION_NVM_H_ */
