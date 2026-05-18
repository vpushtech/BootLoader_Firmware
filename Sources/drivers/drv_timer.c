/*******************************************************************************
 *  Description     : Timer Driver
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

/* ==================== INCLUDE FILES ==================== */
#include "bsp_config.h"
#include "drv_timer.h"

/* ==================== GLOBAL VARIABLES ==================== */

volatile BIN DRV_timerInitStatus_mb[DRV_MAX_TIMER]                          = {(BIN)0};
volatile BIN DRV_timerChannelInitStatus_mb[DRV_MAX_TIMER][DRV_MAX_CHANNEL]  = {{(BIN)0}};

/* ==================== CONFIGURATION STRUCTURES ==================== */
static const lpit_user_channel_config_t * const DRV_timerChannelConfig_mst[DRV_MAX_CHANNEL] =
{
    &lpit1_ChnConfig0,
    &lpit1_ChnConfig1,
};

static const lpit_user_config_t * const DRV_timerConfig_mst[DRV_MAX_TIMER] =
{
    &lpit1_InitConfig,
    &lpit1_InitConfig,
};

/* ==================== IRQ CONFIGURATION ARRAYS ==================== */
static const IRQn_Type DRV_timerIRQnArray_mst[DRV_MAX_CHANNEL] =
{
    LPIT0_Ch0_IRQn,
    LPIT0_Ch1_IRQn,
};
void APP_Timer_Ch0_IRQHandler();
void APP_Timer_Ch1_IRQHandler();
static void (*DRV_timerIRQHandlerArray_mst[DRV_MAX_CHANNEL])(void) =
{
    APP_Timer_Ch0_IRQHandler,
    APP_Timer_Ch1_IRQHandler,
};

/* ==================== PRIVATE HELPER FUNCTION PROTOTYPES ==================== */
static DRV_TimerStatus_ten DRV_Timer_ValidateInstance_prv(
    DRV_TimerInstance_ten timerInstance_argen);

static DRV_TimerStatus_ten DRV_Timer_ValidateInstanceAndChannel_prv(
    DRV_TimerInstance_ten timerInstance_argen,
    DRV_TimerChannel_ten  timerChannel_argen);

static DRV_TimerStatus_ten DRV_Timer_ValidateDelayParams_prv(
    DRV_TimerDelayUnit_ten delayUnit_argen,
    U32                    delayValue_argu32);

static U32 DRV_Timer_ConvertToMicroseconds_prv(
    DRV_TimerDelayUnit_ten delayUnit_argen,
    U32                    delayValue_argu32);

static DRV_TimerStatus_ten DRV_Timer_AutoInit_prv(
    DRV_TimerInstance_ten timerInstance_argen);

/* ==================== PRIVATE HELPER FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_ValidateInstance_prv
 *  Description   : Validates that a timer instance index is within range.
 *  Parameters    : timerInstance_argen - Instance to validate
 *  Return Value  : DRV_TIMER_STATUS_OK               - Valid instance
 *                  DRV_TIMER_STATUS_INVALID_INSTANCE  - Out of range
 * --------------------------------------------------------------------------- */
static DRV_TimerStatus_ten DRV_Timer_ValidateInstance_prv(
    DRV_TimerInstance_ten timerInstance_argen)
{
    DRV_TimerStatus_ten retStatus;
    if (timerInstance_argen >= DRV_MAX_TIMER)
    {
        retStatus = DRV_TIMER_STATUS_INVALID_INSTANCE;
    }
    else
    {
        retStatus = DRV_TIMER_STATUS_OK;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_ValidateInstanceAndChannel_prv
 *  Description   : Validates both instance and channel indices are in range.
 *  Parameters    : timerInstance_argen - Instance to validate
 *                  timerChannel_argen  - Channel to validate
 *  Return Value  : DRV_TIMER_STATUS_OK               - Both valid
 *                  DRV_TIMER_STATUS_INVALID_INSTANCE  - Instance out of range
 *                  DRV_TIMER_STATUS_INVALID_CHANNEL   - Channel out of range
 * --------------------------------------------------------------------------- */
static DRV_TimerStatus_ten DRV_Timer_ValidateInstanceAndChannel_prv(
    DRV_TimerInstance_ten timerInstance_argen,
    DRV_TimerChannel_ten  timerChannel_argen)
{
    DRV_TimerStatus_ten retStatus;

    if (timerInstance_argen >= DRV_MAX_TIMER)
    {
        retStatus = DRV_TIMER_STATUS_INVALID_INSTANCE;
    }
    else if (timerChannel_argen >= DRV_MAX_CHANNEL)
    {
        retStatus = DRV_TIMER_STATUS_INVALID_CHANNEL;
    }
    else
    {
        retStatus = DRV_TIMER_STATUS_OK;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_ValidateDelayParams_prv
 *  Description   : Validates delay unit and that delay value is non-zero and
 *                  within the hardware-supported maximum.
 *  Parameters    : delayUnit_argen    - Delay time base (ms or us)
 *                  delayValue_argu32  - Delay magnitude
 *  Return Value  : DRV_TIMER_STATUS_OK             - Parameters valid
 *                  DRV_TIMER_STATUS_ZERO_DELAY      - delayValue_argu32 == 0
 *                  DRV_TIMER_STATUS_INVALID_DELAY   - Exceeds hardware maximum
 *                  DRV_TIMER_STATUS_ERR             - Unknown delay unit
 * --------------------------------------------------------------------------- */
static DRV_TimerStatus_ten DRV_Timer_ValidateDelayParams_prv(
    DRV_TimerDelayUnit_ten delayUnit_argen,
    U32                    delayValue_argu32)
{
    DRV_TimerStatus_ten retStatus;

    if (delayValue_argu32 == 0U)
    {
        retStatus = DRV_TIMER_STATUS_ZERO_DELAY;
    }
    else if (delayUnit_argen == DRV_DELAY_UNITS_MILLISECOND)
    {
        if (delayValue_argu32 > DRV_TIMER_MAX_DELAY_VALUE_MS)
        {
            retStatus = DRV_TIMER_STATUS_INVALID_DELAY;
        }
        else
        {
            retStatus = DRV_TIMER_STATUS_OK;
        }
    }
    else if (delayUnit_argen == DRV_DELAY_UNITS_MICROSECOND)
    {
        if (delayValue_argu32 > DRV_TIMER_MAX_DELAY_VALUE_US)
        {
            retStatus = DRV_TIMER_STATUS_INVALID_DELAY;
        }
        else
        {
            retStatus = DRV_TIMER_STATUS_OK;
        }
    }
    else
    {
        retStatus = DRV_TIMER_STATUS_ERR;
    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_ConvertToMicroseconds_prv
 *  Description   : Converts a delay value from its declared unit to
 *                  microseconds for LPIT channel configuration.
 *  Parameters    : delayUnit_argen   - Source time unit
 *                  delayValue_argu32 - Delay magnitude in source unit
 *  Return Value  : U32 - Equivalent period in microseconds
 *
 *  Pre-condition : Caller must have validated delayUnit_argen and
 *                  delayValue_argu32 prior to calling this function.
 * --------------------------------------------------------------------------- */
static U32 DRV_Timer_ConvertToMicroseconds_prv(
    DRV_TimerDelayUnit_ten delayUnit_argen,
    U32                    delayValue_argu32)
{
    U32 periodUs_u32;

    if (delayUnit_argen == DRV_DELAY_UNITS_MILLISECOND)
    {
        periodUs_u32 = delayValue_argu32 * 1000U;
    }
    else
    {
        periodUs_u32 = delayValue_argu32;
    }

    return periodUs_u32;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_AutoInit_prv
 *  Description   : Initializes a timer instance if it has not yet been
 *                  initialized. Used internally by Delay and InterruptConfig.
 *  Parameters    : timerInstance_argen - Instance to auto-initialize
 *  Return Value  : DRV_TIMER_STATUS_OK  - Already initialized or init succeeded
 *                  Other               - Init failure propagated from Init_gst
 * --------------------------------------------------------------------------- */
static DRV_TimerStatus_ten DRV_Timer_AutoInit_prv(
    DRV_TimerInstance_ten timerInstance_argen)
{
    DRV_TimerStatus_ten retStatus;
    if (DRV_timerInitStatus_mb[timerInstance_argen] == (BIN)0)
    {
        retStatus = DRV_Timer_Init_gst(timerInstance_argen);
    }
    else
    {
        retStatus = DRV_TIMER_STATUS_OK;
    }

    return retStatus;
}

/* ==================== PUBLIC FUNCTIONS ==================== */

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_Init_gst
 *  Description   : Initializes an LPIT timer module instance.
 *  Parameters    : timerInstance_argen - Timer instance to initialize
 *  Return Value  : DRV_TIMER_STATUS_OK                  - Success
 *                  DRV_TIMER_STATUS_INVALID_INSTANCE      - Index out of range
 *                  DRV_TIMER_STATUS_ALREADY_INITIALIZED   - Already initialized
 * --------------------------------------------------------------------------- */
DRV_TimerStatus_ten DRV_Timer_Init_gst(DRV_TimerInstance_ten timerInstance_argen)
{
    DRV_TimerStatus_ten retStatus;

    retStatus = DRV_Timer_ValidateInstance_prv(timerInstance_argen);

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        if (DRV_timerInitStatus_mb[timerInstance_argen] != (BIN)0)
        {
            retStatus = DRV_TIMER_STATUS_ALREADY_INITIALIZED;
        }
        else
        {
            LPIT_DRV_Init(
                (U32)timerInstance_argen,
                DRV_timerConfig_mst[timerInstance_argen]);

            DRV_timerInitStatus_mb[timerInstance_argen] = (BIN)1;
            retStatus = DRV_TIMER_STATUS_OK;
        }
    }
    else
    {

    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_ChannelInit_gst
 *  Description   : Initializes a single LPIT channel and installs its IRQ
 *                  handler. The parent timer instance must be initialized first.
 *  Parameters    : timerInstance_argen - Timer instance index
 *                  timerChannel_argen  - Channel index within that instance
 *  Return Value  : DRV_TIMER_STATUS_OK                  - Success
 *                  DRV_TIMER_STATUS_INVALID_INSTANCE      - Instance out of range
 *                  DRV_TIMER_STATUS_INVALID_CHANNEL       - Channel out of range
 *                  DRV_TIMER_STATUS_NOT_INITIALIZED       - Instance not initialized
 *                  DRV_TIMER_STATUS_ALREADY_INITIALIZED   - Channel already initialized
 * --------------------------------------------------------------------------- */
DRV_TimerStatus_ten DRV_Timer_ChannelInit_gst(
    DRV_TimerInstance_ten timerInstance_argen,
    DRV_TimerChannel_ten  timerChannel_argen)
{
    DRV_TimerStatus_ten retStatus;

    retStatus = DRV_Timer_ValidateInstanceAndChannel_prv(
        timerInstance_argen, timerChannel_argen);

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        if (DRV_timerInitStatus_mb[timerInstance_argen] == (BIN)0)
        {
            retStatus = DRV_TIMER_STATUS_NOT_INITIALIZED;
        }
        else if (DRV_timerChannelInitStatus_mb[timerInstance_argen][timerChannel_argen] != (BIN)0)
        {
            retStatus = DRV_TIMER_STATUS_ALREADY_INITIALIZED;
        }
        else
        {
            /* Initialize timer channel */
            LPIT_DRV_InitChannel(
                (U32)timerInstance_argen,
                (U32)timerChannel_argen,
                DRV_timerChannelConfig_mst[timerChannel_argen]);

            /* Install IRQ handler */
            INT_SYS_InstallHandler(
                DRV_timerIRQnArray_mst[timerChannel_argen],
                DRV_timerIRQHandlerArray_mst[timerChannel_argen],
                (void*)0);
            DRV_timerChannelInitStatus_mb[timerInstance_argen][timerChannel_argen] = (BIN)1;
            retStatus = DRV_TIMER_STATUS_OK;
        }
    }
    else
    {

    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_StartChannel_gst
 *  Description   : Starts a previously initialized LPIT timer channel.
 *  Parameters    : timerInstance_argen - Timer instance index
 *                  timerChannel_argen  - Channel index
 *  Return Value  : DRV_TIMER_STATUS_OK               - Channel started
 *                  DRV_TIMER_STATUS_INVALID_INSTANCE  - Instance out of range
 *                  DRV_TIMER_STATUS_INVALID_CHANNEL   - Channel out of range
 *                  DRV_TIMER_STATUS_NOT_INITIALIZED   - Instance or channel not init
 * --------------------------------------------------------------------------- */
DRV_TimerStatus_ten DRV_Timer_StartChannel_gst(
    DRV_TimerInstance_ten timerInstance_argen,
    DRV_TimerChannel_ten  timerChannel_argen)
{
    DRV_TimerStatus_ten retStatus;

    retStatus = DRV_Timer_ValidateInstanceAndChannel_prv(
        timerInstance_argen, timerChannel_argen);

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        if (DRV_timerInitStatus_mb[timerInstance_argen] == (BIN)0)
        {
            retStatus = DRV_TIMER_STATUS_NOT_INITIALIZED;
        }
        else if (DRV_timerChannelInitStatus_mb[timerInstance_argen][timerChannel_argen] == (BIN)0)
        {
            retStatus = DRV_TIMER_STATUS_NOT_INITIALIZED;
        }
        else
        {
            LPIT_DRV_StartTimerChannels(
                (U32)timerInstance_argen,
                DRV_TIMER_CHANNEL_MASK(timerChannel_argen));

            retStatus = DRV_TIMER_STATUS_OK;
        }
    }
    else
    {

    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_StopChannel_gst
 *  Description   : Stops a running LPIT timer channel.
 *  Parameters    : timerInstance_argen - Timer instance index
 *                  timerChannel_argen  - Channel index
 *  Return Value  : DRV_TIMER_STATUS_OK               - Channel stopped
 *                  DRV_TIMER_STATUS_INVALID_INSTANCE  - Instance out of range
 *                  DRV_TIMER_STATUS_INVALID_CHANNEL   - Channel out of range
 *                  DRV_TIMER_STATUS_NOT_INITIALIZED   - Instance or channel not init
 * --------------------------------------------------------------------------- */
DRV_TimerStatus_ten DRV_Timer_StopChannel_gst(
    DRV_TimerInstance_ten timerInstance_argen,
    DRV_TimerChannel_ten  timerChannel_argen)
{
    DRV_TimerStatus_ten retStatus;

    retStatus = DRV_Timer_ValidateInstanceAndChannel_prv(
        timerInstance_argen, timerChannel_argen);

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        if (DRV_timerInitStatus_mb[timerInstance_argen] == (BIN)0)
        {
            retStatus = DRV_TIMER_STATUS_NOT_INITIALIZED;
        }
        else if (DRV_timerChannelInitStatus_mb[timerInstance_argen][timerChannel_argen] == (BIN)0)
        {
            retStatus = DRV_TIMER_STATUS_NOT_INITIALIZED;
        }
        else
        {
            LPIT_DRV_StopTimerChannels(
                (U32)timerInstance_argen,
                DRV_TIMER_CHANNEL_MASK(timerChannel_argen));

            retStatus = DRV_TIMER_STATUS_OK;
        }
    }
    else
    {

    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_Delay_gst
 *  Description   : Creates a precise blocking delay using an LPIT channel.
 *                  Auto-initializes the timer instance if required.
 *                  Busy-waits on the channel interrupt flag; no NVIC interrupt
 *                  is enabled during this operation.
 *  Parameters    : timerInstance_argen - Timer instance index
 *                  timerChannel_argen  - Channel index
 *                  delayUnit_argen     - Time unit (ms or us)
 *                  delayValue_argu32   - Delay magnitude
 *  Return Value  : DRV_TIMER_STATUS_OK               - Delay completed
 *                  DRV_TIMER_STATUS_INVALID_INSTANCE  - Instance out of range
 *                  DRV_TIMER_STATUS_INVALID_CHANNEL   - Channel out of range
 *                  DRV_TIMER_STATUS_ZERO_DELAY        - delayValue_argu32 == 0
 *                  DRV_TIMER_STATUS_INVALID_DELAY     - Exceeds maximum
 *                  DRV_TIMER_STATUS_ERR               - Unknown delay unit
 *                  Other                              - Propagated from helpers
 * --------------------------------------------------------------------------- */
DRV_TimerStatus_ten DRV_Timer_Delay_gst(
    DRV_TimerInstance_ten  timerInstance_argen,
    DRV_TimerChannel_ten   timerChannel_argen,
    DRV_TimerDelayUnit_ten delayUnit_argen,
    U32                    delayValue_argu32)
{
    DRV_TimerStatus_ten        retStatus;
    lpit_user_channel_config_t channelConfig;
    U32                        periodUs_u32;
    retStatus = DRV_Timer_ValidateInstanceAndChannel_prv(
        timerInstance_argen, timerChannel_argen);

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        retStatus = DRV_Timer_ValidateDelayParams_prv(
            delayUnit_argen, delayValue_argu32);
    }
    else
    {

    }

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        retStatus = DRV_Timer_AutoInit_prv(timerInstance_argen);
    }
    else
    {

    }

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        periodUs_u32 = DRV_Timer_ConvertToMicroseconds_prv(
            delayUnit_argen, delayValue_argu32);

        LPIT_DRV_GetDefaultChanConfig(&channelConfig);
        channelConfig.isInterruptEnabled = false;
        channelConfig.timerMode          = LPIT_PERIODIC_COUNTER;
        channelConfig.periodUnits        = LPIT_PERIOD_UNITS_MICROSECONDS;
        channelConfig.period             = periodUs_u32;

        LPIT_DRV_InitChannel(
            (U32)timerInstance_argen,
            (U32)timerChannel_argen,
            &channelConfig);
        DRV_timerChannelInitStatus_mb[timerInstance_argen][timerChannel_argen] = (BIN)1;
        LPIT_DRV_ClearInterruptFlagTimerChannels(
            (U32)timerInstance_argen,
            DRV_TIMER_CHANNEL_MASK(timerChannel_argen));
        retStatus = DRV_Timer_StartChannel_gst(timerInstance_argen, timerChannel_argen);
    }
    else
    {

    }

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        while (LPIT_DRV_GetInterruptFlagTimerChannels(
                   (U32)timerInstance_argen,
                   DRV_TIMER_CHANNEL_MASK(timerChannel_argen)) == 0U)
        {

        }
        LPIT_DRV_ClearInterruptFlagTimerChannels(
            (U32)timerInstance_argen,
            DRV_TIMER_CHANNEL_MASK(timerChannel_argen));
        (void)DRV_Timer_StopChannel_gst(timerInstance_argen, timerChannel_argen);

        retStatus = DRV_TIMER_STATUS_OK;
    }
    else
    {

    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_DeInit_gst
 *  Description   : Deinitializes an LPIT timer instance and resets the
 *                  initialization status of all its channels.
 *  Parameters    : timerInstance_argen - Timer instance to deinitialize
 *  Return Value  : DRV_TIMER_STATUS_OK               - Success
 *                  DRV_TIMER_STATUS_INVALID_INSTANCE  - Index out of range
 *                  DRV_TIMER_STATUS_NOT_INITIALIZED   - Was not initialized
 * --------------------------------------------------------------------------- */
DRV_TimerStatus_ten DRV_Timer_DeInit_gst(DRV_TimerInstance_ten timerInstance_argen)
{
    DRV_TimerStatus_ten  retStatus;
    DRV_TimerChannel_ten channel_en;

    retStatus = DRV_Timer_ValidateInstance_prv(timerInstance_argen);

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        if (DRV_timerInitStatus_mb[timerInstance_argen] == (BIN)0)
        {
            retStatus = DRV_TIMER_STATUS_NOT_INITIALIZED;
        }
        else
        {
            LPIT_DRV_Deinit((U32)timerInstance_argen);
            for (channel_en = DRV_CHANNEL_0;
                 channel_en < DRV_MAX_CHANNEL;
                 channel_en++)
            {
                DRV_timerChannelInitStatus_mb[timerInstance_argen][channel_en] = (BIN)0;
            }
            DRV_timerInitStatus_mb[timerInstance_argen] = (BIN)0;

            retStatus = DRV_TIMER_STATUS_OK;
        }
    }
    else
    {

    }

    return retStatus;
}

/* -----------------------------------------------------------------------------
 *  Function Name : DRV_Timer_InterruptConfig_gst
 *  Description   : Configures an LPIT channel for periodic interrupt-driven
 *                  operation. Auto-initializes the timer instance if needed.
 *  Parameters    : timerInstance_argen      - Timer instance index
 *                  timerChannel_argen       - Channel index
 *                  irqIndex_argu8           - NVIC IRQ index
 *                  priority_argu8           - NVIC interrupt priority
 *                  interruptDelayMs_argu32  - Period magnitude
 *                  delayUnit_argen          - Period time unit (ms or us)
 *  Return Value  : DRV_TIMER_STATUS_OK               - Configured successfully
 *                  DRV_TIMER_STATUS_INVALID_INSTANCE  - Instance out of range
 *                  DRV_TIMER_STATUS_INVALID_CHANNEL   - Channel out of range
 *                  DRV_TIMER_STATUS_ZERO_DELAY        - Period is zero
 *                  DRV_TIMER_STATUS_INVALID_DELAY     - Exceeds maximum
 *                  DRV_TIMER_STATUS_ERR               - Unknown delay unit
 *                  DRV_TIMER_STATUS_NVIC_ERROR        - NVIC config failed
 *                  Other                              - Propagated from helpers
 * --------------------------------------------------------------------------- */
DRV_TimerStatus_ten DRV_Timer_InterruptConfig_gst(
    DRV_TimerInstance_ten  timerInstance_argen,
    DRV_TimerChannel_ten   timerChannel_argen,
    U8                     irqIndex_argu8,
    U8                     priority_argu8,
    U32                    interruptDelayMs_argu32,
    DRV_TimerDelayUnit_ten delayUnit_argen)
{
    DRV_TimerStatus_ten        retStatus;
    lpit_user_channel_config_t channelConfig;
    DRV_NVIC_Status_ten        nvicStatus;
    U32                        periodUs_u32;
    retStatus = DRV_Timer_ValidateInstanceAndChannel_prv(
        timerInstance_argen, timerChannel_argen);

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        retStatus = DRV_Timer_ValidateDelayParams_prv(
            delayUnit_argen, interruptDelayMs_argu32);
    }
    else
    {

    }

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        retStatus = DRV_Timer_AutoInit_prv(timerInstance_argen);
    }
    else
    {

    }

    if (retStatus == DRV_TIMER_STATUS_OK)
    {
        periodUs_u32 = DRV_Timer_ConvertToMicroseconds_prv(
            delayUnit_argen, interruptDelayMs_argu32);

        nvicStatus = DRV_NVIC_IRQConfig_gen(irqIndex_argu8, priority_argu8);

        if (nvicStatus != DRV_NVIC_STATUS_OK)
        {
            retStatus = DRV_TIMER_STATUS_NVIC_ERROR;
        }
        else
        {
            LPIT_DRV_GetDefaultChanConfig(&channelConfig);

            channelConfig.isInterruptEnabled = true;
            channelConfig.timerMode          = LPIT_PERIODIC_COUNTER;
            channelConfig.periodUnits        = LPIT_PERIOD_UNITS_MICROSECONDS;
            channelConfig.period             = periodUs_u32;

            /* Initialize the channel */
            LPIT_DRV_InitChannel(
                (U32)timerInstance_argen,
                (U32)timerChannel_argen,
                &channelConfig);

            /* Mark channel as initialized */
            DRV_timerChannelInitStatus_mb[timerInstance_argen][timerChannel_argen] = (BIN)1;

            retStatus = DRV_TIMER_STATUS_OK;
        }
    }
    else
    {

    }

    return retStatus;
}
void APP_Timer_Ch0_IRQHandler()
{

}
void APP_Timer_Ch1_IRQHandler()
{

}
