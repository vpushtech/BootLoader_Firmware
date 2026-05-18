/*
 ******************************************************************************
 * @file         drv_wdg.h
 * @brief        WDG Driver Header
 * @author       Rushikesh
 * @date         17-Jul-2025
 * @version      2.0
 *
 * Modification History:
 * Date        Author      Description
 * ----------------------------------------------------------------------------
 * 17-Jul-2025 RUSHIKESH   WDG Driver Architecture Implementation
 * 05-Aug-2025 RUSHIKESH   WDG Refresh and WDG Interrupt Testing Done
 * 11-Aug-2025 RUSHIKESH   Guidelines Followed - Naming Architecture Implementation
 * 26-Jan-2026 RUSHIKESH   Added validation, init tracking, and error handling
 * 01-May-2026 RUSHIKESH   Applied MISRA C:2012 compliance
 ******************************************************************************
 */

#ifndef DRIVERS_DRV_WDG_H
#define DRIVERS_DRV_WDG_H

/* ==================== HEADER INCLUDES ==================== */

#include "wdg_pal1.h"
#include "drv_nvic.h"
#include "common_types.h"

/* ==================== TYPE DEFINITIONS ==================== */
typedef enum
{
    DRV_WDG_INSTANCE_1    = 0,
    DRV_MAX_WDG_INSTANCE  = 1
} DRV_WdgInstance_ten;

/**
 * @brief  Enumeration of WDG driver return status codes.
 */
typedef enum
{
    DRV_WDG_STATUS_OK                 = 0,
    DRV_WDG_STATUS_ERR                = 1,
    DRV_WDG_STATUS_INVALID_INSTANCE   = 2,
    DRV_WDG_STATUS_ALREADY_INITIALIZED = 3,
    DRV_WDG_STATUS_NOT_INITIALIZED    = 4
} DRV_WdgStatus_ten;

/* ==================== GLOBAL VARIABLES ==================== */
extern volatile BIN DRV_wdgInitStatus_mb[DRV_MAX_WDG_INSTANCE];

/* ==================== FUNCTION DECLARATIONS ==================== */
void DRV_WDG_ISRHandler_v(void);

DRV_WdgStatus_ten DRV_WDG_Init_gen(DRV_WdgInstance_ten wdgInstance_argen);
DRV_WdgStatus_ten DRV_WDG_DeInit_gen(DRV_WdgInstance_ten wdgInstance_argen);
DRV_WdgStatus_ten DRV_WDG_Refresh_gen(DRV_WdgInstance_ten wdgInstance_argen);
DRV_WdgStatus_ten DRV_WDG_GetStatus_gen(DRV_WdgInstance_ten wdgInstance_argen,
                                         BIN * const isRunning_argb);
#endif /* DRIVERS_DRV_WDG_H */
