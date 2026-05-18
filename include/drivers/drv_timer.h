/*******************************************************************************
 *  Description     : Timer Driver Header
 *  Author          : Rushikesh
 *  Created On      : 09-Jul-2025
 *  Version         : 2.0
 *  Modification History:
 *  Date        Author      Description
 *  ----------------------------------------------------------------------------
 *  08-Jul-2025 RUSHIKESH   Timer Driver Architecture Implementation
 *  25-Jul-2025 RUSHIKESH   Timer APIs tested with different delay in ms/us
 *  11-Aug-2025 RUSHIKESH   Naming Architecture Implementation
 *  23-Jan-2026 RUSHIKESH   Removed PWM APIs, restructured timer drivers
 *  29-Jan-2026 RUSHIKESH   Added validation, error handling, tracking
 *  01-May-2026 RUSHIKESH   MISRA C 2012 Compliance
 */

#ifndef DRV_TIMER_H
#define DRV_TIMER_H

/* ==================== INCLUDE FILES ==================== */
#include "common_types.h"
#include "lpit_driver.h"
#include "lpit1.h"
#include "drv_nvic.h"

/* ==================== MACROS ==================== */
#define DRV_TIMER_CHANNEL_MASK(channel)    ( (U32)1U << (U32)(channel) )
#define DRV_TIMER_MAX_DELAY_VALUE_MS       (4294967U)
#define DRV_TIMER_MAX_DELAY_VALUE_US       (4294967295U)

/* ==================== TYPE DEFINITIONS ==================== */
typedef enum
{
    DRV_TIMER0    = 0,
    DRV_TIMER1    = 1,
    DRV_MAX_TIMER = 3
} DRV_TimerInstance_ten;
typedef enum
{
    DRV_CHANNEL_0   = 0,
    DRV_CHANNEL_1   = 1,
    DRV_MAX_CHANNEL = 3
} DRV_TimerChannel_ten;
typedef enum
{
    DRV_DELAY_UNITS_MILLISECOND = 0,
    DRV_DELAY_UNITS_MICROSECOND = 1
} DRV_TimerDelayUnit_ten;
typedef enum
{
    DRV_TIMER_STATUS_OK                  = 0,
    DRV_TIMER_STATUS_ERR                 = 1,
    DRV_TIMER_STATUS_INVALID_INSTANCE    = 2,
    DRV_TIMER_STATUS_INVALID_CHANNEL     = 3,
    DRV_TIMER_STATUS_INVALID_DELAY       = 4,
    DRV_TIMER_STATUS_ZERO_DELAY          = 5,
    DRV_TIMER_STATUS_NVIC_ERROR          = 6,
    DRV_TIMER_STATUS_ALREADY_INITIALIZED = 7,
    DRV_TIMER_STATUS_NOT_INITIALIZED     = 8
} DRV_TimerStatus_ten;

/* ==================== GLOBAL VARIABLE DECLARATIONS ==================== */
extern volatile BIN DRV_timerInitStatus_mb[DRV_MAX_TIMER];
extern volatile BIN DRV_timerChannelInitStatus_mb[DRV_MAX_TIMER][DRV_MAX_CHANNEL];

/* ==================== FUNCTION PROTOTYPES ==================== */
/* ------ Timer Instance Control ------ */
extern DRV_TimerStatus_ten DRV_Timer_Init_gst(
    DRV_TimerInstance_ten timerInstance_argen);

extern DRV_TimerStatus_ten DRV_Timer_DeInit_gst(
    DRV_TimerInstance_ten timerInstance_argen);

/* ------ Timer Channel Control ------ */
extern DRV_TimerStatus_ten DRV_Timer_ChannelInit_gst(
    DRV_TimerInstance_ten timerInstance_argen,
    DRV_TimerChannel_ten  timerChannel_argen);

extern DRV_TimerStatus_ten DRV_Timer_StartChannel_gst(
    DRV_TimerInstance_ten timerInstance_argen,
    DRV_TimerChannel_ten  timerChannel_argen);

extern DRV_TimerStatus_ten DRV_Timer_StopChannel_gst(
    DRV_TimerInstance_ten timerInstance_argen,
    DRV_TimerChannel_ten  timerChannel_argen);

/* ------ Timing Operations ------ */
extern DRV_TimerStatus_ten DRV_Timer_Delay_gst(
    DRV_TimerInstance_ten  timerInstance_argen,
    DRV_TimerChannel_ten   timerChannel_argen,
    DRV_TimerDelayUnit_ten delayUnit_argen,
    U32                    delayValue_argu32);

/* ------ Interrupt Configuration ------ */
extern DRV_TimerStatus_ten DRV_Timer_InterruptConfig_gst(
    DRV_TimerInstance_ten  timerInstance_argen,
    DRV_TimerChannel_ten   timerChannel_argen,
    U8                     irqIndex_argu8,
    U8                     priority_argu8,
    U32                    interruptDelayMs_argu32,
    DRV_TimerDelayUnit_ten delayUnit_argen);

#endif /* DRV_TIMER_H */
