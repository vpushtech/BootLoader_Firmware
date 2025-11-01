#ifndef DRIVERS_DRV_WDG_H_
#define DRIVERS_DRV_WDG_H_

/* ==================== HEADER INCLUDES ==================== */
#include "wdg_pal1.h"
#include "drv_nvic.h"

/* ==================== TYPE DEFINITIONS ==================== */
typedef enum {
    DRV_WDG_INSTANCE_1,
    DRV_WDG_INSTANCE_2,
    DRV_MAX_WDG_INSTANCE
} DRV_WdgInstance_En;

typedef enum {
    DRV_WDG_STATUS_OK,
    DRV_WDG_STATUS_ERR
} DRV_WdgStatus_En;

/* ==================== WATCHDOG FUNCTIONS ==================== */
void DRV_WDG_ISRHandler_v(void);
DRV_WdgStatus_En DRV_WDG_Init_gen(DRV_WdgInstance_En wdgInstance_argen);
void DRV_WDG_Refresh_gen(DRV_WdgInstance_En wdgInstance_argen);



#endif /* DRIVERS_DRV_WDG_H_ */
