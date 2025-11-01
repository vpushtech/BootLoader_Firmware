#ifndef DRIVERS_DRV_CAN_H_
#define DRIVERS_DRV_CAN_H_

/*******************************************************************************
 *  HEADER FILE INCLUDES
 ******************************************************************************/
#include "common_types.h"
#include "device_registers.h"
#include <string.h>
#include "drv_nvic.h"

/* ==================== TYPE DEFINITIONS ==================== */
typedef enum {
    DRV_CAN_STATUS_SUCCESS,
    DRV_CAN_STATUS_ERROR,
} DRV_CanStatus_En;

typedef enum {
    DRV_CAN_ID_MODE_STANDARD,
    DRV_CAN_ID_MODE_EXTENDED
} DRV_CanIdMode_En;

typedef enum {
    DRV_CAN_FRAME_TYPE_DATA,
    DRV_CAN_FRAME_TYPE_REMOTE
} DRV_CanFrameType_En;

typedef enum {
    DRV_CAN_FD_DISABLED = 0,
    DRV_CAN_FD_ENABLED
} DRV_CanFdMode_En;

typedef enum {
    DRV_CAN_BRS_DISABLED = 0,
    DRV_CAN_BRS_ENABLED
} DRV_CanBitRateSwitch_En;

typedef enum {
    DRV_CAN_INSTANCE_1 = 0,
    DRV_CAN_INSTANCE_2,
    DRV_CAN_INSTANCE_3,
    DRV_CAN_MAX_INSTANCE
} DRV_CanInstance_En;

typedef struct {
    U32 DRV_CanId_u32;
    DRV_CanIdMode_En DRV_IdMode_En;
    DRV_CanFrameType_En DRV_FrameType_En;
    U8 DRV_DataLength_u8;
    U8 DRV_Data_arru8[8];
} DRV_CanFrame_St_t;

#define CAN_TIMEOUT				3000

/* ==================== INITIALIZATION/CONFIGURATION ==================== */
DRV_CanStatus_En DRV_CAN_Init_gen(DRV_CanInstance_En instance_arg);
DRV_CanStatus_En DRV_CAN_Deinit_gen(DRV_CanInstance_En instance_enarg);

/* ==================== TRANSMIT CONFIGURATION & OPERATIONS ==================== */
DRV_CanStatus_En DRV_CAN_ConfigTxBuffer_gen(DRV_CanInstance_En instance_arg,
                                  U8 bufferIdx_argu8,
                                  const DRV_CanFrame_St_t* frameConfig_argst,
                                  U32 canId_u32);
DRV_CanStatus_En DRV_CAN_Transmit_gen(DRV_CanInstance_En instance_enarg,
                            U8 bufferIdx_argu8,
                            const DRV_CanFrame_St_t* frame_argst);

/* ==================== RECEIVE CONFIGURATION & OPERATIONS ==================== */
DRV_CanStatus_En DRV_CAN_ConfigRxBuffer_gen(DRV_CanInstance_En instance_arg,
                                  U8 bufferIdx_argu8,
                                  const DRV_CanFrame_St_t* frameConfig_argst,
                                  U32 RxmsgId_argu32);
DRV_CanStatus_En DRV_CAN_Receive_gen(DRV_CanInstance_En instance_enarg,
                           U8 bufferIdx_argu8,
                           DRV_CanFrame_St_t* frame_argst);
DRV_CanStatus_En DRV_CAN_ReceiveBlocking_gen(DRV_CanInstance_En instance_enarg,
                           U8 bufferIdx_argu8,
                           DRV_CanFrame_St_t* frame_argst);
DRV_CanStatus_En DRV_CAN_SetRxFilter_gen(DRV_CanInstance_En instance_arg,
                               U8 bufferIdx_argu8,
                               U32 mask_u32,
                               DRV_CanIdMode_En idMode_enarg);
/* ==================== CAN Callback ==================== */

#endif /* DRIVERS_DRV_CAN_H_ */
