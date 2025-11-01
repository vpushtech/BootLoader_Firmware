#ifndef DRIVERS_DRV_TIMER_H_
#define DRIVERS_DRV_TIMER_H_

/* ==================== HEADER INCLUDES ==================== */
#include "lpit_driver.h"
#include "common_types.h"
#include "lpit1.h"
#include "drv_nvic.h"

/* ==================== DEFINES ==================== */
#define DRV_TIMER_CHANNEL_MASK(channel) (1U << (channel))

/* ==================== TYPE DEFINITIONS ==================== */
typedef enum {
    DRV_TIMER0,
    DRV_TIMER1,
    DRV_TIMER2,
    DRV_TIMER3,
    DRV_MAX_TIMER
} DRV_TimerInstance_En;

typedef enum {
    DRV_CHANNEL_0,
    DRV_CHANNEL_1,
    DRV_CHANNEL_2,
    DRV_CHANNEL_3,
    DRV_MAX_CHANNEL
} DRV_TimerChannel_En;

typedef enum {
    DRV_DELAY_UNITS_MILLISECOND,
    DRV_DELAY_UNITS_MICROSECOND
} DRV_TimerDelayUnit_En;
typedef enum {
    DRV_TIMER_STATUS_OK,
	DRV_TIMER_STATUS_ERR
} DRV_TimerStatus_En;

typedef struct {
    DRV_TimerInstance_En timerInstance;
    DRV_TimerChannel_En timerChannel;
} DRV_TimerConfig_St_t;

/* ==================== TIMER CONTROL FUNCTIONS ==================== */
void DRV_Timer_Init_gv(U8 timerPinIdx);
void DRV_Timer_Start_gv(U8 timerPinIdx);
void DRV_Timer_Stop_gv(U8 timerPinIdx);
void DRV_Timer_DeInit_gv(U8 timerPinIdx);

/* ==================== TIMING OPERATIONS ==================== */
void DRV_Timer_Delay_gv(U8 timerPinIdx, DRV_TimerDelayUnit_En delayUnit, U32 delayValue);
U32 DRV_Timer_GetCountMicrosecond_gu32(U8 timerPinIdx);

/* ==================== INTERRUPT CONFIGURATION ==================== */
DRV_TimerStatus_En DRV_Timer_InterruptConfig_gst(U8 timerPinIdx, U8 irqIndex, U8 priority, U32 interruptDelayMs, DRV_TimerDelayUnit_En delayUnit_argen);
/* ==================== PWM FUNCTIONS ==================== */
DRV_TimerStatus_En DRV_Timer_PWM_Init_gst(U8 timerPinIdx);
DRV_TimerStatus_En DRV_Timer_UpdateDutyCycle_gst(U8 timerPinIdx, U8 dutyCycle);
DRV_TimerStatus_En DRV_Timer_UpdateLedStatus_gst(U8 timerPinIdx);

#endif /* DRIVERS_DRV_TIMER_H_ */
