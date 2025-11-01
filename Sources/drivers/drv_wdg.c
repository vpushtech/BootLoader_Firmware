/*
 * wdg_wdg.c
 *
 *  Description     : WDG Driver
 *  Author          : Rushikesh
 *  Created On      : 17-Jul-2025
 *  Version         : 2.0
 *  Modification History:
 *  Date        Author      Description
 *  ----------------------------------------------------------------------------
 *  17-Jul-2025 RUSHIKESH   WDG Driver Architecture Imlimentation
 *  05-Aug-2025 RUSHIKESH   wdg Refresh and wdg Interrupt Testing Done
 *  11-Aug-2025 RUSHIKESH   Guidelines Followed the naming Architecture Implementation
 ******************************************************************************/
/* ==================== INCLUDE FILES ==================== */
#include "drv_wdg.h"

/* ==================== STATIC VARIABLES ==================== */
static const wdg_instance_t* DRV_WdgInstances_arrst[DRV_MAX_WDG_INSTANCE] = {
    &wdg_pal1_Instance,
};

static const wdg_config_t* DRV_WdgConfig_arrst[DRV_MAX_WDG_INSTANCE] = {
    &wdg_pal1_Config0
};

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : WDG_ISRHandler_v
*   Description   : Watchdog Timer Interrupt Service Routine. Clears the interrupt
*                   flag for WDG1 instance when triggered.
*   Parameters    : None
*   Return Value  : void
*  ---------------------------------------------------------------------------*/
void DRV_WDG_ISRHandler_v(void)
{
    WDG_ClearIntFlag(&wdg_pal1_Instance);
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : WDG_Init_gen
*   Description   : Initializes the specified Watchdog Timer instance and sets up
*                   the interrupt handler. Must be called before using WDG.
*   Parameters    : wdgInstance_argen - Watchdog instance to initialize (WDG_INSTANCE_1/2)
*   Return Value  : wdgStatus_En - DRV_WDG_STATUS_OK if successful, DRV_WDG_STATUS_ERR on failure
*  ---------------------------------------------------------------------------*/
DRV_WdgStatus_En DRV_WDG_Init_gen(DRV_WdgInstance_En wdgInstance_argen)
{
    status_t status = WDG_Init(DRV_WdgInstances_arrst[wdgInstance_argen], DRV_WdgConfig_arrst[wdgInstance_argen]);
    if(status == STATUS_SUCCESS)
    {
        INT_SYS_InstallHandler(WDOG_EWM_IRQn, DRV_WDG_ISRHandler_v, (isr_t *)0);
        return DRV_WDG_STATUS_OK;
    }
    return DRV_WDG_STATUS_ERR;
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_WDG_Refresh_gen
*   Description   : Refreshes the watchdog timer counter to prevent system reset.
*                   Must be called periodically before the watchdog timeout period.
*   Parameters    : wdgInstance_argen - Watchdog instance to refresh (WDG_INSTANCE_1/2)
*   Return Value  : void
*  ---------------------------------------------------------------------------*/
void DRV_WDG_Refresh_gen(DRV_WdgInstance_En wdgInstance_argen)
{
    WDG_Refresh(DRV_WdgInstances_arrst[wdgInstance_argen]);
}
