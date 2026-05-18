/*
 * drv_can.h
 *
 *  Description     : CAN Driver Header
 *  Author          : Rushikesh
 *  Created On      : 13-Jul-2025
 *  Version         : 2.0
 *  Modification History:
 *  Date        Author      Description
 *  ----------------------------------------------------------------------------
 *  13-Jul-2025 RUSHIKESH   CAN Driver Architecture Implementation
 *  28-Jul-2025 RUSHIKESH   CAN Testing Completed with PEAK CAN Device
 *  12-Aug-2025 RUSHIKESH   Guidelines Followed the naming Architecture Implementation
 *  29-Jan-2026 RUSHIKESH   Added validation, init tracking, and error handling
 *  01-May-2026 RUSHIKESH   MISRA C 2012 compliance
 ******************************************************************************/

#ifndef DRIVERS_DRV_CAN_H_
#define DRIVERS_DRV_CAN_H_

#include "common_types.h"
#include "device_registers.h"
#include "drv_nvic.h"

/* ==================== CONSTANT DEFINITIONS ==================== */
#define DRV_CAN_TIMEOUT           (1000U)
#define DRV_CAN_MAX_BUFFERS       (32U)
#define DRV_CAN_MAX_DATA_LENGTH   (8U)
#define DRV_CAN_STD_ID_MASK       (0x7FFU)
#define DRV_CAN_EXT_ID_MASK       (0x1FFFFFFFU)
#define DRV_CAN_IRQ_HIGH_BUF_THR  (15U)
#define DRV_CAN_CS_IDE_BIT        (21U)
#define DRV_CAN_CS_RTR_BIT        (20U)

/* ==================== ENUM DEFINITIONS ==================== */
typedef enum
{
    DRV_CAN_STATUS_SUCCESS           =  0,
    DRV_CAN_STATUS_ERROR             =  1,
    DRV_CAN_STATUS_INVALID_INSTANCE  =  2,
    DRV_CAN_STATUS_INVALID_BUFFER_INDEX = 3,
    DRV_CAN_STATUS_INVALID_ID        =  4,
    DRV_CAN_STATUS_INVALID_DATA_LENGTH = 5,
    DRV_CAN_STATUS_NULL_POINTER      =  6,
    DRV_CAN_INTERRUPT_NOT_ENABLED    =  7,
    DRV_CAN_STATUS_NOT_INITIALIZED   =  8,
    DRV_CAN_STATUS_BUFFER_OVERFLOW   =  9,
    DRV_CAN_STATUS_RECEIVE_FAILED    = 10,
    DRV_CAN_STATUS_TRANSMIT_FAILED   = 11
} DRV_CanStatus_ten;

typedef enum
{
    DRV_CAN_ID_MODE_STANDARD = 0,
    DRV_CAN_ID_MODE_EXTENDED = 1
} DRV_CanIdMode_ten;

typedef enum
{
    DRV_CAN_FRAME_TYPE_DATA   = 0,
    DRV_CAN_FRAME_TYPE_REMOTE = 1
} DRV_CanFrameType_ten;
typedef enum
{
    DRV_CAN_FD_DISABLED = 0,
    DRV_CAN_FD_ENABLED  = 1
} DRV_CanFdMode_ten;
typedef enum
{
    DRV_CAN_BRS_DISABLED = 0,
    DRV_CAN_BRS_ENABLED  = 1
} DRV_CanBitRateSwitch_ten;

typedef enum
{
    DRV_CAN_INSTANCE_0   = 0,
    DRV_CAN_MAX_INSTANCE = 1
} DRV_CanInstance_ten;

typedef enum
{
    DRV_CAN_PN_WAKEUP_NONE    = 0U,
    DRV_CAN_PN_WAKEUP_TIMEOUT = (1U << 0U),
    DRV_CAN_PN_WAKEUP_MATCH   = (1U << 1U)
} DRV_CanPnWakeup_ten;

/* ==================== STRUCT DEFINITIONS ==================== */
typedef struct
{
    U32                  DRV_CanId_u32;
    DRV_CanIdMode_ten    DRV_IdMode_en;
    DRV_CanFrameType_ten DRV_FrameType_en;
    U8                   DRV_DataLength_u8;
    U8                   DRV_Data_arru8[DRV_CAN_MAX_DATA_LENGTH];
} DRV_CanFrame_tst;
typedef struct
{
    bool               enableTimeoutWakeup;
    bool               enableMatchWakeup;
    U32                timeoutValue;
    U32                matchId;
    DRV_CanIdMode_ten  idMode;
} DRV_CanPnConfig_tst;

/* ==================== GLOBAL VARIABLES ==================== */
extern volatile bool DRV_canInitStatus_b[DRV_CAN_MAX_INSTANCE];

/* ==================== FUNCTION PROTOTYPES ==================== */
/* Initialization / De-initialization */
extern DRV_CanStatus_ten DRV_CAN_Init_gen(DRV_CanInstance_ten instance_argen);
extern DRV_CanStatus_ten DRV_CAN_Deinit_gen(DRV_CanInstance_ten instance_argen);

/* Transmit configuration and operations */
extern DRV_CanStatus_ten DRV_CAN_ConfigTxBuffer_gen(DRV_CanInstance_ten      instance_argen,
                                                     U8                       bufferIdx_argu8,
                                                     const DRV_CanFrame_tst  *frameConfig_argst,
                                                     U32                      canId_argu32);

extern DRV_CanStatus_ten DRV_CAN_TransmitBlock_gen(DRV_CanInstance_ten     instance_argen,
                                                   U8                      bufferIdx_argu8,
                                                   const DRV_CanFrame_tst *frame_argst);

extern DRV_CanStatus_ten DRV_CAN_TransmitNonBlock_gen(DRV_CanInstance_ten     instance_argen,
                                                      U8                      bufferIdx_argu8,
                                                      const DRV_CanFrame_tst *frame_argst);

/* Receive configuration and operations */
extern DRV_CanStatus_ten DRV_CAN_ConfigRxBuffer_gen(DRV_CanInstance_ten     instance_argen,
                                                    U8                      bufferIdx_argu8,
                                                    const DRV_CanFrame_tst *frameConfig_argst,
                                                    U32                     rxMsgId_argu32);

extern DRV_CanStatus_ten DRV_CAN_ReceiveBlock_gen(DRV_CanInstance_ten  instance_argen,
                                                  U8                   bufferIdx_argu8,
                                                  DRV_CanFrame_tst    *frame_argst);

extern DRV_CanStatus_ten DRV_CAN_ReceiveNonBlock_gen(DRV_CanInstance_ten  instance_argen,
                                                     U8                   bufferIdx_argu8,
                                                     DRV_CanFrame_tst    *frame_argst);

extern DRV_CanStatus_ten DRV_CAN_SetRxFilter_gen(DRV_CanInstance_ten instance_argen,
                                                 U8                  bufferIdx_argu8,
                                                 U32                 mask_argu32,
                                                 DRV_CanIdMode_ten   idMode_argen);

/* Pretended Networking (low-power wakeup) */
extern DRV_CanStatus_ten DRV_CAN_EnablePretendedNetworking_gen(
                                  DRV_CanInstance_ten       instance_argen,
                                  const DRV_CanPnConfig_tst *pnConfig_argst);

extern DRV_CanStatus_ten DRV_CAN_DisablePretendedNetworking_gen(
                                  DRV_CanInstance_ten instance_argen);

extern DRV_CanPnWakeup_ten DRV_CAN_GetPnWakeupReason_gen(
                                  DRV_CanInstance_ten instance_argen);

#endif /* DRIVERS_DRV_CAN_H_ */
