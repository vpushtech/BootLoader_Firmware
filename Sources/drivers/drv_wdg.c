/*
 ******************************************************************************
 * @file         drv_wdg.c
 * @brief        WDG Driver Implementation
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

/* ==================== INCLUDE FILES ==================== */
#include "drv_wdg.h"

/* ==================== STATIC VARIABLES ==================== */
static const wdg_instance_t * const DRV_WdgInstances_arrst[DRV_MAX_WDG_INSTANCE] =
{
    &wdg_pal1_Instance
};
static const wdg_config_t * const DRV_WdgConfig_arrst[DRV_MAX_WDG_INSTANCE] =
{
    &wdg_pal1_Config0
};

/* ==================== GLOBAL VARIABLES ==================== */
volatile BIN DRV_wdgInitStatus_mb[DRV_MAX_WDG_INSTANCE] =
{
    (BIN)false,
};

/* ==================== FUNCTION DEFINITIONS ==================== */

/* -----------------------------------------------------------------------------
 *  Function   : DRV_WDG_ISRHandler_v
 *  Description: Watchdog Timer Interrupt Service Routine.
 *               Clears the interrupt flag for WDG instance 1.
 *  Parameters : None
 *  Returns    : void
 * ---------------------------------------------------------------------------*/
void DRV_WDG_ISRHandler_v(void)
{
    WDG_ClearIntFlag(&wdg_pal1_Instance);
}

/* -----------------------------------------------------------------------------
 *  Function   : DRV_WDG_Init_gen
 *  Description: Initialises the specified WDG instance and registers the ISR.
 *               Must be called before DRV_WDG_Refresh_gen.
 *  Parameters : wdgInstance_argen – instance to initialise (enum value)
 *  Returns    : DRV_WDG_STATUS_OK on success; appropriate error code otherwise
 * ---------------------------------------------------------------------------*/
DRV_WdgStatus_ten DRV_WDG_Init_gen(DRV_WdgInstance_ten wdgInstance_argen)
{
    DRV_WdgStatus_ten retStatus_en;
    status_t          palStatus_en;
    if (wdgInstance_argen >= DRV_MAX_WDG_INSTANCE)
    {
        retStatus_en = DRV_WDG_STATUS_INVALID_INSTANCE;
    }
    else if ((bool)DRV_wdgInitStatus_mb[wdgInstance_argen] == true)
    {
        retStatus_en = DRV_WDG_STATUS_ALREADY_INITIALIZED;
    }
    else
    {
        palStatus_en = WDG_Init(DRV_WdgInstances_arrst[wdgInstance_argen],
                                DRV_WdgConfig_arrst[wdgInstance_argen]);

        if (palStatus_en == STATUS_SUCCESS)
        {
            INT_SYS_InstallHandler(WDOG_EWM_IRQn,
                                   DRV_WDG_ISRHandler_v,
                                   (isr_t *)0);

            DRV_wdgInitStatus_mb[wdgInstance_argen] = (BIN)true;
            retStatus_en = DRV_WDG_STATUS_OK;
        }
        else
        {
            retStatus_en = DRV_WDG_STATUS_ERR;
        }
    }

    return retStatus_en;
}

/* -----------------------------------------------------------------------------
 *  Function   : DRV_WDG_DeInit_gen
 *  Description: Deinitialises the specified WDG instance.
 *  Parameters : wdgInstance_argen – instance to deinitialise
 *  Returns    : DRV_WDG_STATUS_OK on success; appropriate error code otherwise
 * ---------------------------------------------------------------------------*/
DRV_WdgStatus_ten DRV_WDG_DeInit_gen(DRV_WdgInstance_ten wdgInstance_argen)
{
    DRV_WdgStatus_ten retStatus_en;
    status_t          palStatus_en;
    if (wdgInstance_argen >= DRV_MAX_WDG_INSTANCE)
    {
        retStatus_en = DRV_WDG_STATUS_INVALID_INSTANCE;
    }
    else if ((bool)DRV_wdgInitStatus_mb[wdgInstance_argen] == false)
    {
        retStatus_en = DRV_WDG_STATUS_NOT_INITIALIZED;
    }
    else
    {
        palStatus_en = WDG_Deinit(DRV_WdgInstances_arrst[wdgInstance_argen]);
        DRV_wdgInitStatus_mb[wdgInstance_argen] = (BIN)false;

        if (palStatus_en == STATUS_SUCCESS)
        {
            retStatus_en = DRV_WDG_STATUS_OK;
        }
        else
        {
            retStatus_en = DRV_WDG_STATUS_ERR;
        }
    }

    return retStatus_en; /* MISRA C:2012 Rule 15.5 – single exit point */
}

/* -----------------------------------------------------------------------------
 *  Function   : DRV_WDG_Refresh_gen
 *  Description: Refreshes (kicks) the WDG counter to prevent system reset.
 *               Must be called periodically within the watchdog timeout period.
 *  Parameters : wdgInstance_argen – instance to refresh
 *  Returns    : DRV_WDG_STATUS_OK on success; appropriate error code otherwise
 * ---------------------------------------------------------------------------*/
DRV_WdgStatus_ten DRV_WDG_Refresh_gen(DRV_WdgInstance_ten wdgInstance_argen)
{
    DRV_WdgStatus_ten retStatus_en;
    if (wdgInstance_argen >= DRV_MAX_WDG_INSTANCE)
    {
        retStatus_en = DRV_WDG_STATUS_INVALID_INSTANCE;
    }
    else if ((bool)DRV_wdgInitStatus_mb[wdgInstance_argen] == false)
    {
        retStatus_en = DRV_WDG_STATUS_NOT_INITIALIZED;
    }
    else
    {
        WDG_Refresh(DRV_WdgInstances_arrst[wdgInstance_argen]);
        retStatus_en = DRV_WDG_STATUS_OK;
    }

    return retStatus_en;
}

/* -----------------------------------------------------------------------------
 *  Function   : DRV_WDG_GetStatus_gen
 *  Description: Retrieves the initialised / running status of a WDG instance.
 *  Parameters : wdgInstance_argen – instance to query
 *               isRunning_argb    – non-NULL output pointer; set to true if
 *                                   the instance is initialised
 *  Returns    : DRV_WDG_STATUS_OK on success; appropriate error code otherwise
 * ---------------------------------------------------------------------------*/
DRV_WdgStatus_ten DRV_WDG_GetStatus_gen(DRV_WdgInstance_ten wdgInstance_argen,
                                         BIN * const isRunning_argb)
{
    DRV_WdgStatus_ten retStatus_en;
    if (wdgInstance_argen >= DRV_MAX_WDG_INSTANCE)
    {
        retStatus_en = DRV_WDG_STATUS_INVALID_INSTANCE;
    }
    else if (isRunning_argb == NULL)
    {
        retStatus_en = DRV_WDG_STATUS_ERR;
    }
    else
    {
        *isRunning_argb = DRV_wdgInitStatus_mb[wdgInstance_argen];
        retStatus_en    = DRV_WDG_STATUS_OK;
    }

    return retStatus_en;
}

