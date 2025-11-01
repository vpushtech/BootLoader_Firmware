/*
 * drv_timer.c
 *
 *  Description     : Timer Driver
 *  Author          : Rushikesh
 *  Created On      : 09-Jul-2025
 *  Version         : 2.0
 *  Modification History:
 *  Date        Author      Description
 *  ----------------------------------------------------------------------------
 *  08-Jul-2025 RUSHIKESH   Timer Driver Architecture Implementation
 *  25-Jul-2025 RUSHIKESH   Timer APIs are tested with Different Delay in ms/micro Sec and Callback function Testing Done
 *  11-Aug-2025 RUSHIKESH   Guidelines Followed the naming Architecture Implementation
 ******************************************************************************/
/* ==================== INCLUDE FILES ==================== */

#include "drv_timer.h"
#include"bsp.h"
#include"lpit1.h"
#include"lpit_driver.h"
#include"common_types.h"
#include"application.h"
#include"ftm_common.h"
/* ==================== STATIC VARIABLES ==================== */

/* ==================== GLOBAL VARIABLES ==================== */
extern DRV_TimerConfig_St_t DRV_TimerConfigTable_gst[MAX_TIMER_PIN];
extern BIN canTransmitEnable;
ftm_state_t DRV_ftmStateStruct[MAX_TIMER_PIN];

/* ==================== CONFIGURATION STRUCTURES ==================== */
const lpit_user_channel_config_t* DRV_timerChannelConfig[MAX_TIMER_PIN] = {
    &lpit1_ChnConfig0,
};

const lpit_user_config_t* DRV_timerConfig[MAX_TIMER_PIN] = {
    &lpit1_InitConfig,
};

/* ==================== INTERRUPT HANDLERS ==================== */
static void DRV_Timer_Ch0_IRQHandler(void);
static void DRV_Timer_Ch1_IRQHandler(void);
static void DRV_Timer_Ch2_IRQHandler(void);
static void DRV_Timer_Ch3_IRQHandler(void);

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_Timer_Init
*   Description   : Initializes timer module and channel
*   Parameters    : timerPinIdx - Timer pin index
*   Return Value  : void
*  --------------------------------------------------------------------------- */
void DRV_Timer_Init_gv(U8 timerPinIdx)
{
    LPIT_DRV_Init(DRV_TimerConfigTable_gst[timerPinIdx].timerInstance,
                 DRV_timerConfig[DRV_TimerConfigTable_gst[timerPinIdx].timerInstance]);

    LPIT_DRV_InitChannel(DRV_TimerConfigTable_gst[timerPinIdx].timerInstance,
                        DRV_TimerConfigTable_gst[timerPinIdx].timerChannel,
                        DRV_timerChannelConfig[DRV_TimerConfigTable_gst[timerPinIdx].timerInstance]);

    /* IRQ Handling */
    INT_SYS_InstallHandler(LPIT0_Ch0_IRQn, DRV_Timer_Ch0_IRQHandler, (void *)NULL);
    INT_SYS_InstallHandler(LPIT0_Ch1_IRQn, DRV_Timer_Ch1_IRQHandler, (void *)NULL);
    INT_SYS_InstallHandler(LPIT0_Ch2_IRQn, DRV_Timer_Ch2_IRQHandler, (void *)NULL);
    INT_SYS_InstallHandler(LPIT0_Ch3_IRQn, DRV_Timer_Ch3_IRQHandler, (void *)NULL);
}



/* ==================== TIMER CONTROL FUNCTIONS ==================== */
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_Timer_Start_gv
*   Description   : Starts timer channel
*   Parameters    : timerPinIdx - Timer pin index
*   Return Value  : void
*  --------------------------------------------------------------------------- */
void DRV_Timer_Start_gv(U8 timerPinIdx)
{
    LPIT_DRV_StartTimerChannels(DRV_TimerConfigTable_gst[timerPinIdx].timerInstance,
                               (uint32_t)DRV_TIMER_CHANNEL_MASK(DRV_TimerConfigTable_gst[timerPinIdx].timerChannel));
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_Timer_Stop_gv
*   Description   : Stops timer channel
*   Parameters    : timerPinIdx - Timer pin index
*   Return Value  : void
*  --------------------------------------------------------------------------- */
void DRV_Timer_Stop_gv(U8 timerPinIdx)
{
    LPIT_DRV_StopTimerChannels(DRV_TimerConfigTable_gst[timerPinIdx].timerInstance,
                              (uint32_t)DRV_TIMER_CHANNEL_MASK(DRV_TimerConfigTable_gst[timerPinIdx].timerChannel));
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_Timer_Delay
*   Description   : Creates precise delay using timer
*   Parameters    : timerPinIdx - Timer pin index
*                   delayUnit - Delay units (ms or us)
*                   delayValue - Delay duration
*   Return Value  : void
*  --------------------------------------------------------------------------- */
void DRV_Timer_Delay_gv(U8 timerPinIdx, DRV_TimerDelayUnit_En delayUnit, U32 delayValue)
{
    lpit_user_channel_config_t channelConfig;
    LPIT_DRV_GetDefaultChanConfig(&channelConfig);

    channelConfig.isInterruptEnabled = false;
    channelConfig.timerMode = LPIT_PERIODIC_COUNTER;
    channelConfig.periodUnits = LPIT_PERIOD_UNITS_MICROSECONDS;

    if(DRV_DELAY_UNITS_MILLISECOND == delayUnit)
    {
        channelConfig.period = delayValue * 1000U;
    }
    else
    {
        channelConfig.period = delayValue;
    }

    LPIT_DRV_InitChannel(DRV_TimerConfigTable_gst[timerPinIdx].timerInstance,
                        DRV_TimerConfigTable_gst[timerPinIdx].timerChannel,
                        &channelConfig);

    DRV_Timer_Start_gv(timerPinIdx);

    while (!(LPIT_DRV_GetInterruptFlagTimerChannels(DRV_TimerConfigTable_gst[timerPinIdx].timerInstance,
                                                  DRV_TIMER_CHANNEL_MASK(DRV_TimerConfigTable_gst[timerPinIdx].timerChannel))))
    {
        /* Busy Wait */
    }

    LPIT_DRV_ClearInterruptFlagTimerChannels(DRV_TimerConfigTable_gst[timerPinIdx].timerInstance,
                                           DRV_TIMER_CHANNEL_MASK(DRV_TimerConfigTable_gst[timerPinIdx].timerChannel));
    DRV_Timer_Stop_gv(timerPinIdx);
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_Timer_DeInit
*   Description   : Deinitializes timer module
*   Parameters    : timerPinIdx - Timer pin index
*   Return Value  : void
*  --------------------------------------------------------------------------- */
void DRV_Timer_DeInit_gv(U8 timerPinIdx)
{
    LPIT_DRV_Deinit(DRV_TimerConfigTable_gst[timerPinIdx].timerInstance);
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_Timer_InterruptConfig
*   Description   : Configures timer interrupt
*   Parameters    : timerPinIdx - Timer pin index
*                   irqIndex - Interrupt index
*                   priority - Interrupt priority
*                   interruptDelayMs - Interrupt delay in ms
*   Return Value  : status_t - Operation status
*  --------------------------------------------------------------------------- */
DRV_TimerStatus_En DRV_Timer_InterruptConfig_gst(U8 timerPinIdx, U8 irqIndex, U8 priority, U32 interruptDelayMs, DRV_TimerDelayUnit_En delayUnit_argen)
{
    lpit_user_channel_config_t channelConfig;
    LPIT_DRV_GetDefaultChanConfig(&channelConfig);

    channelConfig.isInterruptEnabled = true;
    channelConfig.timerMode = LPIT_PERIODIC_COUNTER;
    channelConfig.periodUnits = LPIT_PERIOD_UNITS_MICROSECONDS;
    if(DRV_DELAY_UNITS_MILLISECOND == delayUnit_argen)
    {
        channelConfig.period = interruptDelayMs * 1000U;
    }
    else
    {
        channelConfig.period = interruptDelayMs;
    }
    DRV_NVIC_Status_En status =DRV_NVIC_IRQConfig_gen(irqIndex, priority);
    if (status != DRV_NVIC_STATUS_OK)
    {
        return DRV_TIMER_STATUS_ERR;
    }

    LPIT_DRV_InitChannel(DRV_TimerConfigTable_gst[timerPinIdx].timerInstance,
                        DRV_TimerConfigTable_gst[timerPinIdx].timerChannel,
                        &channelConfig);

    return DRV_TIMER_STATUS_OK;
}

/* ==================== INTERRUPT SERVICE ROUTINES ==================== */
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_Timer_Ch0_IRQHandler
*   Description   : Timer channel 0 interrupt handler
*   Parameters    : None
*   Return Value  : void
*  --------------------------------------------------------------------------- */
static void DRV_Timer_Ch0_IRQHandler(void)
{
    if (LPIT_DRV_GetInterruptFlagTimerChannels(DRV_TIMER0, DRV_TIMER_CHANNEL_MASK(DRV_CHANNEL_0)))
    {
    	APP_Timer0_Callback();
        LPIT_DRV_ClearInterruptFlagTimerChannels(DRV_TIMER0, DRV_TIMER_CHANNEL_MASK(DRV_CHANNEL_0));
    }
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_Timer_Ch1_IRQHandler
*   Description   : Timer channel 1 interrupt handler (handles CAN transmit and WDG refresh)
*   Parameters    : None
*   Return Value  : void
*  --------------------------------------------------------------------------- */
static void DRV_Timer_Ch1_IRQHandler(void)
{
    if(LPIT_DRV_GetInterruptFlagTimerChannels(DRV_TIMER0, DRV_TIMER_CHANNEL_MASK(DRV_CHANNEL_1)))
    {
        LPIT_DRV_ClearInterruptFlagTimerChannels(DRV_TIMER0, DRV_TIMER_CHANNEL_MASK(DRV_CHANNEL_1));
    }
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_Timer_Ch2_IRQHandler
*   Description   : Timer channel 2 interrupt handler
*   Parameters    : None
*   Return Value  : void
*  --------------------------------------------------------------------------- */
static void DRV_Timer_Ch2_IRQHandler(void)
{
    if(LPIT_DRV_GetInterruptFlagTimerChannels(DRV_TIMER0, DRV_TIMER_CHANNEL_MASK(DRV_CHANNEL_2)))
    {
        LPIT_DRV_ClearInterruptFlagTimerChannels(DRV_TIMER0, DRV_TIMER_CHANNEL_MASK(DRV_CHANNEL_2));
    }
}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : DRV_Timer_Ch3_IRQHandler
*   Description   : Timer channel 3 interrupt handler
*   Parameters    : None
*   Return Value  : void
*  --------------------------------------------------------------------------- */
static void DRV_Timer_Ch3_IRQHandler(void)
{
    if(LPIT_DRV_GetInterruptFlagTimerChannels(DRV_TIMER0, DRV_TIMER_CHANNEL_MASK(DRV_CHANNEL_3)))
    {
        LPIT_DRV_ClearInterruptFlagTimerChannels(DRV_TIMER0, DRV_TIMER_CHANNEL_MASK(DRV_CHANNEL_3));
    }
}
